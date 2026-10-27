// SPDX-License-Identifier: GPL-2.0
#include "cloudsyncworkspaceapply.h"
#include "cloudsyncworkspaceupload.h"
#include "cloudsyncmanifest.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QLockFile>
#include <QSaveFile>
#include <QRegularExpression>

namespace {
constexpr qint64 PayloadLimit = 64 * 1024 * 1024;
QString text(const QJsonObject &o, const char *key) { return o.value(QLatin1String(key)).toString(); }
const QStringList States = {"prepared", "ready", "applying", "applied", "persisted"};
QJsonObject pin(const CloudWorkspaceRecord &root, const QString &id)
{
	const auto t = root.ticket(id);
	if (text(t, "state") != "committed") return {};
	return {{"name", t.value("name")}, {"object", t.value("object")}, {"version", t.value("version")}, {"sha256", t.value("sha256")}, {"bytes", t.value("bytes")}};
}
bool validPin(const CloudWorkspaceReference &ref, const QString &id, const QJsonObject &p)
{
	const auto v = text(p, "version"); const auto size = p.value("bytes").toDouble();
	return p.size() == 5 && text(p, "name") == ref.payloadName(id) && text(p, "object") != text(ref.value, "root_id") &&
		CloudWorkspaceReference::create(text(ref.value, "provider"), text(ref.value, "account"), text(ref.value, "epoch"), text(p, "object")).valid() &&
		!v.isEmpty() && v.size() <= 512 && !v.contains(QChar::Null) && !v.contains('\n') && !v.contains('\r') &&
		QRegularExpression("^[a-f0-9]{64}$").match(text(p, "sha256")).hasMatch() && p.value("bytes").isDouble() &&
		size >= 1 && size <= PayloadLimit && size == double(qint64(size));
}
bool accepts(const QJsonObject &p, const QByteArray &bytes)
{
	return !bytes.isEmpty() && bytes.size() <= PayloadLimit && bytes.size() == p.value("bytes").toDouble() &&
		CloudSyncManifest::sha256(bytes) == text(p, "sha256") && CloudHistoryRecord::validXml(bytes);
}
QByteArray readXml(const QString &path, const QJsonObject &p)
{
	const QFileInfo info(path); if (!info.isFile() || info.isSymLink() || info.size() > PayloadLimit) return {};
	QFile file(path); if (!file.open(QIODevice::ReadOnly)) return {}; const auto bytes = file.read(PayloadLimit + 1);
	return file.error() == QFileDevice::NoError && accepts(p, bytes) ? bytes : QByteArray();
}
bool saveXml(const QString &path, const QJsonObject &p, const QByteArray &bytes)
{
	const QFileInfo info(path); if (info.exists() || info.isSymLink()) return readXml(path, p) == bytes;
	QSaveFile file(path); return accepts(p, bytes) && file.open(QIODevice::WriteOnly) &&
		file.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner) && file.write(bytes) == bytes.size() && file.commit() && readXml(path, p) == bytes;
}
bool reserved(const CloudWorkspaceRecord &root, const QString &id, const QString &commit, bool active)
{
	const auto r = root.value.value("journal").toObject().value("reservations").toObject().value(id).toObject();
	return text(r, "commit") == commit && (!active || r.value("active") == QJsonValue(true));
}
}
CloudWorkspaceApplyIntent::CloudWorkspaceApplyIntent(const QString &path, const CloudWorkspaceReference &ref,
	const QString &choice, const QString &commit, const QString &reservation) : directory(path), reference(ref)
{
	if (ref.valid() && CloudHistoryRecord::validEpoch(choice) && CloudHistoryRecord::validEpoch(commit) && CloudHistoryRecord::validEpoch(reservation) && commit != reservation)
		scope = {{"schema", 1}, {"format", "neo-workspace-local-restore-development-1"}, {"workspace", ref.value}, {"choice_version", choice}, {"commit", commit}, {"reservation", reservation}};
}
QString CloudWorkspaceApplyIntent::state() const { return text(saved, "state"); }
QByteArray CloudWorkspaceApplyIntent::before() const { return saved.isEmpty() ? QByteArray() : readXml(QDir(directory).filePath("before.xml"), saved.value("before").toObject()); }
QByteArray CloudWorkspaceApplyIntent::after() const { return saved.isEmpty() ? QByteArray() : readXml(QDir(directory).filePath("after.xml"), saved.value("after").toObject()); }
bool CloudWorkspaceApplyIntent::valid(const QJsonObject &body) const
{
	auto fixed = body;
	for (const auto &key : {"state", "parent", "source", "source_reservation", "before", "after", "source_snapshot"}) fixed.remove(QLatin1String(key));
	const auto parent = text(body, "parent"), source = text(body, "source"), reservation = text(body, "source_reservation"), commit = text(scope, "commit");
	return !scope.isEmpty() && fixed == scope && States.contains(text(body, "state")) && CloudHistoryRecord::validEpoch(parent) &&
		CloudHistoryRecord::validEpoch(source) && CloudHistoryRecord::validEpoch(reservation) && source != commit && parent != commit &&
		reservation != commit && reservation != parent && reservation != source && reservation != text(scope, "reservation") &&
		text(scope, "reservation") != parent && text(scope, "reservation") != source &&
		validPin(reference, parent, body.value("before").toObject()) && validPin(reference, commit, body.value("after").toObject()) &&
		validPin(reference, source, body.value("source_snapshot").toObject()) &&
		body.value("after").toObject().value("object") != body.value("before").toObject().value("object") &&
		body.value("after").toObject().value("object") != body.value("source_snapshot").toObject().value("object") &&
		(parent != source || body.value("before") == body.value("source_snapshot")) &&
		(parent == source || body.value("before").toObject().value("object") != body.value("source_snapshot").toObject().value("object")) &&
		body.value("after").toObject().value("sha256") == body.value("source_snapshot").toObject().value("sha256") &&
		body.value("after").toObject().value("bytes") == body.value("source_snapshot").toObject().value("bytes");
}
bool CloudWorkspaceApplyIntent::read(bool verifyXml)
{
	saved = {}; if (scope.isEmpty() || directory.isEmpty() || QFileInfo(directory).isSymLink()) return false;
	const QFileInfo info(QDir(directory).filePath("intent.json")); if (!info.exists()) return !info.isSymLink();
	if (!info.isFile() || info.isSymLink() || info.size() > 16384) return false;
	QFile file(info.filePath()); if (!file.open(QIODevice::ReadOnly)) return false; const auto bytes = file.read(16385);
	const auto body = QJsonDocument::fromJson(bytes).object(); if (file.error() != QFileDevice::NoError || bytes.size() > 16384 || !valid(body)) return false;
	for (const auto &name : {"before", "after"}) {
		const QFileInfo payload(QDir(directory).filePath(QLatin1String(name) + ".xml"));
		if (text(body, "state") == "prepared" && !payload.exists() && !payload.isSymLink()) continue;
		const auto p = body.value(QLatin1String(name)).toObject();
		if (!payload.isFile() || payload.isSymLink() || payload.size() != p.value("bytes").toDouble() || (verifyXml && readXml(payload.filePath(), p).isEmpty())) return false;
	}
	saved = body; return true;
}
bool CloudWorkspaceApplyIntent::inspect() { return read() && !saved.isEmpty(); }
bool CloudWorkspaceApplyIntent::reload()
{
	QLockFile lock(QDir(directory).filePath("intent.json.lock")); return !directory.isEmpty() && lock.tryLock(0) && read(false) && !saved.isEmpty();
}
bool CloudWorkspaceApplyIntent::write(const QJsonObject &body)
{
	if (!valid(body)) return false;
	const auto bytes = QJsonDocument(body).toJson(QJsonDocument::Compact); QSaveFile file(QDir(directory).filePath("intent.json"));
	return bytes.size() <= 16384 && file.open(QIODevice::WriteOnly) && file.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner) &&
		file.write(bytes) == bytes.size() && file.commit() && read() && saved == body;
}
bool CloudWorkspaceApplyIntent::prepare(const QByteArray &before, const QByteArray &after, const CloudWorkspaceRecord &root)
{
	if (scope.isEmpty() || directory.isEmpty() || QFileInfo(directory).isSymLink() || !root.valid() || root.reference().value != reference.value || !QDir().mkpath(directory)) return false;
	QLockFile lock(QDir(directory).filePath("intent.json.lock")); if (!lock.tryLock(0) || !read()) return false;
	const auto commit = text(scope, "commit"); const auto ticket = root.ticket(commit); const auto source = text(ticket, "restore_source"), sourceReservation = text(ticket, "reservation"), parent = text(ticket, "parent");
	auto next = scope; next["state"] = "prepared"; next["parent"] = parent; next["source"] = source; next["source_reservation"] = sourceReservation;
	next["before"] = pin(root, parent); next["after"] = pin(root, commit); next["source_snapshot"] = pin(root, source);
	if (!valid(next) || !accepts(next.value("before").toObject(), before) || !accepts(next.value("after").toObject(), after)) return false;
	if (saved.isEmpty()) {
		// A new receipt cannot claim pre-existing recovery files or other data.
		if (root.head() != commit || !reserved(root, sourceReservation, source, true) || !reserved(root, text(scope, "reservation"), commit, true)) return false;
		for (const auto &name : QDir(directory).entryList(QDir::AllEntries | QDir::Hidden | QDir::System | QDir::NoDotAndDotDot)) if (name != "intent.json.lock") return false;
		if (!write(next)) return false;
	} else { auto previous = saved; previous["state"] = "prepared"; if (previous != next) return false; }
	if (!saveXml(QDir(directory).filePath("before.xml"), saved.value("before").toObject(), before) ||
	    !saveXml(QDir(directory).filePath("after.xml"), saved.value("after").toObject(), after)) return false;
	if (state() != "prepared") return true;
	next = saved; next["state"] = "ready"; return write(next);
}
bool CloudWorkspaceApplyIntent::transition(const QString &from, const QString &to, const QByteArray &observed)
{
	QLockFile lock(QDir(directory).filePath("intent.json.lock")); if (!lock.tryLock(0) || !read() || saved.isEmpty()) return false;
	if (!observed.isEmpty() && !accepts(saved.value("after").toObject(), observed)) return false;
	if (state() == to) return true;
	if (state() != from) return false;
	auto next = saved; next["state"] = to; return write(next);
}
bool CloudWorkspaceApplyIntent::beginApply() { return transition("ready", "applying"); }
bool CloudWorkspaceApplyIntent::applied(const QByteArray &observed) { return !observed.isEmpty() && transition("applying", "applied", observed); }
bool CloudWorkspaceApplyIntent::persisted(const QByteArray &observed) { return !observed.isEmpty() && transition("applied", "persisted", observed); }

