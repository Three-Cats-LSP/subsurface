// SPDX-License-Identifier: GPL-2.0
#include "cloudsyncworkspacecopy.h"
#include "cloudsyncmanifest.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLockFile>
#include <QSaveFile>

namespace {
QString text(const QJsonObject &o, const char *key) { return o.value(QLatin1String(key)).toString(); }
bool revision(const QString &v) { return !v.isEmpty() && v.size() <= 512 && !v.contains(QChar::Null) && !v.contains('\r') && !v.contains('\n'); }
const QStringList States = {"prepared", "registering", "registered", "submitted", "identified", "verified", "uploaded", "committed"};
}
CloudWorkspaceCopyIntent::CloudWorkspaceCopyIntent(const QString &file, const CloudWorkspaceReference &workspace, const CloudWorkspaceCopySource &from,
	const QString &ticket, const QString &parent) : fileName(file), reference(workspace), source(from)
{
	if (reference.valid() && source.matches(text(reference.value, "provider"), text(reference.value, "account")) && CloudHistoryRecord::validEpoch(ticket) &&
	    (parent.isEmpty() || CloudHistoryRecord::validEpoch(parent)) && ticket != parent)
		scope = {{"schema", 1}, {"format", "neo-workspace-copy-intent-development-1"}, {"workspace", reference.value}, {"source_sha256", source.key()}, {"ticket", ticket}, {"parent", parent}};
}
QString CloudWorkspaceCopyIntent::state() const { return text(saved, "state"); }
QString CloudWorkspaceCopyIntent::id() const { return text(saved, "object"); }
QString CloudWorkspaceCopyIntent::version() const { return text(saved, "version"); }
QString CloudWorkspaceCopyIntent::ticket() const { return text(scope, "ticket"); }
QString CloudWorkspaceCopyIntent::parent() const { return text(scope, "parent"); }
bool CloudWorkspaceCopyIntent::read()
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
	     object == text(reference.value, "root_id") || object == text(source.value.value("payload").toObject(), "id") || object == text(source.value.value("manifest").toObject(), "id"))) ||
	    (phase >= 5 && !revision(version))) return false;
	saved = body; return true;
}
bool CloudWorkspaceCopyIntent::write(const QJsonObject &next)
{
	const auto bytes = QJsonDocument(next).toJson(QJsonDocument::Compact); QSaveFile file(fileName);
	return bytes.size() <= 8192 && file.open(QIODevice::WriteOnly) && file.write(bytes) == bytes.size() && file.commit() && read() && saved == next;
}
bool CloudWorkspaceCopyIntent::prepare()
{
	if (fileName.isEmpty() || scope.isEmpty() || !QDir().mkpath(QFileInfo(fileName).absolutePath())) return false;
	QLockFile lock(fileName + ".lock"); if (!lock.tryLock(0) || !read()) return false;
	if (!saved.isEmpty()) return true;
	auto next = scope; next["state"] = "prepared"; next["object"] = ""; next["version"] = ""; return write(next);
}
bool CloudWorkspaceCopyIntent::inspect() { return !fileName.isEmpty() && read() && !saved.isEmpty(); }
bool CloudWorkspaceCopyIntent::reload()
{
	if (fileName.isEmpty() || scope.isEmpty()) return false;
	QLockFile lock(fileName + ".lock"); return lock.tryLock(0) && read() && !saved.isEmpty();
}
bool CloudWorkspaceCopyIntent::advance(const QString &from, const QString &to, const QString &object, const QString &version, bool once)
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
	    object == text(reference.value, "root_id") || object == text(source.value.value("payload").toObject(), "id") || object == text(source.value.value("manifest").toObject(), "id"))) return false;
	if (to == "verified" && !revision(version)) return false;
	return write(next);
}
bool CloudWorkspaceCopyIntent::beginRegistration() { return advance("prepared", "registering"); }
bool CloudWorkspaceCopyIntent::registerTicket() { return advance("registering", "registered"); }
bool CloudWorkspaceCopyIntent::submit() { return advance("registered", "submitted", {}, {}, true); }
bool CloudWorkspaceCopyIntent::identify(const QString &id) { return advance("submitted", "identified", id); }
bool CloudWorkspaceCopyIntent::verify(const QString &version) { return advance("identified", "verified", {}, version); }
bool CloudWorkspaceCopyIntent::upload() { return advance("verified", "uploaded"); }
bool CloudWorkspaceCopyIntent::commit() { return advance("uploaded", "committed"); }

