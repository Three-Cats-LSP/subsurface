// SPDX-License-Identifier: GPL-2.0
#include "cloudsyncworkspacearchive.h"
#include "cloudsyncmanifest.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QLockFile>
#include <QSaveFile>
#include <QUuid>

namespace {
QString text(const QJsonObject &o, const char *key) { return o.value(QLatin1String(key)).toString(); }
bool revision(const QString &v) { return !v.isEmpty() && v.size() <= 512 && !v.contains(QChar::Null) && !v.contains('\r') && !v.contains('\n'); }
const QStringList States = {"prepared", "submitted", "identified", "verified", "publishing", "completed"};
}
CloudWorkspaceArchive::CloudWorkspaceArchive(QNetworkAccessManager *network, QObject *parent) : QObject(parent), files(network, this), reader(network, this)
{
	files.setStrictReads(true); files.setResponseLimit(CloudWorkspaceRecord::WireLimit);
	deadline.setSingleShot(true); deadline.setInterval(600000);
	connect(&deadline, &QTimer::timeout, this, [this] { fail(); });
	connect(&reader, &CloudWorkspaceOpen::finished, this, &CloudWorkspaceArchive::receivedRoot);
	connect(&reader, &CloudWorkspaceOpen::failed, this, [this](const QString &, bool rejected) { fail(rejected); });
	connect(&files, &CloudSyncFileStore::workspaceArchiveCreated, this, [this](CloudSyncProviderType type, const QString &name, const QString &id) {
		if (step != Step::Create) return;
		// Keep a directly acknowledged old-scope ID before checking the guard.
		if (type != provider || name != text(scope, "name") || !advance("submitted", "identified", id)) { fail(); return; }
		readArchive();
	});
	connect(&files, &CloudSyncFileStore::versionedDownloadFinished, this, &CloudWorkspaceArchive::receivedArchive);
	connect(&files, &CloudSyncFileStore::uploadFinished, this, [this](CloudSyncProviderType, const QString &) { if (step == Step::Publish) readRoot(); });
	connect(&files, &CloudSyncFileStore::writeConflict, this, [this](CloudSyncProviderType, const QString &) { if (step == Step::Publish) readRoot(); else fail(); });
	connect(&files, &CloudSyncFileStore::operationError, this, [this](CloudSyncProviderType, const QString &, const QString &, bool rejected) {
		if (step == Step::Publish && !rejected) readRoot(); else fail(rejected);
	});
	connect(&files, &CloudSyncFileStore::fileNotFound, this, [this](CloudSyncProviderType, const QString &) { fail(); });
}
CloudWorkspaceArchive::~CloudWorkspaceArchive() { cancel(); }
void CloudWorkspaceArchive::cancel()
{
	step = Step::Idle; deadline.stop(); files.cancel(provider); reader.cancel(); operationLock.reset();
	base.value = {}; pending.value = {}; reference.value = {}; scope = {}; saved = {}; scopeCurrent = {};
	token.clear(); baseVersion.clear(); choiceFile.clear(); choiceVersion.clear(); checkpointFile.clear(); intentFile.clear();
}
void CloudWorkspaceArchive::fail(bool rejected)
{
	if (step == Step::Idle) return;
	cancel(); emit failed(tr("Cloud history rollover could not be confirmed. All versions and attempts were kept; reload the workspace before retrying."), rejected);
}
bool CloudWorkspaceArchive::current()
{
	if (step == Step::Idle) return false;
	CloudWorkspaceSelection choice(choiceFile, text(reference.value, "provider"), text(reference.value, "account"));
	if (!scopeCurrent || !scopeCurrent() || !choice.load() || choice.version() != choiceVersion || choice.selected().value != reference.value) { fail(); return false; }
	return true;
}
bool CloudWorkspaceArchive::load()
{
	saved = {}; const QFileInfo info(intentFile);
	if (info.isSymLink() || !info.isFile() || info.size() > 16384) return false;
	QFile file(intentFile); if (!file.open(QIODevice::ReadOnly)) return false;
	const auto bytes = file.read(16385); if (bytes.size() > 16384 || file.error() != QFileDevice::NoError) return false;
	const auto body = QJsonDocument::fromJson(bytes).object(); auto fixed = body;
	for (const auto &key : {"state", "object", "version"}) { if (!body.value(QLatin1String(key)).isString()) return false; fixed.remove(QLatin1String(key)); }
	if (fixed != scope) return false;
	const int phase = States.indexOf(text(body, "state")); const auto id = text(body, "object"), version = text(body, "version");
	if (phase < 0 || (phase < 2 && !id.isEmpty()) || (phase < 3 && !version.isEmpty()) ||
	    (phase >= 2 && base.archivePin(text(scope, "nonce"), id, phase >= 3 ? version : "unverified").isEmpty()) ||
	    (phase >= 3 && !revision(version))) return false;
	saved = body; return true;
}
bool CloudWorkspaceArchive::save(const QJsonObject &next)
{
	const auto bytes = QJsonDocument(next).toJson(QJsonDocument::Compact); QSaveFile file(intentFile);
	return bytes.size() <= 16384 && !QFileInfo(intentFile).isSymLink() && file.open(QIODevice::WriteOnly) &&
		file.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner) && file.write(bytes) == bytes.size() && file.commit() && load() && saved == next;
}
bool CloudWorkspaceArchive::prepare()
{
	const QFileInfo info(intentFile), parent(info.absolutePath());
	if (info.isSymLink() || parent.isSymLink() || !QDir().mkpath(parent.filePath())) return false;
	operationLock = std::make_unique<QLockFile>(intentFile + ".operation.lock"); operationLock->setStaleLockTime(0);
	if (!operationLock->tryLock(0)) return false;
	QLockFile lock(intentFile + ".lock"); if (!lock.tryLock(0)) return false;
	QString nonce;
	if (QFileInfo::exists(intentFile)) {
		const QFileInfo info(intentFile); if (info.isSymLink() || !info.isFile() || info.size() > 16384) return false;
		QFile file(intentFile); if (!file.open(QIODevice::ReadOnly)) return false;
		nonce = text(QJsonDocument::fromJson(file.read(16385)).object(), "nonce");
	} else nonce = QUuid::createUuid().toString(QUuid::WithoutBraces);
	if (!CloudHistoryRecord::validEpoch(nonce)) return false;
	scope = {{"schema", 1}, {"format", "neo-workspace-archive-intent-development-1"}, {"workspace", reference.value},
		{"nonce", nonce}, {"name", CloudWorkspaceRecord::archiveName(reference, nonce)},
		{"root_sha256", CloudSyncManifest::sha256(base.json())}, {"root_version", baseVersion}};
	if (QFileInfo::exists(intentFile)) return load();
	auto next = scope; next["state"] = "prepared"; next["object"] = ""; next["version"] = ""; return save(next);
}
bool CloudWorkspaceArchive::advance(const QString &from, const QString &to, const QString &object, const QString &version)
{
	QLockFile lock(intentFile + ".lock"); if (!lock.tryLock(0) || !load()) return false;
	if (text(saved, "state") != from) return false;
	if (to == "identified" && base.archivePin(text(scope, "nonce"), object, "unverified").isEmpty()) return false;
	if (to == "verified" && !revision(version)) return false;
	auto next = saved; next["state"] = to;
	if (!object.isEmpty()) next["object"] = object;
	if (!version.isEmpty()) next["version"] = version;
	return save(next);
}
void CloudWorkspaceArchive::start(const QString &accessToken, const CloudWorkspaceRecord &root, const QString &rootVersion,
	const QString &choice, const QString &choiceVersion, const QString &checkpoint, const QString &intent, const std::function<bool()> &guard)
{
	cancel(); step = Step::Root; token = accessToken; base = root; baseVersion = rootVersion; reference = root.reference();
	choiceFile = choice; this->choiceVersion = choiceVersion; checkpointFile = checkpoint; intentFile = intent; scopeCurrent = guard;
	provider = text(reference.value, "provider") == "google-drive" ? CloudSyncProviderType::GoogleDrive : CloudSyncProviderType::Dropbox;
	if (token.isEmpty() || intentFile.isEmpty() || !base.valid() || !revision(baseVersion) || !current()) { fail(); return; }
	CloudWorkspaceAuthority authority(checkpointFile, reference);
	if (!authority.load() || !authority.record().valid() || !authority.record().follows(base) || !current() || !prepare()) { fail(); return; }
	deadline.start(); const auto state = text(saved, "state");
	if (state == "submitted") { fail(); return; } // Unknown create: never replay or look up its name.
	if (state == "prepared") {
		if (!advance("prepared", "submitted") || !current()) { fail(); return; }
		step = Step::Create; files.setResponseLimit(8192);
		files.createWorkspaceArchiveIdentity(provider, token, text(scope, "name"), base.json());
	} else readArchive();
}
void CloudWorkspaceArchive::readArchive()
{
	if (!current() || !load()) { fail(); return; }
	step = Step::Archive; files.setResponseLimit(CloudWorkspaceRecord::WireLimit);
	if (text(saved, "version").isEmpty()) files.downloadIdentity(provider, token, text(scope, "name"), text(saved, "object"));
	else files.downloadExact(provider, token, text(scope, "name"), text(saved, "object"), text(saved, "version"));
}
void CloudWorkspaceArchive::receivedArchive(CloudSyncProviderType type, const QString &name, const QByteArray &bytes, const QString &version, const QString &id)
{
	if (step != Step::Archive || !current() || !load()) { fail(); return; }
	if (type != provider || name != text(scope, "name") || id != text(saved, "object") || bytes != base.json() || !revision(version)) { fail(); return; }
	if (text(saved, "state") == "identified") { if (!advance("identified", "verified", {}, version)) { fail(); return; } }
	else if (text(saved, "version") != version) { fail(); return; }
	pending = base; if (!pending.archiveTo(base.archivePin(text(scope, "nonce"), id, version))) { fail(); return; }
	readRoot();
}
void CloudWorkspaceArchive::readRoot()
{
	if (!current()) return;
	CloudWorkspaceAuthority authority(checkpointFile, reference);
	if (!authority.load() || !authority.record().valid()) { fail(); return; }
	step = Step::Root;
	reader.start(token, text(reference.value, "provider"), text(reference.value, "account"), reference, scopeCurrent, authority.record());
}
void CloudWorkspaceArchive::receivedRoot(const QJsonObject &value, const QString &version)
{
	if (step != Step::Root || !current() || !load()) { fail(); return; }
	CloudWorkspaceRecord observed; observed.value = value;
	if (!observed.valid() || !observed.follows(base) || !pending.valid()) { fail(); return; }
	const auto state = text(saved, "state");
	if (observed.follows(pending)) {
		CloudWorkspaceAuthority authority(checkpointFile, reference);
		if (!authority.observe(observed, version, true) || !current() ||
		    (state != "completed" && !advance(state, "completed"))) { fail(); return; }
		const auto result = observed.value; cancel(); emit finished(result, version); return;
	}
	// Never rebase an old page over a concurrent change. A fresh caller may
	// retain this orphan and start a new, separately journalled rollover.
	if ((state != "verified" && state != "publishing") || observed.value != base.value || version != baseVersion ||
	    (state == "verified" && !advance("verified", "publishing")) || !current()) { fail(); return; }
	step = Step::Publish;
	files.uploadIdentity(provider, token, reference.rootName(), pending.json(), baseVersion, text(reference.value, "root_id"));
}
