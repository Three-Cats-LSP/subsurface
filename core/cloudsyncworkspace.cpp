// SPDX-License-Identifier: GPL-2.0
#include "cloudsyncworkspace.h"
#include "cloudsyncmanifest.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonArray>
#include <QSet>
#include <QLockFile>
#include <QRegularExpression>
#include <QSaveFile>
#include <QUuid>

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
CloudWorkspaceCopySource CloudWorkspaceRecord::copySource(const QString &id) const
{
	CloudWorkspaceCopySource source; source.value = value.value("copies").toObject().value(id).toObject(); return source;
}
CloudHistoryRecord CloudWorkspaceRecord::journal() const
{
	CloudHistoryRecord result;
	const auto ref = reference();
	const auto format = string(value, "format");
	const bool acceptance = format == "neo-workspace-history-development-3";
	const bool copies = acceptance || format == "neo-workspace-history-development-2";
	QStringList keys = {"schema", "format", "workspace", "journal"}; if (copies) keys.append("copies");
	if (acceptance) keys.append("legacy_acceptances");
	if (!ref.valid() || !fields(value, keys) || value.value("schema") != QJsonValue(4) ||
	    (!copies && format != "neo-workspace-history-development-1") || !value.value("journal").isObject() ||
	    (copies && (!value.value("copies").isObject() || value.value("copies").toObject().isEmpty())) ||
	    (acceptance && (!value.value("legacy_acceptances").isArray() || value.value("legacy_acceptances").toArray().isEmpty()))) return result;
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
	const auto copies = value.value("copies").toObject();
	for (auto it = copies.begin(); it != copies.end(); ++it) {
		const auto source = copySource(it.key()); const auto t = ticket(it.key()); const auto p = source.value.value("payload").toObject(), m = source.value.value("manifest").toObject();
		if (!uuid(it.key()) || t.isEmpty() || !source.matches(string(ref.value, "provider"), string(ref.value, "account")) ||
		    t.value("sha256") != p.value("sha256") || t.value("bytes") != p.value("bytes") || !string(t, "restore_source").isEmpty() ||
		    string(p, "id") == string(ref.value, "root_id") || string(m, "id") == string(ref.value, "root_id") ||
		    (!string(t, "object").isEmpty() && (string(t, "object") == string(p, "id") || string(t, "object") == string(m, "id")))) return false;
	}
	if (string(value, "format") == "neo-workspace-history-development-3") {
		const auto events = value.value("legacy_acceptances").toArray();
		const auto history = internal.value.value("history").toArray();
		if (events.size() > 512) return false;
		QSet<QString> ids; QString parent;
		for (const auto &entry : events) {
			if (!entry.isObject()) return false;
			const auto event = entry.toObject();
			const auto id = string(event, "id"), previous = string(event, "parent"), copy = string(event, "ticket"), observedHead = string(event, "head");
			int copyIndex = -1, headIndex = -1;
			for (int i = 0; i < history.size(); ++i) {
				const auto commit = history[i].toString();
				if (commit == copy) copyIndex = i;
				if (commit == observedHead) headIndex = i;
			}
			if (!fields(event, {"id", "parent", "ticket", "head"}) || !uuid(id) || ids.contains(id) || tickets.contains(id) ||
			    internal.value.value("reservations").toObject().contains(id) || previous != parent ||
			    !uuid(copy) || !uuid(observedHead) || id == copy || id == observedHead || copyIndex < 0 || headIndex < copyIndex ||
			    string(ticket(copy), "state") != "committed" || !copySource(copy).valid()) return false;
			ids.insert(id); parent = id;
		}
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
	if (!valid() || !previous.valid() || reference().value != previous.reference().value || !journal().follows(previous.journal())) return false;
	const auto before = previous.value.value("copies").toObject(), after = value.value("copies").toObject();
	for (auto it = before.begin(); it != before.end(); ++it) if (after.value(it.key()) != it.value()) return false;
	for (auto it = after.begin(); it != after.end(); ++it) if (!before.contains(it.key()) && !previous.ticket(it.key()).isEmpty()) return false;
	const auto format = string(value, "format"), oldFormat = string(previous.value, "format");
	if (oldFormat == "neo-workspace-history-development-3" || format == "neo-workspace-history-development-3") {
		if (format != "neo-workspace-history-development-3" ||
		    (oldFormat != "neo-workspace-history-development-2" && oldFormat != "neo-workspace-history-development-3")) return false;
		const auto oldEvents = previous.value.value("legacy_acceptances").toArray(), events = value.value("legacy_acceptances").toArray();
		if (events.size() < oldEvents.size() || (oldFormat != format && events.isEmpty())) return false;
		for (int i = 0; i < oldEvents.size(); ++i) if (events[i] != oldEvents[i]) return false;
		return true;
	}
	return value.value("format") == previous.value.value("format") ||
		(oldFormat == "neo-workspace-history-development-1" && format == "neo-workspace-history-development-2" &&
		 journal().value.value("sequence").toInt() > previous.journal().value.value("sequence").toInt());
}
QJsonObject CloudWorkspaceRecord::latestLegacyAcceptance() const
{
	const auto events = value.value("legacy_acceptances").toArray();
	return events.isEmpty() ? QJsonObject() : events.last().toObject();
}
bool CloudWorkspaceRecord::acceptLegacyCopy(const QString &id, const QString &expectedAcceptance, const QString &expectedHead,
	const QString &copyTicket, const CloudWorkspaceCopySource &observed)
{
	if (!valid() || !uuid(id) || expectedHead != head() || expectedAcceptance != string(latestLegacyAcceptance(), "id") ||
	    !observed.valid() || observed.json() != copySource(copyTicket).json() || string(ticket(copyTicket), "state") != "committed") return false;
	auto next = *this; auto events = value.value("legacy_acceptances").toArray();
	if (events.size() >= 512) return false;
	events.append(QJsonObject{{"id", id}, {"parent", expectedAcceptance}, {"ticket", copyTicket}, {"head", expectedHead}});
	next.value["format"] = "neo-workspace-history-development-3"; next.value["legacy_acceptances"] = events;
	if (!next.valid() || !next.follows(*this)) return false;
	value = next.value; return true;
}
bool CloudWorkspaceRecord::registerCopy(const QString &id, const QString &parent, const CloudWorkspaceCopySource &source)
{
	const auto ref = reference(); const auto payload = source.value.value("payload").toObject(); auto next = *this;
	if (!valid() || !source.matches(string(ref.value, "provider"), string(ref.value, "account")) ||
	    !next.registerUpload(id, parent, string(payload, "sha256"), qint64(payload.value("bytes").toDouble()))) return false;
	auto copies = value.value("copies").toObject(); copies[id] = source.value;
	if (string(value, "format") != "neo-workspace-history-development-3") next.value["format"] = "neo-workspace-history-development-2";
	next.value["copies"] = copies;
	if (!next.valid() || !next.follows(*this)) return false;
	value = next.value; return true;
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
bool CloudWorkspaceAuthority::observe(const CloudWorkspaceRecord &root, const QString &version, bool requireExisting)
{
	if (fileName.isEmpty() || !reference.valid() || !root.valid() || root.reference().value != reference.value || !versionValid(version) ||
	    !QDir().mkpath(QFileInfo(fileName).absolutePath())) return false;
	QLockFile lock(fileName + ".lock"); if (!lock.tryLock(0) || !readUnlocked()) return false;
	if (requireExisting && !checkpoint.valid()) return false;
	if (checkpoint.valid() && !root.follows(checkpoint)) return false;
	if (checkpoint.json() == root.json() && observedVersion == version) return true;
	const auto bytes = QJsonDocument(QJsonObject{{"schema", 1}, {"format", "neo-workspace-checkpoint-development-1"},
		{"workspace_key", reference.key()}, {"record", root.value}, {"version", version}}).toJson(QJsonDocument::Compact);
	if (bytes.size() > CheckpointLimit) return false;
	QSaveFile file(fileName);
	if (!file.open(QIODevice::WriteOnly) || file.write(bytes) != bytes.size() || !file.commit()) return false;
	return readUnlocked() && checkpoint.json() == root.json() && observedVersion == version;
}

CloudWorkspaceCreationIntent::CloudWorkspaceCreationIntent(const QString &path, const QString &provider, const QString &account,
	const QString &epoch, const QString &nonce, const QString &originHash) : fileName(path)
{
	if (CloudWorkspaceReference::create(provider, account, epoch, provider == "google-drive" ? "bootstrap" : "id:bootstrap").valid() && uuid(nonce) && hash(originHash))
		scope = {{"schema", 1}, {"format", "neo-workspace-creation-development-1"}, {"provider", provider}, {"account", account},
			{"epoch", epoch}, {"nonce", nonce}, {"origin_sha256", originHash}};
}
QString CloudWorkspaceCreationIntent::state() const { return string(saved, "state"); }
QString CloudWorkspaceCreationIntent::id() const { return string(saved, "id"); }
QString CloudWorkspaceCreationIntent::rootName() const { return scope.isEmpty() ? QString() : "subsurface-neo-workspace-" + string(scope, "epoch") + ".json"; }
CloudWorkspaceReference CloudWorkspaceCreationIntent::reference() const
{
	return CloudWorkspaceReference::create(string(scope, "provider"), string(scope, "account"), string(scope, "epoch"), id());
}
QByteArray CloudWorkspaceCreationIntent::bootstrap() const
{
	if (scope.isEmpty()) return {};
	auto body = scope; body["schema"] = 4; body["format"] = "neo-workspace-bootstrap-development-1";
	return QJsonDocument(body).toJson(QJsonDocument::Compact);
}
bool CloudWorkspaceCreationIntent::readUnlocked()
{
	saved = {};
	if (scope.isEmpty()) return false;
	const QFileInfo info(fileName);
	if (!info.exists()) return !info.isSymLink();
	if (!info.isFile() || info.isSymLink() || info.size() > 8192) return false;
	QFile file(fileName); if (!file.open(QIODevice::ReadOnly)) return false;
	const auto bytes = file.read(8193); if (bytes.size() > 8192 || file.error() != QFileDevice::NoError) return false;
	const auto body = QJsonDocument::fromJson(bytes).object();
	if (!fields(body, {"schema", "format", "provider", "account", "epoch", "nonce", "origin_sha256", "state", "id"})) return false;
	for (auto it = scope.begin(); it != scope.end(); ++it) if (body.value(it.key()) != it.value()) return false;
	const auto stage = string(body, "state"), identity = string(body, "id");
	if ((stage != "prepared" && stage != "submitted" && stage != "identified" && stage != "binding" && stage != "bound") || !body.value("id").isString()) return false;
	if (stage == "prepared" || stage == "submitted") { if (!identity.isEmpty()) return false; }
	else if (!CloudWorkspaceReference::create(string(scope, "provider"), string(scope, "account"), string(scope, "epoch"), identity).valid()) return false;
	saved = body; return true;
}
bool CloudWorkspaceCreationIntent::writeUnlocked(const QJsonObject &next)
{
	QSaveFile file(fileName); const auto bytes = QJsonDocument(next).toJson(QJsonDocument::Compact);
	return file.open(QIODevice::WriteOnly) && file.write(bytes) == bytes.size() && file.commit() && readUnlocked() && saved == next;
}
bool CloudWorkspaceCreationIntent::prepare()
{
	if (fileName.isEmpty() || scope.isEmpty() || !QDir().mkpath(QFileInfo(fileName).absolutePath())) return false;
	QLockFile lock(fileName + ".lock"); if (!lock.tryLock(0) || !readUnlocked()) return false;
	if (!saved.isEmpty()) return true;
	auto next = scope; next["state"] = "prepared"; next["id"] = ""; return writeUnlocked(next);
}
bool CloudWorkspaceCreationIntent::advance(const QString &stage, const QString &identity)
{
	if (fileName.isEmpty() || scope.isEmpty()) return false;
	QLockFile lock(fileName + ".lock"); if (!lock.tryLock(0) || !readUnlocked() || saved.isEmpty()) return false;
	if (stage == "submitted") { if (state() != "prepared") return false; }
	else if (stage == "identified") {
		if (!CloudWorkspaceReference::create(string(scope, "provider"), string(scope, "account"), string(scope, "epoch"), identity).valid()) return false;
		if (state() == "identified" || state() == "binding" || state() == "bound") return id() == identity;
		if (state() != "submitted") return false;
	} else if (stage == "binding") { if (state() == "binding") return true; if (state() != "identified") return false; }
	else if (stage == "bound") { if (state() == "bound") return true; if (state() != "binding") return false; }
	else return false;
	auto next = saved; next["state"] = stage; if (stage == "identified") next["id"] = identity; return writeUnlocked(next);
}
bool CloudWorkspaceCreationIntent::submit() { return advance("submitted"); }
bool CloudWorkspaceCreationIntent::identify(const QString &id) { return advance("identified", id); }
bool CloudWorkspaceCreationIntent::beginBinding() { return advance("binding"); }
bool CloudWorkspaceCreationIntent::bind() { return advance("bound"); }
bool CloudWorkspaceCreationIntent::reload()
{
	if (fileName.isEmpty() || scope.isEmpty()) return false;
	QLockFile lock(fileName + ".lock"); return lock.tryLock(0) && readUnlocked() && !saved.isEmpty();
}

CloudWorkspaceCreate::CloudWorkspaceCreate(QNetworkAccessManager *network, QObject *parent) : QObject(parent), files(network, this)
{
	files.setStrictReads(true); files.setResponseLimit(RootLimit);
	deadline.setSingleShot(true); deadline.setInterval(180000);
	connect(&deadline, &QTimer::timeout, this, [this] { fail(); });
	connect(&files, &CloudSyncFileStore::workspaceIdentityCreated, this, [this](CloudSyncProviderType type, const QString &name, const QString &id) {
		if (step != Step::Create) return;
		// Retain the acknowledged old-scope ID even if that scope just changed.
		// No dependent request/binding is allowed after the scope guard fails.
		if (type != provider || name != intent->rootName() || !intent->identify(id)) { fail(); return; }
		read(Step::Read);
	});
	connect(&files, &CloudSyncFileStore::versionedDownloadFinished, this, &CloudWorkspaceCreate::received);
	connect(&files, &CloudSyncFileStore::uploadFinished, this, [this](CloudSyncProviderType, const QString &) { if (step == Step::Bind) read(Step::Confirm); });
	connect(&files, &CloudSyncFileStore::writeConflict, this, [this](CloudSyncProviderType, const QString &) { if (step == Step::Bind) read(Step::Confirm); else fail(); });
	connect(&files, &CloudSyncFileStore::operationError, this, [this](CloudSyncProviderType, const QString &, const QString &, bool rejected) {
		if (step == Step::Bind && !rejected) read(Step::Confirm); else fail(rejected);
	});
	connect(&files, &CloudSyncFileStore::fileNotFound, this, [this](CloudSyncProviderType, const QString &) { fail(); });
}
CloudWorkspaceCreate::~CloudWorkspaceCreate() { cancel(); }
void CloudWorkspaceCreate::cancel()
{
	step = Step::Idle; deadline.stop(); files.cancel(provider); intent.reset(); scopeCurrent = {}; token.clear(); checkpointDirectory.clear(); originHash.clear();
}
void CloudWorkspaceCreate::fail(bool rejected)
{
	if (step == Step::Idle) return;
	cancel(); emit failed(tr("Workspace creation is incomplete. All files were kept; review or resume the recorded attempt before creating another workspace."), rejected);
}
bool CloudWorkspaceCreate::current()
{
	if (step == Step::Idle) return false;
	if (!scopeCurrent || !scopeCurrent()) { fail(); return false; }
	return true;
}
void CloudWorkspaceCreate::start(const QString &intentFile, const QString &directory, const QString &providerId, const QString &accessToken,
	const QString &account, const QString &epoch, const QString &nonce, const QString &origin, const std::function<bool()> &guard)
{
	cancel(); step = Step::Read; provider = providerId == "google-drive" ? CloudSyncProviderType::GoogleDrive : CloudSyncProviderType::Dropbox;
	token = accessToken; checkpointDirectory = directory; originHash = origin; scopeCurrent = guard;
	intent = std::make_unique<CloudWorkspaceCreationIntent>(intentFile, providerId, account, epoch, nonce, origin);
	if (!current()) return;
	if (token.isEmpty() || directory.isEmpty() || !intent->prepare() || !QDir().mkpath(directory)) { fail(); return; }
	if (intent->state() == "prepared") {
		if (!current() || !intent->submit()) { fail(); return; }
		step = Step::Create;
		if (current()) files.createWorkspaceIdentity(provider, token, intent->rootName(), intent->bootstrap());
	} else if (intent->state() == "submitted") fail(); // Unknown create result: never replay or look up a name.
	else {
		if (intent->state() == "bound") {
			CloudWorkspaceAuthority saved(QDir(directory).filePath(intent->reference().key() + ".json"), intent->reference());
			if (!saved.load() || !saved.record().valid()) { fail(); return; }
		}
		read(Step::Read);
	}
}
void CloudWorkspaceCreate::read(Step next)
{
	if (!current()) return;
	step = next; files.downloadIdentity(provider, token, intent->rootName(), intent->id());
}
void CloudWorkspaceCreate::received(CloudSyncProviderType type, const QString &name, const QByteArray &bytes, const QString &version, const QString &id)
{
	if (!current()) return;
	if (!intent->reload()) { fail(); return; }
	const auto ref = intent->reference();
	if ((step != Step::Read && step != Step::Confirm) || type != provider || name != intent->rootName() || id != intent->id() || !versionValid(version)) { fail(); return; }
	const auto path = QDir(checkpointDirectory).filePath(ref.key() + ".json"); CloudWorkspaceAuthority saved(path, ref);
	if (!saved.load()) { fail(); return; }
	const auto initial = CloudWorkspaceRecord::create(ref, originHash);
	if (bytes == intent->bootstrap()) {
		if (step != Step::Read || (intent->state() != "identified" && intent->state() != "binding") || saved.record().valid() || !initial.valid() || !intent->beginBinding()) { fail(); return; }
		step = Step::Bind; if (current()) files.uploadIdentity(provider, token, name, initial.json(), version, id); return;
	}
	const auto root = CloudWorkspaceRecord::parse(bytes);
	if ((intent->state() != "binding" && intent->state() != "bound") || !root.follows(initial) || (saved.record().valid() && !root.follows(saved.record())) || !current() || !saved.observe(root, version, intent->state() == "bound" || saved.record().valid()) || !current() || !intent->bind()) { fail(); return; }
	const auto confirmed = root.value; cancel(); emit finished(confirmed, version, path);
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
	selectedKey.clear(); choiceVersion.clear(); references = {};
	if (!providerValid(provider) || !hash(account)) return false;
	const QFileInfo info(fileName);
	if (!info.exists()) return !info.isSymLink();
	if (!info.isFile() || info.isSymLink() || info.size() > SelectionLimit) return false;
	QFile file(fileName); if (!file.open(QIODevice::ReadOnly)) return false;
	const auto bytes = file.read(SelectionLimit + 1);
	if (bytes.size() > SelectionLimit || file.error() != QFileDevice::NoError) return false;
	const auto root = QJsonDocument::fromJson(bytes).object();
	if (!fields(root, {"schema", "format", "provider", "account", "selected", "version", "references"}) || root.value("schema") != QJsonValue(1) ||
	    string(root, "format") != "neo-workspace-selection-development-2" || string(root, "provider") != provider || string(root, "account") != account ||
	    !root.value("references").isObject() || !hash(string(root, "selected")) || !uuid(string(root, "version"))) return false;
	const auto known = root.value("references").toObject();
	if (known.isEmpty() || known.size() > WorkspaceLimit || !known.contains(string(root, "selected"))) return false;
	for (auto it = known.begin(); it != known.end(); ++it) {
		CloudWorkspaceReference reference; reference.value = it.value().toObject();
		if (!reference.matches(provider, account) || reference.key() != it.key()) return false;
	}
	references = known; selectedKey = string(root, "selected"); choiceVersion = string(root, "version"); return true;
}
bool CloudWorkspaceSelection::inspect() { return !fileName.isEmpty() && readUnlocked(); }
bool CloudWorkspaceSelection::load()
{
	selectedKey.clear(); choiceVersion.clear(); references = {};
	if (fileName.isEmpty()) return false;
	if (!QFileInfo(QFileInfo(fileName).absolutePath()).exists()) return readUnlocked();
	QLockFile lock(fileName + ".lock"); return lock.tryLock(0) && readUnlocked();
}
bool CloudWorkspaceSelection::select(const QString &expectedVersion, const CloudWorkspaceReference &reference)
{
	if (fileName.isEmpty() || !reference.matches(provider, account) || (!expectedVersion.isEmpty() && !uuid(expectedVersion)) ||
	    !QDir().mkpath(QFileInfo(fileName).absolutePath())) return false;
	QLockFile lock(fileName + ".lock"); if (!lock.tryLock(0) || !readUnlocked()) return false;
	const auto key = reference.key();
	if (selectedKey == key) return true; // Reconcile an already completed local choice without rewriting it.
	if (choiceVersion != expectedVersion || (!references.contains(key) && references.size() == WorkspaceLimit)) return false;
	auto next = references; next[key] = reference.value;
	const auto nextVersion = QUuid::createUuid().toString(QUuid::WithoutBraces);
	const auto bytes = QJsonDocument(QJsonObject{{"schema", 1}, {"format", "neo-workspace-selection-development-2"}, {"provider", provider},
		{"account", account}, {"selected", key}, {"version", nextVersion}, {"references", next}}).toJson(QJsonDocument::Compact);
	if (bytes.size() > SelectionLimit) return false;
	QSaveFile file(fileName);
	if (!file.open(QIODevice::WriteOnly) || file.write(bytes) != bytes.size() || !file.commit()) return false;
	return readUnlocked() && selectedKey == key && choiceVersion == nextVersion && references == next;
}