CloudWorkspaceCopy::CloudWorkspaceCopy(QNetworkAccessManager *network, QObject *parent) : QObject(parent), files(network, this), rootReader(network, this), archiver(network, this), sourceReview(network, this)
{
	connect(&rootReader, &CloudWorkspaceOpen::finished, this, &CloudWorkspaceCopy::receivedRoot);
	connect(&archiver, &CloudWorkspaceArchive::finished, this, &CloudWorkspaceCopy::receivedRoot);
	connect(&archiver, &CloudWorkspaceArchive::failed, this, [this](const QString &, bool rejected) { fail(rejected); });
	connect(&rootReader, &CloudWorkspaceOpen::failed, this, [this](const QString &, bool rejected) { fail(rejected); });
	files.setStrictReads(true); files.setResponseLimit(64 * 1024 * 1024); deadline.setSingleShot(true); deadline.setInterval(600000);
	startupRetry.setSingleShot(true); connect(&startupRetry, &QTimer::timeout, this, &CloudWorkspaceCopy::begin);
	connect(&deadline, &QTimer::timeout, this, [this] { fail(); });
	connect(&files, &CloudSyncFileStore::versionedDownloadFinished, this,
		[this](CloudSyncProviderType, const QString &name, const QByteArray &bytes, const QString &version, const QString &id) { received(name, bytes, version, id); });
	connect(&files, &CloudSyncFileStore::workspacePayloadCreated, this, [this](CloudSyncProviderType type, const QString &name, const QString &id) {
		if (step != Step::Upload) return;
		// Retain the acknowledged identity even if account/choice just changed.
		if (type != provider || name != reference.payloadName(intent->ticket()) || !intent->identify(id)) { fail(); return; }
		if (current()) { step = Step::Payload; files.setResponseLimit(64 * 1024 * 1024); files.downloadIdentity(provider, token, name, id); }
	});
	connect(&files, &CloudSyncFileStore::uploadFinished, this, [this](CloudSyncProviderType, const QString &) { if (step == Step::WriteRoot) readRoot(Step::ConfirmRoot); });
	connect(&files, &CloudSyncFileStore::writeConflict, this, [this](CloudSyncProviderType, const QString &) { if (step == Step::WriteRoot) readRoot(Step::ConfirmRoot); else fail(); });
	connect(&files, &CloudSyncFileStore::operationError, this, [this](CloudSyncProviderType, const QString &, const QString &, bool rejected) {
		if (step == Step::WriteRoot && !rejected) readRoot(Step::ConfirmRoot); else fail(rejected);
	});
	connect(&files, &CloudSyncFileStore::fileNotFound, this, [this](CloudSyncProviderType, const QString &) { fail(); });
	connect(&sourceReview, &CloudWorkspaceSourceReview::failed, this, [this](const QString &, bool rejected) { fail(rejected); });
	connect(&sourceReview, &CloudWorkspaceSourceReview::finished, this, [this](const QJsonObject &record, const QByteArray &body) {
		if (!current()) return;
		CloudWorkspaceCopySource observed; observed.value = record;
		if (step == Step::SourceAfter) {
			const auto result = root.value; const auto version = rootVersion, ticket = resultTicket; const bool changed = observed.key() != source.key();
			cancel(); emit finished(result, version, ticket, changed); return;
		}
		if (observed.key() != source.key() || !source.accepts(body)) { fail(); return; }
		if (step == Step::SourceBefore) { payload = body; continueCopy(); }
		else if (step == Step::SourceBeforePublish && body == payload) {
			step = Step::PayloadBeforePublish; files.setResponseLimit(64 * 1024 * 1024);
			files.downloadExact(provider, token, reference.payloadName(intent->ticket()), intent->id(), intent->version());
		}
		else fail();
	});
}
CloudWorkspaceCopy::~CloudWorkspaceCopy() { cancel(); }
void CloudWorkspaceCopy::cancel()
{
	step = Step::Idle; deadline.stop(); startupRetry.stop(); startupWait.invalidate(); files.cancel(provider); rootReader.cancel(); archiver.cancel(); sourceReview.cancel(); intent.reset(); reference.value = {}; source.value = {}; root.value = {}; pending.value = {};
	token.clear(); choiceFile.clear(); choiceVersion.clear(); checkpointFile.clear(); rootVersion.clear(); resultTicket.clear(); payload.clear(); scopeCurrent = {}; continuation = {};
	initialIntentFile.clear(); initialProofFile.clear(); initialTicket.clear(); initialParent.clear();
}
void CloudWorkspaceCopy::fail(bool rejected)
{
	if (step == Step::Idle) return;
	cancel(); emit failed(tr("The cloud copy could not be fully confirmed. All source files, upload attempts and your local dive log were kept. Review the workspace before retrying."), rejected);
}
bool CloudWorkspaceCopy::current()
{
	if (step == Step::Idle) return false;
	CloudWorkspaceSelection choice(choiceFile, text(reference.value, "provider"), text(reference.value, "account"));
	if (!scopeCurrent || !scopeCurrent()) { fail(); return false; }
	if (!choice.load()) { if (step == Step::Starting) retryStartupOrFail(choiceFile); else fail(); return false; }
	if (choice.version() != choiceVersion || choice.selected().value != reference.value) { fail(); return false; }
	return true;
}
void CloudWorkspaceCopy::start(const QString &accessToken, const CloudWorkspaceReference &workspace, const CloudWorkspaceCopySource &from,
	const QString &choice, const QString &choiceVersion, const QString &checkpoint, const QString &intentFile, const QString &proofFile,
	const QString &ticket, const QString &parent, const std::function<bool()> &guard)
{
	cancel(); reference = workspace; source = from; token = accessToken; choiceFile = choice; this->choiceVersion = choiceVersion; checkpointFile = checkpoint; scopeCurrent = guard;
	provider = text(reference.value, "provider") == "google-drive" ? CloudSyncProviderType::GoogleDrive : CloudSyncProviderType::Dropbox; step = Step::Starting;
	initialIntentFile = intentFile; initialProofFile = proofFile; initialTicket = ticket; initialParent = parent;
	if (token.isEmpty() || !reference.valid() || !source.matches(text(reference.value, "provider"), text(reference.value, "account")) ||
	    !CloudHistoryRecord::validEpoch(choiceVersion) || !scopeCurrent || !scopeCurrent()) { fail(); return; }
	startupWait.start(); begin();
}
void CloudWorkspaceCopy::retryStartupOrFail(const QString &path)
{
	// A process can die between creating a lock file and writing its PID.
	// Keep the normal lock fence; let QLockFile's own stale/native-lock checks
	// resolve it. Never force-remove a file or wait for a known live owner.
	QLockFile lock(path + ".lock"); const QFileInfo info(lock.fileName()); qint64 pid = 0; QString host, application;
	if (startupWait.isValid() && !startupWait.hasExpired(35000) && info.isFile() && !info.isSymLink() && !lock.getLockInfo(&pid, &host, &application)) {
		startupRetry.start(150); return;
	}
	fail();
}
void CloudWorkspaceCopy::begin()
{
	if (step != Step::Starting) return;
	if (!scopeCurrent || !scopeCurrent()) { fail(); return; }
	CloudWorkspaceSelection choice(choiceFile, text(reference.value, "provider"), text(reference.value, "account"));
	if (!choice.load()) { retryStartupOrFail(choiceFile); return; }
	if (choice.version() != choiceVersion || choice.selected().value != reference.value) { fail(); return; }
	CloudWorkspaceAuthority authority(checkpointFile, reference);
	if (!authority.load()) { retryStartupOrFail(checkpointFile); return; }
	if (!authority.record().valid()) { fail(); return; }
	const bool existingIntent = QFileInfo::exists(initialIntentFile);
	if (existingIntent && CloudWorkspaceCopySource::load(initialProofFile).json() != source.json()) { fail(); return; }
	if (!source.preserve(initialProofFile)) { retryStartupOrFail(initialProofFile); return; }
	intent = std::make_unique<CloudWorkspaceCopyIntent>(initialIntentFile, reference, source, initialTicket, initialParent);
	if (!intent->prepare()) { retryStartupOrFail(initialIntentFile); return; }
	if (intent->state() == "submitted") { fail(); return; }
	startupRetry.stop(); startupWait.invalidate();
	deadline.start(); readRoot(Step::InitialRoot);
}
void CloudWorkspaceCopy::readRoot(Step next)
{
	if (!current()) return;
	CloudWorkspaceAuthority authority(checkpointFile, reference);
	if (!authority.load() || !authority.record().valid()) { fail(); return; }
	step = next;
	rootReader.start(token, text(reference.value, "provider"), text(reference.value, "account"), reference, scopeCurrent, authority.record());
}
void CloudWorkspaceCopy::received(const QString &name, const QByteArray &bytes, const QString &version, const QString &id)
{
	if (!current() || !intent->reload()) { fail(); return; }
	if (step == Step::PayloadBeforePublish || step == Step::PayloadAfter) {
		const auto t = root.ticket(step == Step::PayloadAfter ? resultTicket : intent->ticket());
		if (name != text(t, "name") || id != text(t, "object") || version != text(t, "version") || !source.accepts(bytes) ||
		    (step == Step::PayloadBeforePublish && bytes != payload)) { fail(); return; }
		if (step == Step::PayloadBeforePublish) readRoot(Step::PublishRoot);
		else { step = Step::SourceAfter; sourceReview.start(token, text(reference.value, "provider"), text(reference.value, "account"), [this] { return current(); }); }
		return;
	}
	if (step == Step::Payload || step == Step::ExistingPayload) {
		const auto t = root.ticket(resultTicket.isEmpty() ? intent->ticket() : resultTicket);
		if (name != text(t, "name") || !source.accepts(bytes) || (step == Step::Payload && (bytes != payload || id != intent->id())) ||
		    (step == Step::ExistingPayload && (id != text(t, "object") || version != text(t, "version")))) { fail(); return; }
		if (step == Step::ExistingPayload) { complete(); return; }
		if ((intent->state() == "identified" && !intent->verify(version)) || intent->version() != version) { fail(); return; }
		if (text(t, "state") == "uploaded") {
			if (text(t, "object") != id || text(t, "version") != version) { fail(); return; }
			uploaded(); return;
		}
		auto next = root; if (!next.completeUpload(intent->ticket(), id, version)) { fail(); return; }
		writeRoot(next, [this] { uploaded(); }); return;
	}
	fail(); // Only payload callbacks use this transport; roots use rootReader.
}
void CloudWorkspaceCopy::receivedRoot(const QJsonObject &value, const QString &version)
{
	if (!current()) { fail(); return; }
	CloudWorkspaceRecord observed; observed.value = value;
	CloudWorkspaceAuthority authority(checkpointFile, reference);
	if (!observed.valid() || observed.reference().value != reference.value || (step == Step::ConfirmRoot && !observed.follows(pending)) ||
	    !authority.observe(observed, version, true) || !current()) { fail(); return; }
	root = observed; rootVersion = version;
	if (root.needsArchive()) {
		const auto archiveIntent = initialIntentFile + ".history-" + CloudSyncManifest::sha256(root.json() + version.toUtf8()) + ".json";
		archiver.start(token, root, version, choiceFile, choiceVersion, checkpointFile, archiveIntent, scopeCurrent); return;
	}
	if (step == Step::ConfirmRoot) { pending.value = {}; auto next = std::move(continuation); continuation = {}; next(); return; }
	if (step == Step::PublishRoot) { publish(); return; }
	if (step != Step::InitialRoot) { fail(); return; }
	// An already committed exact source is idempotent, but its payload must
	// still be checked. This never adopts a source/target merely by filename.
	for (const auto &entry : root.value.value("journal").toObject().value("history").toArray()) {
		const auto key = entry.toString(); if (root.copySource(key).key() != source.key()) continue;
		resultTicket = key; const auto t = root.ticket(key); step = Step::ExistingPayload;
		files.setResponseLimit(64 * 1024 * 1024);
		files.downloadExact(provider, token, text(t, "name"), text(t, "object"), text(t, "version")); return;
	}
	step = Step::SourceBefore; sourceReview.start(token, text(reference.value, "provider"), text(reference.value, "account"), [this] { return current(); });
}
void CloudWorkspaceCopy::writeRoot(const CloudWorkspaceRecord &next, const std::function<void()> &then)
{
	if (!current()) return;
	CloudWorkspaceAuthority authority(checkpointFile, reference);
	if (!next.follows(root) || !authority.observe(root, rootVersion, true) || !current()) { fail(); return; }
	pending = next; continuation = then; step = Step::WriteRoot;
	files.uploadIdentity(provider, token, reference.rootName(), next.json(), rootVersion, text(reference.value, "root_id"));
}
void CloudWorkspaceCopy::continueCopy()
{
	if (!current() || !intent->reload()) { fail(); return; }
	const auto t = root.ticket(intent->ticket());
	if (t.isEmpty()) {
		if ((intent->state() != "prepared" && intent->state() != "registering") || root.head() != intent->parent() || !intent->beginRegistration()) { fail(); return; }
		auto next = root; if (!next.registerCopy(intent->ticket(), intent->parent(), source)) { fail(); return; }
		writeRoot(next, [this] { prepareUpload(); }); return;
	}
	if (intent->state() == "prepared" || root.copySource(intent->ticket()).key() != source.key() || text(t, "parent") != intent->parent()) { fail(); return; }
	prepareUpload();
}
void CloudWorkspaceCopy::prepareUpload()
{
	if (!current() || !intent->reload() || root.head() != intent->parent() || root.copySource(intent->ticket()).key() != source.key()) { fail(); return; }
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
void CloudWorkspaceCopy::uploaded()
{
	if (!current() || !intent->upload()) { fail(); return; }
	step = Step::SourceBeforePublish; sourceReview.start(token, text(reference.value, "provider"), text(reference.value, "account"), [this] { return current(); });
}
void CloudWorkspaceCopy::publish()
{
	if (!current() || !intent->reload()) { fail(); return; }
	const auto t = root.ticket(intent->ticket());
	if (root.copySource(intent->ticket()).key() != source.key() || text(t, "object") != intent->id() || text(t, "version") != intent->version()) { fail(); return; }
	if (text(t, "state") == "committed") { resultTicket = intent->ticket(); complete(); return; }
	if (intent->state() != "uploaded" || root.head() != intent->parent()) { fail(); return; }
	auto next = root; if (!next.publish(intent->ticket(), intent->parent())) { fail(); return; }
	writeRoot(next, [this] { resultTicket = intent->ticket(); complete(); });
}
void CloudWorkspaceCopy::complete()
{
	if (!current()) return;
	if (resultTicket == intent->ticket()) {
		// A committed root may be observed after termination before the final
		// local phase. Never fabricate a missing acknowledged object identity.
		const auto t = root.ticket(resultTicket);
		if (intent->state() != "prepared" && (intent->id() != text(t, "object") || (!intent->version().isEmpty() && intent->version() != text(t, "version")))) { fail(); return; }
		if (intent->state() == "identified" && !intent->verify(text(t, "version"))) { fail(); return; }
		if (intent->state() == "verified" && !intent->upload()) { fail(); return; }
		if (intent->state() == "uploaded" && !intent->commit()) { fail(); return; }
		if (intent->state() != "committed" && intent->state() != "prepared") { fail(); return; }
	}
	const auto t = root.ticket(resultTicket); step = Step::PayloadAfter; files.setResponseLimit(64 * 1024 * 1024);
	files.downloadExact(provider, token, text(t, "name"), text(t, "object"), text(t, "version"));
}