CloudWorkspaceApply::CloudWorkspaceApply(QNetworkAccessManager *network, QObject *parent) : QObject(parent), reader(network, this), opener(network, this)
{
	deadline.setSingleShot(true); deadline.setInterval(600000); startupRetry.setSingleShot(true);
	connect(&deadline, &QTimer::timeout, this, [this] { fail(); }); connect(&startupRetry, &QTimer::timeout, this, &CloudWorkspaceApply::begin);
	connect(&reader, &CloudWorkspaceRead::failed, this, [this](const QString &, bool rejected) { fail(rejected); });
	connect(&opener, &CloudWorkspaceOpen::failed, this, [this](const QString &, bool rejected) { fail(rejected); });
	connect(&reader, &CloudWorkspaceRead::finished, this, [this](const QJsonObject &root, const QString &version, const QString &c, const QString &r, const QByteArray &bytes) {
		if (step != Step::Reading || c != commit || r != reservation) { fail(); return; } received(root, version, bytes);
	});
	connect(&opener, &CloudWorkspaceOpen::finished, this, &CloudWorkspaceApply::completed);
}
CloudWorkspaceApply::~CloudWorkspaceApply() { cancel(); }
void CloudWorkspaceApply::cancel()
{
	++generation; step = Step::Idle; reader.cancel(); opener.cancel(); deadline.stop(); startupRetry.stop(); startupWait.invalidate(); intent.reset(); operationLock.reset(); reference.value = {}; recoveryEstablished = false;
	token.clear(); commit.clear(); reservation.clear(); choiceFile.clear(); choiceVersion.clear(); checkpointFile.clear(); sourceFile.clear(); uploadFile.clear(); readFile.clear(); recoveryDirectory.clear(); beforePayload.clear(); afterPayload.clear();
	scopeCurrent = {}; localSnapshot = {}; applyPayload = {};
}
void CloudWorkspaceApply::fail(bool rejected)
{
	if (step == Step::Idle) return;
	cancel(); emit failed(tr("The local restore needs review. Recovery copies, cloud versions and reservations were kept; later local edits were not replaced."), rejected);
}
void CloudWorkspaceApply::start(const QString &accessToken, const CloudWorkspaceReference &workspace, const QString &targetCommit, const QString &readReservation,
	const QString &choice, const QString &expectedChoice, const QString &checkpoint, const QString &sourceIntent, const QString &uploadIntent, const QString &readIntent,
	const QString &directory, const QByteArray &expectedBefore, const std::function<bool()> &guard, const std::function<QByteArray()> &snapshot,
	const std::function<bool(const QByteArray &)> &apply)
{
	cancel(); token = accessToken; reference = workspace; commit = targetCommit; reservation = readReservation; choiceFile = choice; choiceVersion = expectedChoice; checkpointFile = checkpoint;
	sourceFile = sourceIntent; uploadFile = uploadIntent; readFile = readIntent; recoveryDirectory = directory; beforePayload = expectedBefore; scopeCurrent = guard; localSnapshot = snapshot; applyPayload = apply; step = Step::Starting;
	if (token.isEmpty() || !reference.valid() || !CloudHistoryRecord::validEpoch(commit) || !CloudHistoryRecord::validEpoch(reservation) || commit == reservation ||
	    !CloudHistoryRecord::validEpoch(choiceVersion) || directory.isEmpty() || !scopeCurrent || !localSnapshot || !applyPayload || sourceFile.isEmpty() || uploadFile.isEmpty() || readFile.isEmpty()) { fail(); return; }
	startupWait.start(); begin();
}
void CloudWorkspaceApply::retryStartupOrFail(const QString &path)
{
	QLockFile lock(path + ".lock"); const QFileInfo info(lock.fileName()); qint64 pid = 0; QString host, application;
	if (startupWait.isValid() && !startupWait.hasExpired(35000) && info.isFile() && !info.isSymLink() && !lock.getLockInfo(&pid, &host, &application)) { startupRetry.start(150); return; }
	fail();
}
void CloudWorkspaceApply::begin()
{
	if (step != Step::Starting) return;
	if (QFileInfo(recoveryDirectory).isSymLink() || !QDir().mkpath(recoveryDirectory)) { fail(); return; }
	if (!operationLock) operationLock = std::make_unique<QLockFile>(recoveryDirectory + ".operation.lock");
	if (!operationLock->isLocked() && !operationLock->tryLock(0)) { retryStartupOrFail(recoveryDirectory + ".operation"); return; }
	intent = std::make_unique<CloudWorkspaceApplyIntent>(recoveryDirectory, reference, choiceVersion, commit, reservation);
	const auto path = QDir(recoveryDirectory).filePath("intent.json"); recoveryEstablished = QFileInfo::exists(path);
	if (recoveryEstablished) {
		if (!intent->reload()) { retryStartupOrFail(path); return; }
		const auto previous = intent->before();
		if (!previous.isEmpty()) { if (!beforePayload.isEmpty() && beforePayload != previous) { fail(); return; } beforePayload = previous; }
		afterPayload = intent->after();
		if (intent->state() != "prepared" && (previous.isEmpty() || afterPayload.isEmpty())) { fail(); return; }
		if (intent->state() == "prepared" && beforePayload.isEmpty()) {
			const auto serial = generation; const auto snapshot = localSnapshot; const auto actual = snapshot();
			if (serial != generation || step == Step::Idle) return;
			if (!accepts(intent->proof().value("before").toObject(), actual)) { fail(); return; }
			beforePayload = actual;
		}
	}
	if (beforePayload.isEmpty() || beforePayload.size() > PayloadLimit || !CloudHistoryRecord::validXml(beforePayload)) { fail(); return; }
	if (!current()) return;
	CloudWorkspaceAuthority authority(checkpointFile, reference);
	if (!authority.load()) { retryStartupOrFail(checkpointFile); return; }
	if (!authority.record().valid()) { fail(); return; }
	startupWait.invalidate(); startupRetry.stop(); deadline.start();
	if (intent->state() == "persisted") { readFinalRoot(); return; }
	step = Step::Reading; reader.start(CloudWorkspaceRead::Action::Read, token, reference, commit, reservation, choiceFile, choiceVersion, checkpointFile, readFile, [this] { return current(); });
}
bool CloudWorkspaceApply::current(bool local)
{
	if (step == Step::Idle) return false;
	const auto serial = generation; const auto guard = scopeCurrent; const bool allowed = guard && guard();
	if (serial != generation || step == Step::Idle) return false;
	if (!allowed) { fail(); return false; }
	CloudWorkspaceSelection choice(choiceFile, text(reference.value, "provider"), text(reference.value, "account"));
	if (!choice.load()) { if (step == Step::Starting) retryStartupOrFail(choiceFile); else fail(); return false; }
	if (choice.version() != choiceVersion || choice.selected().value != reference.value) { fail(); return false; }
	const auto path = QDir(recoveryDirectory).filePath("intent.json");
	if ((recoveryEstablished || QFileInfo::exists(path)) && (!intent || !intent->reload())) { if (step == Step::Starting) retryStartupOrFail(path); else fail(); return false; }
	if (!local || (intent && intent->state() == "persisted")) return true;
	const auto snapshot = localSnapshot; if (!snapshot) { fail(); return false; } const auto actual = snapshot(); if (serial != generation || step == Step::Idle) return false;
	const auto phase = intent ? intent->state() : QString();
	const bool sameBefore = actual == beforePayload, sameAfter = !afterPayload.isEmpty() && actual == afterPayload;
	if ((phase == "applied" || phase == "applying") ? !sameBefore && !sameAfter : !sameBefore) { fail(); return false; }
	return true;
}
bool CloudWorkspaceApply::proofs(const CloudWorkspaceRecord &root, bool active)
{
	if (!root.valid() || root.reference().value != reference.value || !reserved(root, reservation, commit, active)) return false;
	const auto t = root.ticket(commit); const auto source = text(t, "restore_source"), sourceReservation = text(t, "reservation"), parent = text(t, "parent");
	const auto after = pin(root, commit), previous = pin(root, parent), original = pin(root, source);
	if (after.isEmpty() || previous.isEmpty() || original.isEmpty() || !accepts(previous, beforePayload) || !reserved(root, sourceReservation, source, active) || root.copySource(commit).valid()) return false;
	CloudWorkspaceReadIntent sourceRead(sourceFile, reference, source, sourceReservation), applicationRead(readFile, reference, commit, reservation);
	if (!sourceRead.reload() || !applicationRead.reload() || sourceRead.snapshot() != original || applicationRead.snapshot() != after) return false;
	const auto owned = [active](const QString &phase) { return phase == "verified" || (!active && (phase == "releasing" || phase == "released")); };
	if (!owned(sourceRead.state()) || !owned(applicationRead.state())) return false;
	const QJsonObject proof = {{"source", source}, {"reservation", sourceReservation}, {"snapshot", original}};
	CloudWorkspaceUploadIntent upload(uploadFile, reference, text(after, "sha256"), qint64(after.value("bytes").toDouble()), commit, parent, proof);
	if (!upload.reload() || upload.state() != "committed" || upload.id() != text(after, "object") || upload.version() != text(after, "version")) return false;
	if (intent && !intent->proof().isEmpty()) {
		const auto saved = intent->proof();
		if (saved.value("before").toObject() != previous || saved.value("after").toObject() != after || saved.value("source_snapshot").toObject() != original ||
		    text(saved, "source") != source || text(saved, "source_reservation") != sourceReservation || text(saved, "parent") != parent) return false;
	}
	return true;
}
void CloudWorkspaceApply::received(const QJsonObject &value, const QString &, const QByteArray &payload)
{
	CloudWorkspaceRecord root; root.value = value;
	if (!current() || !proofs(root, true)) { fail(); return; }
	const auto serial = generation; const auto snapshot = localSnapshot; const auto actual = snapshot(); if (serial != generation || step == Step::Idle) return;
	const bool alreadyApplied = (intent->state() == "applying" || intent->state() == "applied") && !afterPayload.isEmpty() && actual == afterPayload;
	if ((!alreadyApplied && root.head() != commit) || !intent->prepare(beforePayload, payload, root)) { fail(); return; }
	recoveryEstablished = true; afterPayload = payload;
	if (!current() || !proofs(root, true)) { fail(); return; }
	if (alreadyApplied) { if (intent->state() == "applying" && !intent->applied(actual)) { fail(); return; } }
	else {
		// An applied-but-unpersisted model is lost on process exit. A restart
		// still holding the exact old bytes can apply again after the fresh head
		// check, while preserving the existing durable recovery receipt.
		if ((intent->state() != "applied" && !intent->beginApply()) || !current()) { fail(); return; }
		const auto apply = applyPayload; const bool accepted = apply(payload);
		if (serial != generation || step == Step::Idle) return;
		if (!accepted || !current()) { fail(); return; }
		const auto observed = snapshot(); if (serial != generation || step == Step::Idle) return;
		if (!intent->applied(observed)) { fail(); return; }
	}
	step = Step::Persistence; const auto directory = recoveryDirectory; emit applied(directory);
}
void CloudWorkspaceApply::confirmPersisted()
{
	if (step != Step::Persistence || !current()) return;
	const auto serial = generation; const auto snapshot = localSnapshot; const auto observed = snapshot(); if (serial != generation || step == Step::Idle) return;
	if (!intent->persisted(observed)) { fail(); return; }
	readFinalRoot();
}
void CloudWorkspaceApply::readFinalRoot()
{
	if (!current(false)) return;
	CloudWorkspaceAuthority authority(checkpointFile, reference);
	if (!authority.load() || !authority.record().valid()) { fail(); return; }
	step = Step::FinalRoot;
	opener.start(token, text(reference.value, "provider"), text(reference.value, "account"), reference, [this] { return current(false); }, authority.record());
}
void CloudWorkspaceApply::completed(const QJsonObject &value, const QString &version)
{
	if (step != Step::FinalRoot || !current(false) || !intent || intent->state() != "persisted") { fail(); return; }
	CloudWorkspaceRecord root; root.value = value; CloudWorkspaceAuthority authority(checkpointFile, reference);
	if (!proofs(root, false) || !authority.observe(root, version, true) || !current(false)) { fail(); return; }
	const auto expected = intent->after(); const auto target = commit; const bool headMatches = root.head() == commit;
	const auto serial = generation; const auto snapshot = localSnapshot; const auto actual = snapshot(); if (serial != generation || step == Step::Idle) return;
	const bool localMatches = actual == expected; cancel(); emit finished(value, version, target, headMatches, localMatches);
}
