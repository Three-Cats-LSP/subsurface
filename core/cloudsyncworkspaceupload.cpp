// SPDX-License-Identifier: GPL-2.0
#include "cloudsyncworkspaceupload.h"
#include "cloudsyncmanifest.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLockFile>
#include <QSaveFile>
#include <QRegularExpression>

namespace {
QString text(const QJsonObject &o, const char *key) { return o.value(QLatin1String(key)).toString(); }
bool revision(const QString &v) { return !v.isEmpty() && v.size() <= 512 && !v.contains(QChar::Null) && !v.contains('\r') && !v.contains('\n'); }
const QStringList States = {"prepared", "registering", "registered", "submitted", "identified", "verified", "uploaded", "committed"};
}
CloudWorkspaceUploadIntent::CloudWorkspaceUploadIntent(const QString &file, const CloudWorkspaceReference &workspace, const QString &hash, qint64 bytes,
	const QString &ticket, const QString &parent) : fileName(file), reference(workspace)
{
	if (reference.valid() && QRegularExpression("^[a-f0-9]{64}$").match(hash).hasMatch() && bytes >= 1 && bytes <= 64 * 1024 * 1024 &&
		CloudHistoryRecord::validEpoch(ticket) && (parent.isEmpty() || CloudHistoryRecord::validEpoch(parent)) && ticket != parent)
		scope = {{"schema", 1}, {"format", "neo-workspace-upload-intent-development-1"}, {"workspace", reference.value},
		    {"payload_sha256", hash}, {"payload_bytes", double(bytes)}, {"ticket", ticket}, {"parent", parent}};
}
QString CloudWorkspaceUploadIntent::state() const { return text(saved, "state"); }
QString CloudWorkspaceUploadIntent::id() const { return text(saved, "object"); }
QString CloudWorkspaceUploadIntent::version() const { return text(saved, "version"); }
QString CloudWorkspaceUploadIntent::ticket() const { return text(scope, "ticket"); }
QString CloudWorkspaceUploadIntent::parent() const { return text(scope, "parent"); }
bool CloudWorkspaceUploadIntent::read()
{
	saved = {}; if (scope.isEmpty()) return false;
	const QFileInfo info(fileName); if (!info.exists()) return !info.isSymLink();
	if (!info.isFile() || info.isSymLink() || info.size() > 8192) return false;
	QFile file(fileName); if (!file.open(QIODevice::ReadOnly)) return false;
	const auto bytes = file.read(8193); if (bytes.size() > 8192 || file.error() != QFileDevice::NoError) return false;
	const auto body = QJsonDocument::fromJson(bytes).object(); auto fixed = body;
	for (const auto &key : {"state", "object", "version"}) { if (!body.value(QLatin1String(key)).isString()) return false; fixed.remove(QLatin1String(key)); }
	if (fixed != scope) return false;
	const auto phase = States.indexOf(text(body, "state")); const auto object = text(body, "object"), version = text(body, "version");
	if (phase < 0 || (phase < 4 && !object.isEmpty()) || (phase < 5 && !version.isEmpty()) ||
	    (phase >= 4 && (!CloudWorkspaceReference::create(text(reference.value, "provider"), text(reference.value, "account"), text(reference.value, "epoch"), object).valid() ||
	     object == text(reference.value, "root_id"))) ||
	    (phase >= 5 && !revision(version))) return false;
	saved = body; return true;
}
bool CloudWorkspaceUploadIntent::write(const QJsonObject &next)
{
	const auto bytes = QJsonDocument(next).toJson(QJsonDocument::Compact); QSaveFile file(fileName);
	return bytes.size() <= 8192 && file.open(QIODevice::WriteOnly) && file.write(bytes) == bytes.size() && file.commit() && read() && saved == next;
}
bool CloudWorkspaceUploadIntent::prepare()
{
	if (fileName.isEmpty() || scope.isEmpty() || !QDir().mkpath(QFileInfo(fileName).absolutePath())) return false;
	QLockFile lock(fileName + ".lock"); if (!lock.tryLock(0) || !read()) return false;
	if (!saved.isEmpty()) return true;
	auto next = scope; next["state"] = "prepared"; next["object"] = ""; next["version"] = ""; return write(next);
}
bool CloudWorkspaceUploadIntent::inspect() { return !fileName.isEmpty() && read() && !saved.isEmpty(); }
bool CloudWorkspaceUploadIntent::reload()
{
	if (fileName.isEmpty() || scope.isEmpty()) return false;
	QLockFile lock(fileName + ".lock"); return lock.tryLock(0) && read() && !saved.isEmpty();
}
bool CloudWorkspaceUploadIntent::advance(const QString &from, const QString &to, const QString &object, const QString &version, bool once)
{
	if (fileName.isEmpty() || scope.isEmpty()) return false;
	QLockFile lock(fileName + ".lock"); if (!lock.tryLock(0) || !read() || saved.isEmpty()) return false;
	if (!once && state() == to) return (object.isEmpty() || object == id()) && (version.isEmpty() || version == this->version());
	if (state() != from) return false;
	auto next = saved; next["state"] = to;
	if (!object.isEmpty()) next["object"] = object;
	if (!version.isEmpty()) next["version"] = version;
	// Validate a candidate without changing the existing file on invalid input.
	if (to == "identified" && (!CloudWorkspaceReference::create(text(reference.value, "provider"), text(reference.value, "account"), text(reference.value, "epoch"), object).valid() ||
	    object == text(reference.value, "root_id"))) return false;
	if (to == "verified" && !revision(version)) return false;
	return write(next);
}
bool CloudWorkspaceUploadIntent::beginRegistration() { return advance("prepared", "registering"); }
bool CloudWorkspaceUploadIntent::registerTicket() { return advance("registering", "registered"); }
bool CloudWorkspaceUploadIntent::submit() { return advance("registered", "submitted", {}, {}, true); }
bool CloudWorkspaceUploadIntent::identify(const QString &id) { return advance("submitted", "identified", id); }
bool CloudWorkspaceUploadIntent::verify(const QString &version) { return advance("identified", "verified", {}, version); }
bool CloudWorkspaceUploadIntent::upload() { return advance("verified", "uploaded"); }
bool CloudWorkspaceUploadIntent::commit() { return advance("uploaded", "committed"); }

