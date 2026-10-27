// SPDX-License-Identifier: GPL-2.0
#include "cloudsyncworkspace.h"
#include "cloudsyncmanifest.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QLockFile>
#include <QRegularExpression>
#include <QSaveFile>

namespace {
constexpr int ReferenceLimit = 2048, SelectionLimit = 64 * 1024, WorkspaceLimit = 32;
constexpr int RootLimit = 1024 * 1024 + ReferenceLimit + 1024;
constexpr int CheckpointLimit = RootLimit + 2048;
bool providerValid(const QString &s) { return s == "google-drive" || s == "dropbox"; }
bool hash(const QString &s) { return QRegularExpression(QStringLiteral("^[a-f0-9]{64}$")).match(s).hasMatch(); }
bool uuid(const QString &s) { return QRegularExpression(QStringLiteral("^[a-f0-9]{8}(?:-[a-f0-9]{4}){3}-[a-f0-9]{12}$")).match(s).hasMatch(); }
bool versionValid(const QString &s) { return !s.isEmpty() && s.size() <= 512 && !s.contains(QChar::Null) && !s.contains('\r') && !s.contains('\n'); }
bool fields(const QJsonObject &o, const QStringList &keys) {
	if (o.size() != keys.size()) return false;
	for (const auto &key : keys) if (!o.contains(key)) return false;
	return true;
}
QString string(const QJsonObject &o, const char *key) { return o.value(QLatin1String(key)).toString(); }
}

CloudWorkspaceReference CloudWorkspaceRecord::reference() const
{
	CloudWorkspaceReference result; result.value = value.value("workspace").toObject(); return result;
}
QByteArray CloudWorkspaceRecord::json() const { return QJsonDocument(value).toJson(QJsonDocument::Compact); }
QString CloudWorkspaceRecord::head() const { return value.value("journal").toObject().value("head").toString(); }
QJsonObject CloudWorkspaceRecord::ticket(const QString &id) const { return value.value("journal").toObject().value("tickets").toObject().value(id).toObject(); }
CloudHistoryRecord CloudWorkspaceRecord::journal() const
{
	CloudHistoryRecord result;
	const auto ref = reference();
	if (!ref.valid() || !fields(value, {"schema", "format", "workspace", "journal"}) || value.value("schema") != QJsonValue(4) ||
	    string(value, "format") != "neo-workspace-history-development-1" || !value.value("journal").isObject()) return result;
	auto journal = value.value("journal").toObject();
	if (!journal.value("tickets").isObject()) return result;
	for (const auto &key : {"provider", "account", "epoch"}) if (journal.value(QLatin1String(key)) != ref.value.value(QLatin1String(key))) return result;
	auto tickets = journal.value("tickets").toObject();
	for (auto it = tickets.begin(); it != tickets.end(); ++it) {
		auto t = it.value().toObject();
		if (string(t, "name") != ref.payloadName(it.key())) return result;
		// Normalize only in memory to reuse the existing journal state machine.
		// Wire records always retain the exact root-qualified ownership name.
		t["name"] = CloudHistoryRecord::payloadName(string(ref.value, "epoch"), it.key()); it.value() = t;
	}
	journal["tickets"] = tickets; result.value = journal; return result;
}
bool CloudWorkspaceRecord::valid() const
{
	if (json().size() > RootLimit) return false;
	const auto internal = journal();
	if (!internal.valid()) return false;
	const auto ref = reference();
	const auto tickets = internal.value.value("tickets").toObject();
	for (const auto &entry : tickets) {
		const auto t = entry.toObject(); const auto id = string(t, "object"), version = string(t, "version");
		if (string(t, "state") != "registered" &&
		    (!CloudWorkspaceReference::create(string(ref.value, "provider"), string(ref.value, "account"), string(ref.value, "epoch"), id).valid() ||
		     id == string(ref.value, "root_id") || !versionValid(version))) return false;
	}
	return true;
}
CloudWorkspaceRecord CloudWorkspaceRecord::create(const CloudWorkspaceReference &reference, const QString &legacyHash)
{
	CloudWorkspaceRecord result;
	if (!reference.valid()) return result;
	const auto journal = CloudHistoryRecord::create(string(reference.value, "provider"), string(reference.value, "account"), string(reference.value, "epoch"), legacyHash);
	result.value = {{"schema", 4}, {"format", "neo-workspace-history-development-1"}, {"workspace", reference.value}, {"journal", journal.value}};
	if (!result.valid()) result.value = {};
	return result;
}
CloudWorkspaceRecord CloudWorkspaceRecord::parse(const QByteArray &bytes)
{
	CloudWorkspaceRecord result;
	if (bytes.size() > RootLimit) return result;
	result.value = QJsonDocument::fromJson(bytes).object();
	if (!result.valid()) result.value = {};
	return result;
}
bool CloudWorkspaceRecord::follows(const CloudWorkspaceRecord &previous) const
{
	return valid() && previous.valid() && reference().value == previous.reference().value && journal().follows(previous.journal());
}
bool CloudWorkspaceRecord::replaceJournal(const CloudHistoryRecord &record)
{
	if (!valid() || !record.valid() || !record.follows(journal())) return false;
	auto next = *this; auto wire = record.value; auto tickets = wire.value("tickets").toObject();
	for (auto it = tickets.begin(); it != tickets.end(); ++it) {
		auto t = it.value().toObject(); t["name"] = reference().payloadName(it.key()); it.value() = t;
	}
	wire["tickets"] = tickets; next.value["journal"] = wire;
	if (!next.valid()) return false;
	value = next.value; return true;
}
bool CloudWorkspaceRecord::registerUpload(const QString &id, const QString &expectedHead, const QString &hash, qint64 bytes, const QString &source, const QString &reservation)
{
	auto next = journal(); return next.registerUpload(id, expectedHead, hash, bytes, source, reservation) && replaceJournal(next);
}
bool CloudWorkspaceRecord::completeUpload(const QString &id, const QString &object, const QString &version)
{
	auto next = journal(); return next.completeUpload(id, object, version) && replaceJournal(next);
}
bool CloudWorkspaceRecord::publish(const QString &id, const QString &expectedHead)
{
	auto next = journal(); return next.publish(id, expectedHead) && replaceJournal(next);
}
bool CloudWorkspaceRecord::reserve(const QString &id, const QString &commit)
{
	auto next = journal(); return next.reserve(id, commit) && replaceJournal(next);
}
bool CloudWorkspaceRecord::release(const QString &id)
{
	auto next = journal(); return next.release(id) && replaceJournal(next);
}

