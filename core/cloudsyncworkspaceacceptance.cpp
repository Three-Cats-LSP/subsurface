// SPDX-License-Identifier: GPL-2.0
#include "cloudsyncworkspaceacceptance.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLockFile>
#include <QRegularExpression>
#include <QSaveFile>

namespace {
QString text(const QJsonObject &o, const char *key) { return o.value(QLatin1String(key)).toString(); }
constexpr qint64 IntentLimit = 64 * 1024;
}

CloudWorkspaceAcceptanceIntent::CloudWorkspaceAcceptanceIntent(const QString &path, const CloudWorkspaceReference &workspace,
	const QString &choice, const CloudWorkspaceCopySource &source, const QString &localHash, const QString &event, const QString &previous,
	const QString &head, const QString &copy) : fileName(path)
{
	if (workspace.valid() && source.matches(text(workspace.value, "provider"), text(workspace.value, "account")) &&
	    CloudHistoryRecord::validEpoch(choice) && QRegularExpression("^[a-f0-9]{64}$").match(localHash).hasMatch() && CloudHistoryRecord::validEpoch(event) && CloudHistoryRecord::validEpoch(head) &&
	    CloudHistoryRecord::validEpoch(copy) && (previous.isEmpty() || CloudHistoryRecord::validEpoch(previous)) &&
	    event != previous && event != head && event != copy)
		scope = {{"schema", 1}, {"format", "neo-workspace-legacy-acceptance-development-1"}, {"workspace", workspace.value},
			{"choice_version", choice}, {"source", source.value}, {"local_sha256", localHash}, {"event", event}, {"previous", previous}, {"head", head}, {"copy", copy}};
}
QString CloudWorkspaceAcceptanceIntent::state() const { return text(saved, "state"); }
bool CloudWorkspaceAcceptanceIntent::read()
{
	saved = {};
	if (scope.isEmpty() || fileName.isEmpty()) return false;
	const QFileInfo info(fileName); if (info.isSymLink()) return false;
	if (!info.exists()) return true;
	if (!info.isFile() || info.size() < 1 || info.size() > IntentLimit) return false;
	QFile file(fileName); if (!file.open(QIODevice::ReadOnly)) return false;
	const auto bytes = file.read(IntentLimit + 1); auto body = QJsonDocument::fromJson(bytes).object();
	if (file.error() != QFileDevice::NoError || bytes.size() > IntentLimit) return false;
	const auto phase = text(body, "state"); body.remove("state");
	if (body != scope || !QStringList{"prepared", "submitted", "committed", "completed"}.contains(phase)) return false;
	body["state"] = phase; saved = body; return true;
}
bool CloudWorkspaceAcceptanceIntent::inspect() { return read() && !saved.isEmpty(); }
bool CloudWorkspaceAcceptanceIntent::write(const QJsonObject &body)
{
	const auto bytes = QJsonDocument(body).toJson(QJsonDocument::Compact); if (bytes.size() > IntentLimit) return false;
	QSaveFile file(fileName);
	return file.open(QIODevice::WriteOnly) && file.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner) &&
		file.write(bytes) == bytes.size() && file.commit() && read() && saved == body;
}
bool CloudWorkspaceAcceptanceIntent::prepare()
{
	const QFileInfo parent(QFileInfo(fileName).absolutePath());
	if (scope.isEmpty() || fileName.isEmpty() || parent.isSymLink() || !QDir().mkpath(parent.filePath())) return false;
	QLockFile lock(fileName + ".lock"); if (!lock.tryLock(0) || !read()) return false;
	if (!saved.isEmpty()) return true;
	auto body = scope; body["state"] = "prepared"; return write(body);
}
bool CloudWorkspaceAcceptanceIntent::advance(const QStringList &from, const QString &to)
{
	QLockFile lock(fileName + ".lock"); if (!lock.tryLock(0) || !read() || saved.isEmpty()) return false;
	if (state() == to) return true;
	if (!from.contains(state())) return false;
	auto next = saved; next["state"] = to; return write(next);
}
bool CloudWorkspaceAcceptanceIntent::submitted() { return advance({"prepared"}, "submitted"); }
bool CloudWorkspaceAcceptanceIntent::committed() { return advance({"submitted"}, "committed"); }
bool CloudWorkspaceAcceptanceIntent::completed() { return advance({"committed"}, "completed"); }