CloudWorkspaceUpload::CloudWorkspaceUpload(QNetworkAccessManager *network, QObject *parent) : QObject(parent), files(network, this)
{
	files.setStrictReads(true); deadline.setSingleShot(true); deadline.setInterval(600000); startupRetry.setSingleShot(true);
	connect(&startupRetry, &QTimer::timeout, this, &CloudWorkspaceUpload::begin);
	connect(&deadline, &QTimer::timeout, this, [this] { fail(); });
	connect(&files, &CloudSyncFileStore::versionedDownloadFinished, this,
		[this](CloudSyncProviderType, const QString &name, const QByteArray &bytes, const QString &version, const QString &id) { received(name, bytes, version, id); });
	connect(&files, &CloudSyncFileStore::workspacePayloadCreated, this, [this](CloudSyncProviderType type, const QString &name, const QString &id) {
		if (step != Step::Upload) return;
		// Preserve the direct acknowledgement even if scope changed meanwhile.
		if (type != provider || name != reference.payloadName(intent->ticket()) || !intent->identify(id)) { fail(); return; }
		if (current()) { step = Step::Payload; files.setResponseLimit(64 * 1024 * 1024); files.downloadIdentity(provider, token, name, id); }
	});
	connect(&files, &CloudSyncFileStore::uploadFinished, this, [this](CloudSyncProviderType, const QString &) { if (step == Step::WriteRoot) readRoot(Step::ConfirmRoot); });
	connect(&files, &CloudSyncFileStore::writeConflict, this, [this](CloudSyncProviderType, const QString &) { if (step == Step::WriteRoot) readRoot(Step::ConfirmRoot); else fail(); });
	connect(&files, &CloudSyncFileStore::operationError, this, [this](CloudSyncProviderType, const QString &, const QString &, bool rejected) {
		if (step == Step::WriteRoot && !rejected) readRoot(Step::ConfirmRoot); else fail(rejected);
	});
	connect(&files, &CloudSyncFileStore::fileNotFound, this, [this](CloudSyncProviderType, const QString &) { fail(); });
}
CloudWorkspaceUpload::~CloudWorkspaceUpload() { cancel(); }
void CloudWorkspaceUpload::cancel()
{
	step = Step::Idle; deadline.stop(); startupRetry.stop(); startupWait.invalidate(); files.cancel(provider); intent.reset(); reference.value = {}; root.value = {}; pending.value = {};
	token.clear(); choiceFile.clear(); choiceVersion.clear(); checkpointFile.clear(); rootVersion.clear(); payloadHash.clear(); payload.clear(); scopeCurrent = {}; continuation = {};
	initialIntentFile.clear(); initialTicket.clear(); initialParent.clear();
}
void CloudWorkspaceUpload::fail(bool rejected)
{
	if (step == Step::Idle) return;
	cancel(); emit failed(tr("The workspace version could not be fully confirmed. All upload attempts and your local dive log were kept. Review the workspace before retrying."), rejected);
}
bool CloudWorkspaceUpload::current()
{
	if (step == Step::Idle) return false;
	CloudWorkspaceSelection choice(choiceFile, text(reference.value, "provider"), text(reference.value, "account"));
	if (!scopeCurrent || !scopeCurrent()) { fail(); return false; }
	if (!choice.load()) { if (step == Step::Starting) retryStartupOrFail(choiceFile); else fail(); return false; }
	if (choice.version() != choiceVersion || choice.selected().value != reference.value) { fail(); return false; }
	return true;
}
bool CloudWorkspaceUpload::accepts(const QByteArray &bytes) const
{
	return bytes == payload && CloudSyncManifest::sha256(bytes) == payloadHash && CloudHistoryRecord::validXml(bytes);
}
bool CloudWorkspaceUpload::matchesTicket() const
{
	const auto t = root.ticket(intent->ticket());
	return !t.isEmpty() && text(t, "name") == reference.payloadName(intent->ticket()) && text(t, "parent") == intent->parent() &&
		text(t, "sha256") == payloadHash && t.value("bytes").toDouble() == payload.size() && text(t, "restore_source").isEmpty() &&
		text(t, "reservation").isEmpty() && !root.copySource(intent->ticket()).valid();
}
void CloudWorkspaceUpload::start(const QString &accessToken, const CloudWorkspaceReference &workspace, const QByteArray &bytes,
	const QString &choice, const QString &expectedChoice, const QString &checkpoint, const QString &intentFile,
	const QString &ticket, const QString &parent, const std::function<bool()> &guard)
{
	cancel(); reference = workspace; token = accessToken; payload = bytes; payloadHash = CloudSyncManifest::sha256(bytes);
	choiceFile = choice; choiceVersion = expectedChoice; checkpointFile = checkpoint; scopeCurrent = guard;
	initialIntentFile = intentFile; initialTicket = ticket; initialParent = parent;
	provider = text(reference.value, "provider") == "google-drive" ? CloudSyncProviderType::GoogleDrive : CloudSyncProviderType::Dropbox; step = Step::Starting;
	if (token.isEmpty() || !reference.valid() || payload.isEmpty() || payload.size() > 64 * 1024 * 1024 || !CloudHistoryRecord::validXml(payload) ||
	    !CloudHistoryRecord::validEpoch(choiceVersion) || !scopeCurrent || !scopeCurrent()) { fail(); return; }
	startupWait.start(); begin();
}
void CloudWorkspaceUpload::retryStartupOrFail(const QString &path)
{
	QLockFile lock(path + ".lock"); const QFileInfo info(lock.fileName()); qint64 pid = 0; QString host, application;
	// Preserve normal QLockFile ownership/stale checks, including native locks.
	if (startupWait.isValid() && !startupWait.hasExpired(35000) && info.isFile() && !info.isSymLink() && !lock.getLockInfo(&pid, &host, &application)) {
		startupRetry.start(150); return;
	}
	fail();
}
void CloudWorkspaceUpload::begin()
{
	if (step != Step::Starting || !current()) return;
	CloudWorkspaceAuthority authority(checkpointFile, reference);
	if (!authority.load()) { retryStartupOrFail(checkpointFile); return; }
	if (!authority.record().valid()) { fail(); return; }
	intent = std::make_unique<CloudWorkspaceUploadIntent>(initialIntentFile, reference, payloadHash, payload.size(), initialTicket, initialParent);
	if (!intent->prepare()) { retryStartupOrFail(initialIntentFile); return; }
	if (intent->state() == "submitted") { fail(); return; }
	startupRetry.stop(); startupWait.invalidate(); deadline.start(); readRoot(Step::InitialRoot);
}
void CloudWorkspaceUpload::readRoot(Step next)
{
	if (!current()) return;
	step = next; files.setResponseLimit(1024 * 1024 + 4096); files.downloadIdentity(provider, token, reference.rootName(), text(reference.value, "root_id"));
}
void CloudWorkspaceUpload::received(const QString &name, const QByteArray &bytes, const QString &version, const QString &id)
{
	if (!current() || !intent->reload()) { fail(); return; }
	if (step == Step::Payload || step == Step::PayloadBeforePublish || step == Step::PayloadAfter) {
		const auto t = root.ticket(intent->ticket());
		if (name != reference.payloadName(intent->ticket()) || id != intent->id() || !accepts(bytes)) { fail(); return; }
		if (step != Step::Payload) {
			if (version != intent->version() || id != text(t, "object") || version != text(t, "version")) { fail(); return; }
			readRoot(step == Step::PayloadAfter ? Step::FinalRoot : Step::PublishRoot); return;
		}
		if ((intent->state() == "identified" && !intent->verify(version)) || intent->version() != version) { fail(); return; }
		if (text(t, "state") == "uploaded") {
			if (text(t, "object") != id || text(t, "version") != version) { fail(); return; }
			uploaded(); return;
		}
		auto next = root; if (!next.completeUpload(intent->ticket(), id, version)) { fail(); return; }
		writeRoot(next, [this] { uploaded(); }); return;
	}
	if (name != reference.rootName() || id != text(reference.value, "root_id") || !revision(version)) { fail(); return; }
	const auto observed = CloudWorkspaceRecord::parse(bytes); CloudWorkspaceAuthority authority(checkpointFile, reference);
	if (!observed.valid() || observed.reference().value != reference.value || (step == Step::ConfirmRoot && !observed.follows(pending)) ||
	    !authority.observe(observed, version, true) || !current()) { fail(); return; }
	root = observed; rootVersion = version;
	if (step == Step::ConfirmRoot) { pending.value = {}; auto next = std::move(continuation); continuation = {}; next(); return; }
	if (step == Step::PublishRoot) { publish(); return; }
	if (step == Step::FinalRoot) {
		const auto t = root.ticket(intent->ticket());
		if (!matchesTicket() || text(t, "state") != "committed" || text(t, "object") != intent->id() || text(t, "version") != intent->version()) { fail(); return; }
		const auto result = root.value; const auto observedVersion = rootVersion, ticket = intent->ticket(); const bool headMatches = root.head() == ticket;
		cancel(); emit finished(result, observedVersion, ticket, headMatches); return;
	}
	if (step != Step::InitialRoot) { fail(); return; }
	continueUpload();
}
void CloudWorkspaceUpload::writeRoot(const CloudWorkspaceRecord &next, const std::function<void()> &then)
{
	if (!current()) return;
	CloudWorkspaceAuthority authority(checkpointFile, reference);
	if (!next.follows(root) || !authority.observe(root, rootVersion, true) || !current()) { fail(); return; }
	pending = next; continuation = then; step = Step::WriteRoot;
	files.uploadIdentity(provider, token, reference.rootName(), next.json(), rootVersion, text(reference.value, "root_id"));
}
void CloudWorkspaceUpload::continueUpload()
{
	if (!current() || !intent->reload()) { fail(); return; }
	const auto t = root.ticket(intent->ticket());
	if (t.isEmpty()) {
		if ((intent->state() != "prepared" && intent->state() != "registering") || root.head() != intent->parent() || !intent->beginRegistration()) { fail(); return; }
		auto next = root; if (!next.registerUpload(intent->ticket(), intent->parent(), payloadHash, payload.size())) { fail(); return; }
		writeRoot(next, [this] { prepareUpload(); }); return;
	}
	if (!matchesTicket() || intent->state() == "prepared") { fail(); return; }
	if (text(t, "state") == "committed") { complete(); return; }
	prepareUpload();
}
void CloudWorkspaceUpload::prepareUpload()
{
	if (!current() || !intent->reload() || !matchesTicket() || root.head() != intent->parent()) { fail(); return; }
	const auto t = root.ticket(intent->ticket());
	if (intent->state() == "registering" && !intent->registerTicket()) { fail(); return; }
	if (intent->state() == "registered") {
		if (text(t, "state") != "registered" || !intent->submit() || !current()) { fail(); return; }
		step = Step::Upload; files.setResponseLimit(8192); files.createWorkspacePayloadIdentity(provider, token, reference.payloadName(intent->ticket()), payload); return;
	}
	if (intent->state() != "identified" && intent->state() != "verified" && intent->state() != "uploaded") { fail(); return; }
	if (intent->state() == "uploaded" && text(t, "state") != "uploaded") { fail(); return; }
	step = Step::Payload; files.setResponseLimit(64 * 1024 * 1024); files.downloadIdentity(provider, token, reference.payloadName(intent->ticket()), intent->id());
}
void CloudWorkspaceUpload::uploaded()
{
	if (!current() || !intent->upload()) { fail(); return; }
	step = Step::PayloadBeforePublish; files.setResponseLimit(64 * 1024 * 1024);
	files.downloadExact(provider, token, reference.payloadName(intent->ticket()), intent->id(), intent->version());
}
void CloudWorkspaceUpload::publish()
{
	if (!current() || !intent->reload() || !matchesTicket()) { fail(); return; }
	const auto t = root.ticket(intent->ticket());
	if (text(t, "object") != intent->id() || text(t, "version") != intent->version()) { fail(); return; }
	if (text(t, "state") == "committed") { complete(); return; }
	if (intent->state() != "uploaded" || root.head() != intent->parent()) { fail(); return; }
	auto next = root; if (!next.publish(intent->ticket(), intent->parent())) { fail(); return; }
	writeRoot(next, [this] { complete(); });
}
void CloudWorkspaceUpload::complete()
{
	if (!current() || !intent->reload() || !matchesTicket()) { fail(); return; }
	const auto t = root.ticket(intent->ticket());
	if (text(t, "state") != "committed" || intent->id() != text(t, "object") || (!intent->version().isEmpty() && intent->version() != text(t, "version"))) { fail(); return; }
	if (intent->state() == "identified" && !intent->verify(text(t, "version"))) { fail(); return; }
	if (intent->state() == "verified" && !intent->upload()) { fail(); return; }
	if (intent->state() == "uploaded" && !intent->commit()) { fail(); return; }
	if (intent->state() != "committed") { fail(); return; }
	step = Step::PayloadAfter; files.setResponseLimit(64 * 1024 * 1024);
	files.downloadExact(provider, token, reference.payloadName(intent->ticket()), intent->id(), intent->version());
}