CloudWorkspaceAuthority::CloudWorkspaceAuthority(const QString &path, const CloudWorkspaceReference &ref) : fileName(path), reference(ref) {}
bool CloudWorkspaceAuthority::readUnlocked()
{
	checkpoint.value = {}; observedVersion.clear();
	if (!reference.valid()) return false;
	const QFileInfo info(fileName);
	if (!info.exists()) return !info.isSymLink();
	if (!info.isFile() || info.isSymLink() || info.size() > CheckpointLimit) return false;
	QFile file(fileName); if (!file.open(QIODevice::ReadOnly)) return false;
	const auto bytes = file.read(CheckpointLimit + 1);
	if (bytes.size() > CheckpointLimit || file.error() != QFileDevice::NoError) return false;
	const auto saved = QJsonDocument::fromJson(bytes).object();
	if (!fields(saved, {"schema", "format", "workspace_key", "record", "version"}) || saved.value("schema") != QJsonValue(1) ||
	    string(saved, "format") != "neo-workspace-checkpoint-development-1" || string(saved, "workspace_key") != reference.key() ||
	    !versionValid(string(saved, "version"))) return false;
	CloudWorkspaceRecord root; root.value = saved.value("record").toObject();
	if (!root.valid() || root.reference().value != reference.value) return false;
	checkpoint = root; observedVersion = string(saved, "version"); return true;
}
bool CloudWorkspaceAuthority::load()
{
	checkpoint.value = {}; observedVersion.clear();
	if (fileName.isEmpty()) return false;
	if (!QFileInfo(QFileInfo(fileName).absolutePath()).exists()) return readUnlocked();
	QLockFile lock(fileName + ".lock"); return lock.tryLock(0) && readUnlocked();
}
bool CloudWorkspaceAuthority::observe(const CloudWorkspaceRecord &root, const QString &version)
{
	if (fileName.isEmpty() || !reference.valid() || !root.valid() || root.reference().value != reference.value || !versionValid(version) ||
	    !QDir().mkpath(QFileInfo(fileName).absolutePath())) return false;
	QLockFile lock(fileName + ".lock"); if (!lock.tryLock(0) || !readUnlocked()) return false;
	if (checkpoint.valid() && !root.follows(checkpoint)) return false;
	if (checkpoint.json() == root.json() && observedVersion == version) return true;
	const auto bytes = QJsonDocument(QJsonObject{{"schema", 1}, {"format", "neo-workspace-checkpoint-development-1"},
		{"workspace_key", reference.key()}, {"record", root.value}, {"version", version}}).toJson(QJsonDocument::Compact);
	if (bytes.size() > CheckpointLimit) return false;
	QSaveFile file(fileName);
	if (!file.open(QIODevice::WriteOnly) || file.write(bytes) != bytes.size() || !file.commit()) return false;
	return readUnlocked() && checkpoint.json() == root.json() && observedVersion == version;
}

