// SPDX-License-Identifier: GPL-2.0
#include "cloudsyncworkspaceread.h"
#include "cloudsyncmanifest.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QLockFile>
#include <QRegularExpression>
#include <QSaveFile>

namespace {
QString text(const QJsonObject &o, const char *key) { return o.value(QLatin1String(key)).toString(); }
bool revision(const QString &v) { return !v.isEmpty() && v.size() <= 512 && !v.contains(QChar::Null) && !v.contains('\r') && !v.contains('\n'); }
const QStringList States = {"prepared", "reserving", "reserved", "verified", "releasing", "released"};
}
CloudWorkspaceReadIntent::CloudWorkspaceReadIntent(const QString &file, const CloudWorkspaceReference &ref, const QString &commit, const QString &reservation) :
	fileName(file), commit(commit), reference(ref)
{
	if (ref.valid() && CloudHistoryRecord::validEpoch(commit) && CloudHistoryRecord::validEpoch(reservation) && commit != reservation)
		scope = {{"schema", 1}, {"format", "neo-workspace-read-intent-development-1"}, {"workspace", ref.value}, {"commit", commit}, {"reservation", reservation}};
}
QString CloudWorkspaceReadIntent::state() const { return text(saved, "state"); }
QJsonObject CloudWorkspaceReadIntent::snapshot() const { return saved.value("snapshot").toObject(); }
bool CloudWorkspaceReadIntent::matches(const CloudWorkspaceReference &ref, const QString &id, const QString &reservation) const
{
	return !scope.isEmpty() && scope.value("workspace") == ref.value && text(scope, "commit") == id && text(scope, "reservation") == reservation;
}
bool CloudWorkspaceReadIntent::validSnapshot(const QJsonObject &s) const
{
	if (s.size() != 5 || text(s, "name") != reference.payloadName(commit) || text(s, "object") == text(reference.value, "root_id") ||
	    !CloudWorkspaceReference::create(text(reference.value, "provider"), text(reference.value, "account"), text(reference.value, "epoch"), text(s, "object")).valid() ||
	    !revision(text(s, "version")) || !QRegularExpression("^[a-f0-9]{64}$").match(text(s, "sha256")).hasMatch() || !s.value("bytes").isDouble()) return false;
	const auto size = s.value("bytes").toDouble(); return size >= 1 && size <= 64 * 1024 * 1024 && size == double(qint64(size));
}
bool CloudWorkspaceReadIntent::read()
{
	saved = {}; if (scope.isEmpty()) return false;
	const QFileInfo info(fileName); if (!info.exists()) return !info.isSymLink();
	if (!info.isFile() || info.isSymLink() || info.size() > 8192) return false;
	QFile file(fileName); if (!file.open(QIODevice::ReadOnly)) return false; const auto bytes = file.read(8193);
	if (file.error() != QFileDevice::NoError || bytes.size() > 8192) return false;
	const auto body = QJsonDocument::fromJson(bytes).object(); auto fixed = body; fixed.remove("state"); fixed.remove("snapshot");
	if (fixed != scope || !body.value("state").isString() || !body.value("snapshot").isObject() || !States.contains(text(body, "state"))) return false;
	const auto s = body.value("snapshot").toObject();
	if (text(body, "state") == "prepared" ? !s.isEmpty() : !validSnapshot(s)) return false;
	saved = body; return true;
}
bool CloudWorkspaceReadIntent::write(const QJsonObject &next)
{
	QSaveFile file(fileName); const auto bytes = QJsonDocument(next).toJson(QJsonDocument::Compact);
	return bytes.size() <= 8192 && file.open(QIODevice::WriteOnly) && file.write(bytes) == bytes.size() && file.commit() && read() && saved == next;
}
bool CloudWorkspaceReadIntent::prepare()
{
	if (fileName.isEmpty() || scope.isEmpty() || !QDir().mkpath(QFileInfo(fileName).absolutePath())) return false;
	QLockFile lock(fileName + ".lock"); if (!lock.tryLock(0) || !read()) return false;
	if (!saved.isEmpty()) return true;
	auto next = scope; next["state"] = "prepared"; next["snapshot"] = QJsonObject(); return write(next);
}
bool CloudWorkspaceReadIntent::inspect() { return !fileName.isEmpty() && read() && !saved.isEmpty(); }
bool CloudWorkspaceReadIntent::reload()
{
	if (fileName.isEmpty() || scope.isEmpty()) return false;
	QLockFile lock(fileName + ".lock"); return lock.tryLock(0) && read() && !saved.isEmpty();
}
bool CloudWorkspaceReadIntent::advance(const QStringList &from, const QString &to, const QJsonObject &pin)
{
	if (fileName.isEmpty() || scope.isEmpty() || (!pin.isEmpty() && !validSnapshot(pin))) return false;
	QLockFile lock(fileName + ".lock"); if (!lock.tryLock(0) || !read() || saved.isEmpty()) return false;
	if (state() == to) return pin.isEmpty() || pin == snapshot();
	if (!from.contains(state())) return false;
	auto next = saved; next["state"] = to; if (!pin.isEmpty()) next["snapshot"] = pin; return write(next);
}
bool CloudWorkspaceReadIntent::pin(const QJsonObject &s) { return validSnapshot(s) && advance({"prepared"}, "reserving", s); }
bool CloudWorkspaceReadIntent::markReserved() { return advance({"reserving"}, "reserved"); }
bool CloudWorkspaceReadIntent::markVerified() { return advance({"reserved"}, "verified"); }
bool CloudWorkspaceReadIntent::beginRelease() { return state() == "released" ? reload() && state() == "released" : advance({"reserving", "reserved", "verified"}, "releasing"); }
bool CloudWorkspaceReadIntent::markReleased() { return advance({"releasing"}, "released"); }