CloudWorkspaceAcceptance::CloudWorkspaceAcceptance(QNetworkAccessManager *network, QObject *parent) :
	QObject(parent), files(network, this), sourceReview(network, this)
{
	files.setStrictReads(true); deadline.setSingleShot(true); deadline.setInterval(180000); startupRetry.setSingleShot(true);
	connect(&deadline, &QTimer::timeout, this, [this] { fail(); });
	connect(&startupRetry, &QTimer::timeout, this, &CloudWorkspaceAcceptance::begin);
	connect(&files, &CloudSyncFileStore::versionedDownloadFinished, this,
		[this](CloudSyncProviderType, const QString &name, const QByteArray &body, const QString &version, const QString &id) { received(name, body, version, id); });
	connect(&files, &CloudSyncFileStore::uploadFinished, this, [this](CloudSyncProviderType, const QString &) {
		if (step == Step::WriteRoot) readRoot(Step::ConfirmRoot); else fail();
	});
	connect(&files, &CloudSyncFileStore::writeConflict, this, [this](CloudSyncProviderType, const QString &) {
		if (step == Step::WriteRoot) readRoot(Step::ConfirmRoot); else fail();
	});
	connect(&files, &CloudSyncFileStore::operationError, this, [this](CloudSyncProviderType, const QString &, const QString &, bool rejected) {
		if (step == Step::WriteRoot && !rejected) readRoot(Step::ConfirmRoot); else fail(rejected);
	});
	connect(&files, &CloudSyncFileStore::fileNotFound, this, [this](CloudSyncProviderType, const QString &) { fail(); });
	connect(&sourceReview, &CloudWorkspaceSourceReview::failed, this, [this](const QString &, bool rejected) { fail(rejected); });
	connect(&sourceReview, &CloudWorkspaceSourceReview::finished, this, [this](const QJsonObject &value, const QByteArray &body) {
		if (!current() || !intent->inspect()) { fail(); return; }
		CloudWorkspaceCopySource observed; observed.value = value;
		if (!observed.valid() || !observed.accepts(body)) { fail(); return; }
		if (step == Step::SourceBefore) {
			if (observed.json() != source.json()) { fail(); return; }
			const auto t = root.ticket(copyTicket); step = Step::CopiedPayload; files.setResponseLimit(64 * 1024 * 1024);
			files.downloadExact(provider, token, text(t, "name"), text(t, "object"), text(t, "version")); return;
		}
		if (step == Step::SourceAfter) {
			sourceChanged = observed.json() != source.json();
			readRoot(Step::FinalRoot); return;
		}
		fail();
	});
}
CloudWorkspaceAcceptance::~CloudWorkspaceAcceptance() { cancel(); }
void CloudWorkspaceAcceptance::cancel()
{
	++generation; step = Step::Idle; deadline.stop(); startupRetry.stop(); startupWait.invalidate(); files.cancel(provider); sourceReview.cancel(); intent.reset(); operationLock.reset();
	reference.value = {}; source.value = {}; root.value = {}; pending.value = {}; sourceChanged = false; token.clear(); choiceFile.clear(); choiceVersion.clear();
	checkpointFile.clear(); intentFile.clear(); localHash.clear(); event.clear(); previous.clear(); expectedHead.clear(); copyTicket.clear(); rootVersion.clear(); scopeCurrent = {};
}
void CloudWorkspaceAcceptance::fail(bool rejected)
{
	if (step == Step::Idle) return;
	cancel(); emit failed(tr("The cloud branch acceptance needs review. Your log, existing versions and saved attempt were kept."), rejected);
}
void CloudWorkspaceAcceptance::start(const QString &accessToken, const CloudWorkspaceReference &workspace, const CloudWorkspaceCopySource &observed,
	const QString &choice, const QString &choiceVer, const QString &checkpoint, const QString &intentPath, const QString &expectedLocalHash, const QString &eventId,
	const QString &expectedPrevious, const QString &head, const QString &copy, const std::function<bool()> &guard)
{
	cancel(); token = accessToken; reference = workspace; source = observed; choiceFile = choice; choiceVersion = choiceVer; checkpointFile = checkpoint;
	intentFile = intentPath; localHash = expectedLocalHash; event = eventId; previous = expectedPrevious; expectedHead = head; copyTicket = copy; scopeCurrent = guard; step = Step::Starting;
	if (token.isEmpty() || !reference.valid() || !source.matches(text(reference.value, "provider"), text(reference.value, "account")) ||
	    !CloudHistoryRecord::validEpoch(choiceVersion) || !QRegularExpression("^[a-f0-9]{64}$").match(localHash).hasMatch() || !CloudHistoryRecord::validEpoch(event) || !CloudHistoryRecord::validEpoch(expectedHead) ||
	    !CloudHistoryRecord::validEpoch(copyTicket) || (!previous.isEmpty() && !CloudHistoryRecord::validEpoch(previous)) ||
	    choiceFile.isEmpty() || checkpointFile.isEmpty() || intentFile.isEmpty() || !scopeCurrent) { fail(); return; }
	provider = text(reference.value, "provider") == "google-drive" ? CloudSyncProviderType::GoogleDrive : CloudSyncProviderType::Dropbox;
	startupWait.start(); begin();
}
bool CloudWorkspaceAcceptance::current()
{
	if (step == Step::Idle) return false;
	const auto serial = generation; const auto guard = scopeCurrent;
	const bool allowed = guard && guard();
	if (serial != generation || step == Step::Idle) return false;
	if (!allowed) { fail(); return false; }
	CloudWorkspaceSelection choice(choiceFile, text(reference.value, "provider"), text(reference.value, "account"));
	if (!choice.load()) { if (step == Step::Starting) retryStartupOrFail(choiceFile); else fail(); return false; }
	if (choice.version() != choiceVersion || choice.selected().value != reference.value) { fail(); return false; }
	return true;
}
void CloudWorkspaceAcceptance::retryStartupOrFail(const QString &path)
{
	QLockFile lock(path + ".lock"); const QFileInfo info(lock.fileName()); qint64 pid = 0; QString host, application;
	if (startupWait.isValid() && !startupWait.hasExpired(35000) && info.isFile() && !info.isSymLink() && !lock.getLockInfo(&pid, &host, &application)) {
		startupRetry.start(150); return;
	}
	fail();
}
void CloudWorkspaceAcceptance::begin()
{
	if (step != Step::Starting || !current()) return;
	CloudWorkspaceAuthority authority(checkpointFile, reference);
	if (!authority.load()) { retryStartupOrFail(checkpointFile); return; }
	if (!authority.record().valid()) { fail(); return; }
	const QFileInfo parent(QFileInfo(intentFile).absolutePath());
	if (parent.isSymLink() || !QDir().mkpath(parent.filePath())) { fail(); return; }
	if (!operationLock) operationLock = std::make_unique<QLockFile>(intentFile + ".operation.lock");
	if (!operationLock->isLocked() && !operationLock->tryLock(0)) { retryStartupOrFail(intentFile + ".operation"); return; }
	intent = std::make_unique<CloudWorkspaceAcceptanceIntent>(intentFile, reference, choiceVersion, source, localHash, event, previous, expectedHead, copyTicket);
	if (!intent->prepare()) { retryStartupOrFail(intentFile); return; }
	if (!current()) return;
	startupRetry.stop(); startupWait.invalidate(); deadline.start(); readRoot(Step::InitialRoot);
}
void CloudWorkspaceAcceptance::readRoot(Step next)
{
	if (!current()) return;
	step = next; files.setResponseLimit(1024 * 1024 + 4096);
	files.downloadIdentity(provider, token, reference.rootName(), text(reference.value, "root_id"));
}
bool CloudWorkspaceAcceptance::eventPresent(const CloudWorkspaceRecord &observed) const
{
	const QJsonObject expected{{"id", event}, {"parent", previous}, {"ticket", copyTicket}, {"head", expectedHead}};
	for (const auto &entry : observed.value.value("legacy_acceptances").toArray()) if (entry.toObject() == expected) return true;
	return false;
}
void CloudWorkspaceAcceptance::observeSource(Step next)
{
	if (!current()) return;
	step = next; sourceReview.start(token, text(reference.value, "provider"), text(reference.value, "account"), [this] { return current(); });
}
void CloudWorkspaceAcceptance::received(const QString &name, const QByteArray &bytes, const QString &version, const QString &id)
{
	if (!current() || !intent->inspect()) { fail(); return; }
	if (step == Step::CopiedPayload || step == Step::FinalPayload) {
		const auto t = root.ticket(copyTicket);
		if (name != text(t, "name") || id != text(t, "object") || version != text(t, "version") || !source.accepts(bytes)) { fail(); return; }
		if (step == Step::CopiedPayload) { readRoot(Step::PublishRoot); return; }
		if (!intent->completed()) { fail(); return; }
		const auto result = root.value; const auto v = rootVersion, eventId = event;
		const bool latest = text(root.latestLegacyAcceptance(), "id") == event, changed = sourceChanged;
		cancel(); emit finished(result, v, eventId, latest, changed); return;
	}
	if (name != reference.rootName() || id != text(reference.value, "root_id") || version.isEmpty()) { fail(); return; }
	auto observed = CloudWorkspaceRecord::parse(bytes); CloudWorkspaceAuthority authority(checkpointFile, reference);
	if (!observed.valid() || observed.reference().value != reference.value ||
	    (step == Step::ConfirmRoot && pending.valid() && !observed.follows(pending)) ||
	    !authority.observe(observed, version, true) || !current()) { fail(); return; }
	root = observed; rootVersion = version;
	if (step == Step::FinalRoot) {
		if (!eventPresent(root) || root.copySource(copyTicket).json() != source.json() || (intent->state() != "committed" && intent->state() != "completed")) { fail(); return; }
		const auto t = root.ticket(copyTicket); step = Step::FinalPayload; files.setResponseLimit(64 * 1024 * 1024);
		files.downloadExact(provider, token, text(t, "name"), text(t, "object"), text(t, "version")); return;
	}
	if (eventPresent(root)) {
		if (root.copySource(copyTicket).json() != source.json() || intent->state() == "prepared" || (intent->state() != "completed" && !intent->committed())) { fail(); return; }
		observeSource(Step::SourceAfter); return;
	}
	if (intent->state() == "committed" || intent->state() == "completed" || step == Step::ConfirmRoot || root.head() != expectedHead ||
	    text(root.latestLegacyAcceptance(), "id") != previous || root.copySource(copyTicket).json() != source.json()) { fail(); return; }
	if (step == Step::InitialRoot) { observeSource(Step::SourceBefore); return; }
	if (step != Step::PublishRoot) { fail(); return; }
	auto next = root; if (!next.acceptLegacyCopy(event, previous, expectedHead, copyTicket, source) ||
	    !intent->submitted() || !current()) { fail(); return; }
	pending = next; step = Step::WriteRoot;
	files.uploadIdentity(provider, token, reference.rootName(), next.json(), rootVersion, text(reference.value, "root_id"));
}