CloudWorkspaceOpen::CloudWorkspaceOpen(QNetworkAccessManager *network, QObject *parent) : QObject(parent), files(network, this)
{
	files.setStrictReads(true); files.setResponseLimit(RootLimit);
	deadline.setSingleShot(true); deadline.setInterval(180000);
	connect(&deadline, &QTimer::timeout, this, [this] { fail(); });
	connect(&files, &CloudSyncFileStore::versionedDownloadFinished, this,
		[this](CloudSyncProviderType type, const QString &name, const QByteArray &bytes, const QString &version, const QString &id) {
			if (!active) return;
			const auto root = CloudWorkspaceRecord::parse(bytes);
			if (!scopeCurrent || !scopeCurrent() || type != provider || id != string(reference.value, "root_id") || name != reference.rootName() ||
			    !versionValid(version) || !root.valid() ||
			    root.reference().value != reference.value || (!checkpoint.value.isEmpty() && !root.follows(checkpoint))) { fail(); return; }
			const auto confirmed = root.value; cancel(); emit finished(confirmed, version);
		});
	connect(&files, &CloudSyncFileStore::operationError, this, [this](CloudSyncProviderType, const QString &, const QString &, bool rejected) { fail(rejected); });
	connect(&files, &CloudSyncFileStore::fileNotFound, this, [this](CloudSyncProviderType, const QString &) { fail(); });
	connect(&files, &CloudSyncFileStore::writeConflict, this, [this](CloudSyncProviderType, const QString &) { fail(); });
}
CloudWorkspaceOpen::~CloudWorkspaceOpen() { cancel(); }
void CloudWorkspaceOpen::cancel()
{
	active = false; deadline.stop(); files.cancel(provider); reference.value = {}; checkpoint.value = {}; scopeCurrent = {};
}
void CloudWorkspaceOpen::fail(bool rejected)
{
	if (!active) return;
	cancel(); emit failed(tr("Cloud workspace could not be verified. Keep the current log and check the selected workspace before retrying."), rejected);
}
void CloudWorkspaceOpen::start(const QString &token, const QString &providerId, const QString &account, const CloudWorkspaceReference &ref,
	const std::function<bool()> &guard, const CloudWorkspaceRecord &previous)
{
	cancel(); active = true; reference = ref; checkpoint = previous; scopeCurrent = guard;
	provider = providerId == "google-drive" ? CloudSyncProviderType::GoogleDrive : CloudSyncProviderType::Dropbox;
	if (token.isEmpty() || !reference.matches(providerId, account) || !scopeCurrent || !scopeCurrent() ||
	    (!checkpoint.value.isEmpty() && (!checkpoint.valid() || checkpoint.reference().value != reference.value))) { fail(); return; }
	deadline.start(); files.downloadIdentity(provider, token, reference.rootName(), string(reference.value, "root_id"));
}