CloudWorkspaceRead::CloudWorkspaceRead(QNetworkAccessManager *network, QObject *parent) : QObject(parent), files(network, this)
{
	files.setStrictReads(true); deadline.setSingleShot(true); deadline.setInterval(600000); startupRetry.setSingleShot(true);
	connect(&startupRetry, &QTimer::timeout, this, &CloudWorkspaceRead::begin);
	connect(&deadline, &QTimer::timeout, this, [this] { fail(); });
	connect(&files, &CloudSyncFileStore::versionedDownloadFinished, this,
		[this](CloudSyncProviderType, const QString &name, const QByteArray &bytes, const QString &version, const QString &id) { received(name, bytes, version, id); });
	connect(&files, &CloudSyncFileStore::uploadFinished, this, [this](CloudSyncProviderType, const QString &) { if (step == Step::WriteRoot) readRoot(Step::ConfirmRoot); });
	connect(&files, &CloudSyncFileStore::writeConflict, this, [this](CloudSyncProviderType, const QString &) { if (step == Step::WriteRoot) readRoot(Step::ConfirmRoot); else fail(); });
	connect(&files, &CloudSyncFileStore::operationError, this, [this](CloudSyncProviderType, const QString &, const QString &, bool rejected) {
		if (step == Step::WriteRoot && !rejected) readRoot(Step::ConfirmRoot); else fail(rejected);
	});
	connect(&files, &CloudSyncFileStore::fileNotFound, this, [this](CloudSyncProviderType, const QString &) { fail(); });
}
CloudWorkspaceRead::~CloudWorkspaceRead() { cancel(); }
void CloudWorkspaceRead::cancel()
{
	step = Step::Idle; deadline.stop(); startupRetry.stop(); startupWait.invalidate(); files.cancel(provider); intent.reset();
	reference.value = {}; root.value = {}; pending.value = {}; token.clear(); commit.clear(); reservation.clear(); choiceFile.clear(); choiceVersion.clear(); checkpointFile.clear(); intentFile.clear(); rootVersion.clear(); payload.clear(); scopeCurrent = {};
}
void CloudWorkspaceRead::fail(bool rejected)
{
	if (step == Step::Idle) return;
	cancel(); emit failed(tr("The workspace read could not be fully confirmed. Your local log and all recovery reservations were kept. Review the workspace before retrying."), rejected);
}
bool CloudWorkspaceRead::current()
{
	if (step == Step::Idle) return false;
	if (!scopeCurrent || !scopeCurrent()) { fail(); return false; }
	CloudWorkspaceSelection choice(choiceFile, text(reference.value, "provider"), text(reference.value, "account"));
	if (!choice.load()) { if (step == Step::Starting) retryStartupOrFail(choiceFile); else fail(); return false; }
	if (choice.version() != choiceVersion || choice.selected().value != reference.value) { fail(); return false; }
	if (step != Step::Starting && (!intent || !intent->reload() || (action == Action::Read && (intent->state() == "releasing" || intent->state() == "released")))) { fail(); return false; }
	return true;
}
void CloudWorkspaceRead::start(Action requested, const QString &accessToken, const CloudWorkspaceReference &workspace, const QString &targetCommit, const QString &reservationId,
	const QString &choice, const QString &expectedChoice, const QString &checkpoint, const QString &file, const std::function<bool()> &guard)
{
	cancel(); action = requested; token = accessToken; reference = workspace; commit = targetCommit; reservation = reservationId;
	choiceFile = choice; choiceVersion = expectedChoice; checkpointFile = checkpoint; intentFile = file; scopeCurrent = guard;
	provider = text(reference.value, "provider") == "google-drive" ? CloudSyncProviderType::GoogleDrive : CloudSyncProviderType::Dropbox; step = Step::Starting;
	if ((action != Action::Read && action != Action::Release) || token.isEmpty() || !reference.valid() || !CloudHistoryRecord::validEpoch(commit) || !CloudHistoryRecord::validEpoch(reservation) || commit == reservation ||
	    !CloudHistoryRecord::validEpoch(choiceVersion) || !scopeCurrent || !scopeCurrent()) { fail(); return; }
	startupWait.start(); begin();
}
void CloudWorkspaceRead::retryStartupOrFail(const QString &path)
{
	QLockFile lock(path + ".lock"); const QFileInfo info(lock.fileName()); qint64 pid = 0; QString host, application;
	if (startupWait.isValid() && !startupWait.hasExpired(35000) && info.isFile() && !info.isSymLink() && !lock.getLockInfo(&pid, &host, &application)) { startupRetry.start(150); return; }
	fail();
}
void CloudWorkspaceRead::begin()
{
	if (step != Step::Starting || !current()) return;
	CloudWorkspaceAuthority authority(checkpointFile, reference);
	if (!authority.load()) { retryStartupOrFail(checkpointFile); return; }
	if (!authority.record().valid()) { fail(); return; }
	intent = std::make_unique<CloudWorkspaceReadIntent>(intentFile, reference, commit, reservation);
	if (!intent->prepare()) { retryStartupOrFail(intentFile); return; }
	if (action == Action::Read && (intent->state() == "releasing" || intent->state() == "released")) { fail(); return; }
	if (action == Action::Release && !intent->beginRelease()) { fail(); return; }
	startupRetry.stop(); startupWait.invalidate(); deadline.start(); readRoot(Step::InitialRoot);
}
void CloudWorkspaceRead::readRoot(Step next)
{
	if (!current()) return;
	step = next; files.setResponseLimit(1024 * 1024 + 4096); files.downloadIdentity(provider, token, reference.rootName(), text(reference.value, "root_id"));
}
QJsonObject CloudWorkspaceRead::target() const
{
	const auto t = root.ticket(commit); if (text(t, "state") != "committed") return {};
	return {{"name", t.value("name")}, {"object", t.value("object")}, {"version", t.value("version")}, {"sha256", t.value("sha256")}, {"bytes", t.value("bytes")}};
}
bool CloudWorkspaceRead::pinned() const { return !intent->snapshot().isEmpty() && intent->snapshot() == target(); }
bool CloudWorkspaceRead::active() const
{
	const auto r = root.value.value("journal").toObject().value("reservations").toObject().value(reservation).toObject();
	return pinned() && text(r, "commit") == commit && r.value("active") == QJsonValue(true);
}
void CloudWorkspaceRead::received(const QString &name, const QByteArray &bytes, const QString &version, const QString &id)
{
	if (!current()) return;
	if (step == Step::Payload) {
		const auto t = intent->snapshot();
		if (name != text(t, "name") || id != text(t, "object") || version != text(t, "version") || bytes.size() != t.value("bytes").toDouble() ||
		    CloudSyncManifest::sha256(bytes) != text(t, "sha256") || !CloudHistoryRecord::validXml(bytes)) { fail(); return; }
		payload = bytes; readRoot(Step::FinalRoot); return;
	}
	if (name != reference.rootName() || id != text(reference.value, "root_id") || !revision(version)) { fail(); return; }
	const auto observed = CloudWorkspaceRecord::parse(bytes); CloudWorkspaceAuthority authority(checkpointFile, reference);
	if (!observed.valid() || observed.reference().value != reference.value || (step == Step::ConfirmRoot && !observed.follows(pending)) ||
	    !authority.observe(observed, version, true) || !current()) { fail(); return; }
	root = observed; rootVersion = version;
	if (step == Step::InitialRoot) { if (action == Action::Read) reserve(); else release(); return; }
	if (step == Step::ConfirmRoot) { pending.value = {}; if (action == Action::Read) access(); else finishRelease(); return; }
	if (step == Step::BeforePayload) {
		if (!active()) { fail(); return; }
		const auto t = intent->snapshot(); step = Step::Payload; files.setResponseLimit(64 * 1024 * 1024);
		files.downloadExact(provider, token, text(t, "name"), text(t, "object"), text(t, "version")); return;
	}
	if (step == Step::FinalRoot) {
		if (!active() || !intent->markVerified() || !current()) { fail(); return; }
		const auto result = root.value; const auto body = payload; const auto v = rootVersion, c = commit, r = reservation;
		cancel(); emit finished(result, v, c, r, body); return;
	}
	fail();
}
void CloudWorkspaceRead::writeRoot(const CloudWorkspaceRecord &next)
{
	if (!current()) return;
	CloudWorkspaceAuthority authority(checkpointFile, reference);
	if (!next.follows(root) || !authority.observe(root, rootVersion, true) || !current()) { fail(); return; }
	pending = next; step = Step::WriteRoot;
	files.uploadIdentity(provider, token, reference.rootName(), next.json(), rootVersion, text(reference.value, "root_id"));
}
void CloudWorkspaceRead::reserve()
{
	const auto t = target(), r = root.value.value("journal").toObject().value("reservations").toObject().value(reservation).toObject();
	if (t.isEmpty()) { fail(); return; }
	if (!r.isEmpty()) {
		// A missing local intent cannot adopt a reservation visible in the cloud.
		if (intent->state() == "prepared" || !active()) { fail(); return; }
		access(); return;
	}
	if (intent->state() != "prepared" && intent->state() != "reserving") { fail(); return; }
	if (intent->state() == "prepared" && !intent->pin(t)) { fail(); return; }
	if (!pinned()) { fail(); return; }
	auto next = root; if (!next.reserve(reservation, commit)) { fail(); return; }
	writeRoot(next);
}
void CloudWorkspaceRead::access()
{
	if (!current() || !active()) { fail(); return; }
	if (intent->state() == "reserving" && !intent->markReserved()) { fail(); return; }
	if (intent->state() != "reserved" && intent->state() != "verified") { fail(); return; }
	readRoot(Step::BeforePayload);
}
void CloudWorkspaceRead::release()
{
	if (!pinned()) { fail(); return; }
	const auto r = root.value.value("journal").toObject().value("reservations").toObject().value(reservation).toObject();
	if (!r.isEmpty() && text(r, "commit") != commit) { fail(); return; }
	if (!r.isEmpty() && r.value("active") == QJsonValue(false)) { finishRelease(); return; }
	if (intent->state() == "released") { fail(); return; }
	auto next = root;
	// Fence a delayed registration even when its acknowledgement/root update
	// was never observed: one CAS installs a permanent inactive reservation.
	if ((r.isEmpty() && !next.reserve(reservation, commit)) || !next.release(reservation)) { fail(); return; }
	writeRoot(next);
}
void CloudWorkspaceRead::finishRelease()
{
	if (!current() || !pinned()) { fail(); return; }
	const auto r = root.value.value("journal").toObject().value("reservations").toObject().value(reservation).toObject();
	if (text(r, "commit") != commit || r.value("active") != QJsonValue(false) || !intent->markReleased() || !current()) { fail(); return; }
	const auto result = root.value; const auto v = rootVersion, rId = reservation;
	cancel(); emit released(result, v, rId);
}