CloudWorkspaceReference CloudWorkspaceReference::create(const QString &provider, const QString &account, const QString &epoch, const QString &rootId)
{
	CloudWorkspaceReference result;
	result.value = {{"schema", 1}, {"format", "neo-workspace-reference-development-1"}, {"provider", provider},
		{"account", account}, {"epoch", epoch}, {"root_id", rootId}};
	if (!result.valid()) result.value = {};
	return result;
}
CloudWorkspaceReference CloudWorkspaceReference::parse(const QByteArray &bytes)
{
	CloudWorkspaceReference result;
	if (bytes.size() > ReferenceLimit) return result;
	result.value = QJsonDocument::fromJson(bytes).object();
	if (!result.valid()) result.value = {};
	return result;
}
bool CloudWorkspaceReference::valid() const
{
	const auto provider = string(value, "provider"), id = string(value, "root_id");
	return fields(value, {"schema", "format", "provider", "account", "epoch", "root_id"}) && value.value("schema") == QJsonValue(1) &&
		string(value, "format") == "neo-workspace-reference-development-1" && providerValid(provider) && hash(string(value, "account")) &&
		uuid(string(value, "epoch")) && id.size() <= 512 && QRegularExpression(provider == "google-drive" ? QStringLiteral("^[A-Za-z0-9_-]+$") :
		QStringLiteral("^id:[A-Za-z0-9_-]+$")).match(id).hasMatch() && json().size() <= ReferenceLimit;
}
QByteArray CloudWorkspaceReference::json() const { return QJsonDocument(value).toJson(QJsonDocument::Compact); }
QString CloudWorkspaceReference::key() const { return valid() ? CloudSyncManifest::sha256(json()) : QString(); }
QString CloudWorkspaceReference::rootName() const { return valid() ? "subsurface-neo-workspace-" + string(value, "epoch") + ".json" : QString(); }
QString CloudWorkspaceReference::payloadName(const QString &ticket) const
{
	return valid() && uuid(ticket) ? "subsurface-neo-workspace-" + string(value, "epoch") + "-" + key() + "-" + ticket + ".xml" : QString();
}
bool CloudWorkspaceReference::matches(const QString &provider, const QString &account) const
{
	return valid() && string(value, "provider") == provider && string(value, "account") == account;
}
CloudWorkspaceSelection::CloudWorkspaceSelection(const QString &path, const QString &provider, const QString &account) :
	fileName(path), provider(provider), account(account) {}
CloudWorkspaceReference CloudWorkspaceSelection::selected() const
{
	CloudWorkspaceReference result; result.value = references.value(selectedKey).toObject(); return result;
}
bool CloudWorkspaceSelection::readUnlocked()
{
	selectedKey.clear(); references = {};
	if (!providerValid(provider) || !hash(account)) return false;
	const QFileInfo info(fileName);
	if (!info.exists()) return !info.isSymLink();
	if (!info.isFile() || info.isSymLink() || info.size() > SelectionLimit) return false;
	QFile file(fileName); if (!file.open(QIODevice::ReadOnly)) return false;
	const auto bytes = file.read(SelectionLimit + 1);
	if (bytes.size() > SelectionLimit || file.error() != QFileDevice::NoError) return false;
	const auto root = QJsonDocument::fromJson(bytes).object();
	if (!fields(root, {"schema", "format", "provider", "account", "selected", "references"}) || root.value("schema") != QJsonValue(1) ||
	    string(root, "format") != "neo-workspace-selection-development-1" || string(root, "provider") != provider || string(root, "account") != account ||
	    !root.value("references").isObject() || !hash(string(root, "selected"))) return false;
	const auto known = root.value("references").toObject();
	if (known.isEmpty() || known.size() > WorkspaceLimit || !known.contains(string(root, "selected"))) return false;
	for (auto it = known.begin(); it != known.end(); ++it) {
		CloudWorkspaceReference reference; reference.value = it.value().toObject();
		if (!reference.matches(provider, account) || reference.key() != it.key()) return false;
	}
	references = known; selectedKey = string(root, "selected"); return true;
}
bool CloudWorkspaceSelection::load()
{
	selectedKey.clear(); references = {};
	if (fileName.isEmpty()) return false;
	if (!QFileInfo(QFileInfo(fileName).absolutePath()).exists()) return readUnlocked();
	QLockFile lock(fileName + ".lock"); return lock.tryLock(0) && readUnlocked();
}
bool CloudWorkspaceSelection::select(const QString &expectedKey, const CloudWorkspaceReference &reference)
{
	if (fileName.isEmpty() || !reference.matches(provider, account) || (!expectedKey.isEmpty() && !hash(expectedKey)) ||
	    !QDir().mkpath(QFileInfo(fileName).absolutePath())) return false;
	QLockFile lock(fileName + ".lock"); if (!lock.tryLock(0) || !readUnlocked()) return false;
	const auto key = reference.key();
	if (selectedKey == key) return true; // Reconcile an already completed local choice without rewriting it.
	if (selectedKey != expectedKey || (!references.contains(key) && references.size() == WorkspaceLimit)) return false;
	auto next = references; next[key] = reference.value;
	const auto bytes = QJsonDocument(QJsonObject{{"schema", 1}, {"format", "neo-workspace-selection-development-1"}, {"provider", provider},
		{"account", account}, {"selected", key}, {"references", next}}).toJson(QJsonDocument::Compact);
	if (bytes.size() > SelectionLimit) return false;
	QSaveFile file(fileName);
	if (!file.open(QIODevice::WriteOnly) || file.write(bytes) != bytes.size() || !file.commit()) return false;
	return readUnlocked() && selectedKey == key && references == next;
}
