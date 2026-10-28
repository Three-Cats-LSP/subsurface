// SPDX-License-Identifier: GPL-2.0
#include "cloudsyncworkspacereceive.h"
#include <QDir>
#include <QFileInfo>
#include <QLockFile>

namespace {
QString text(const QJsonObject &o, const char *key) { return o.value(QLatin1String(key)).toString(); }
QJsonObject pin(const QJsonObject &t)
{
	return {{"name", t.value("name")}, {"object", t.value("object")}, {"version", t.value("version")},
		{"sha256", t.value("sha256")}, {"bytes", t.value("bytes")}};
}
bool reservationActive(const CloudWorkspaceRecord &root, const QString &reservation, const QString &target)
{
	const auto r = root.value.value("journal").toObject().value("reservations").toObject().value(reservation).toObject();
	return text(r, "commit") == target && r.value("active") == QJsonValue(true);
}
}

CloudWorkspaceReceive::CloudWorkspaceReceive(QNetworkAccessManager *network, QObject *parent) :
	QObject(parent), reader(network, this), opener(network, this)
{
	deadline.setSingleShot(true); deadline.setInterval(600000); startupRetry.setSingleShot(true);
	connect(&deadline, &QTimer::timeout, this, [this] { fail(); });
	connect(&startupRetry, &QTimer::timeout, this, &CloudWorkspaceReceive::begin);
	connect(&reader, &CloudWorkspaceRead::failed, this, [this](const QString &, bool rejected) { fail(rejected); });
	connect(&opener, &CloudWorkspaceOpen::failed, this, [this](const QString &, bool rejected) { fail(rejected); });
	connect(&reader, &CloudWorkspaceRead::finished, this, [this](const QJsonObject &root, const QString &, const QString &commit,
		const QString &id, const QByteArray &payload) {
		if (step != Step::Reading || commit != target || id != reservation) { fail(); return; }
		received(root, payload);
	});
	connect(&reader, &CloudWorkspaceRead::released, this, &CloudWorkspaceReceive::released);
	connect(&opener, &CloudWorkspaceOpen::finished, this, &CloudWorkspaceReceive::completed);
}
CloudWorkspaceReceive::~CloudWorkspaceReceive() { cancel(); }
void CloudWorkspaceReceive::cancel()
{
	++generation; step = Step::Idle; reader.cancel(); opener.cancel(); deadline.stop(); startupRetry.stop(); startupWait.invalidate();
	receipt.reset(); operationLock.reset(); reference.value = {}; bindingBefore = {}; recoveryEstablished = false;
	token.clear(); baseline.clear(); target.clear(); reservation.clear(); choiceFile.clear(); choiceVersion.clear(); checkpointFile.clear();
	bindingFile.clear(); bindingRevision.clear(); nextRevision.clear(); readFile.clear(); recoveryDirectory.clear(); before.clear(); raw.clear(); after.clear();
	scopeCurrent = {}; localSnapshot = {}; projectPayload = {}; applyPayload = {};
}
void CloudWorkspaceReceive::fail(bool rejected)
{
	if (step == Step::Idle) return;
	cancel(); emit failed(tr("The workspace receive needs review. Local data, cloud versions and recovery reservations were kept."), rejected);
}
void CloudWorkspaceReceive::start(const QString &accessToken, const CloudWorkspaceReference &workspace,
	const QString &base, const QString &commit, const QString &id, const QString &choice, const QString &choiceVer,
	const QString &checkpoint, const QString &binding, const QString &oldRevision, const QString &newRevision,
	const QString &readIntent, const QString &directory, const QByteArray &expectedBefore, const std::function<bool()> &guard,
	const std::function<QByteArray()> &snapshot, const std::function<QByteArray(const QByteArray &, const QByteArray &)> &project,
	const std::function<bool(const QByteArray &, const QByteArray &, const QByteArray &)> &apply)
{
	cancel(); token = accessToken; reference = workspace; baseline = base; target = commit; reservation = id;
	choiceFile = choice; choiceVersion = choiceVer; checkpointFile = checkpoint; bindingFile = binding; bindingRevision = oldRevision;
	nextRevision = newRevision; readFile = readIntent; recoveryDirectory = directory; before = expectedBefore;
	scopeCurrent = guard; localSnapshot = snapshot; projectPayload = project; applyPayload = apply; step = Step::Starting;
	if (token.isEmpty() || !reference.valid() || !CloudHistoryRecord::validEpoch(baseline) || !CloudHistoryRecord::validEpoch(target) ||
	    !CloudHistoryRecord::validEpoch(reservation) || !CloudHistoryRecord::validEpoch(choiceVersion) ||
	    !CloudHistoryRecord::validEpoch(bindingRevision) || !CloudHistoryRecord::validEpoch(nextRevision) ||
	    baseline == target || reservation == baseline || reservation == target || bindingRevision == nextRevision ||
	    choiceFile.isEmpty() || checkpointFile.isEmpty() || bindingFile.isEmpty() || readFile.isEmpty() || recoveryDirectory.isEmpty() ||
	    !scopeCurrent || !localSnapshot || !projectPayload || !applyPayload) { fail(); return; }
	startupWait.start(); begin();
}
void CloudWorkspaceReceive::retryStartupOrFail(const QString &path)
{
	QLockFile lock(path + ".lock"); const QFileInfo info(lock.fileName()); qint64 pid = 0; QString host, application;
	if (startupWait.isValid() && !startupWait.hasExpired(35000) && info.isFile() && !info.isSymLink() &&
	    !lock.getLockInfo(&pid, &host, &application)) { startupRetry.start(150); return; }
	fail();
}
void CloudWorkspaceReceive::begin()
{
	if (step != Step::Starting) return;
	if (QFileInfo(recoveryDirectory).isSymLink() || !QDir().mkpath(recoveryDirectory)) { fail(); return; }
	if (!operationLock) { operationLock = std::make_unique<QLockFile>(recoveryDirectory + ".operation.lock"); operationLock->setStaleLockTime(0); }
	if (!operationLock->isLocked() && !operationLock->tryLock(0)) { retryStartupOrFail(recoveryDirectory + ".operation"); return; }
	receipt = std::make_unique<CloudWorkspaceReceiveReceipt>(recoveryDirectory, reference, choiceVersion, bindingRevision, baseline, target, reservation, nextRevision);
	const auto path = QDir(recoveryDirectory).filePath("intent.json"); recoveryEstablished = QFileInfo::exists(path);
	if (recoveryEstablished) {
		if (!receipt->reload()) { retryStartupOrFail(path); return; }
		const auto previous = receipt->before();
		if (!previous.isEmpty()) { if (!before.isEmpty() && before != previous) { fail(); return; } before = previous; }
		raw = receipt->raw(); after = receipt->after();
		if (receipt->state() != "prepared" && (before.isEmpty() || raw.isEmpty() || after.isEmpty())) { fail(); return; }
	}
	if (before.isEmpty() || before.size() > 64 * 1024 * 1024 || !CloudHistoryRecord::validXml(before)) { fail(); return; }
	if (!current()) return;
	CloudWorkspaceAuthority authority(checkpointFile, reference); CloudWorkspaceBinding binding(bindingFile, reference, choiceVersion);
	if (!authority.load()) { retryStartupOrFail(checkpointFile); return; }
	if (!binding.load()) { retryStartupOrFail(bindingFile); return; }
	if (!authority.record().valid() || (binding.revision() != bindingRevision && binding.revision() != nextRevision) ||
	    (binding.revision() == bindingRevision && (binding.commit() != baseline || !binding.matchesModel(before))) ||
	    (binding.revision() == nextRevision && (!receipt || receipt->state() != "persisted"))) { fail(); return; }
	if (binding.revision() == bindingRevision) bindingBefore = binding.value();
	startupRetry.stop(); startupWait.invalidate(); deadline.start();
	if (receipt && receipt->state() == "persisted") { readFinalRoot(); return; }
	step = Step::Reading;
	reader.start(CloudWorkspaceRead::Action::Read, token, reference, target, reservation, choiceFile, choiceVersion, checkpointFile, readFile, [this] { return current(); });
}
bool CloudWorkspaceReceive::current(bool local)
{
	if (step == Step::Idle) return false;
	const auto serial = generation; const auto guard = scopeCurrent; const bool allowed = guard && guard();
	if (serial != generation || step == Step::Idle) return false;
	if (!allowed) { fail(); return false; }
	CloudWorkspaceSelection choice(choiceFile, text(reference.value, "provider"), text(reference.value, "account"));
	if (!choice.load()) { if (step == Step::Starting) retryStartupOrFail(choiceFile); else fail(); return false; }
	if (choice.version() != choiceVersion || choice.selected().value != reference.value) { fail(); return false; }
	if (!bindingBefore.isEmpty()) {
		CloudWorkspaceBinding binding(bindingFile, reference, choiceVersion);
		if (!binding.load() || (binding.revision() == bindingRevision && binding.value() != bindingBefore) ||
		    (binding.revision() != bindingRevision && binding.revision() != nextRevision)) { fail(); return false; }
	}
	const auto path = QDir(recoveryDirectory).filePath("intent.json");
	if ((recoveryEstablished || QFileInfo::exists(path)) && (!receipt || !receipt->reload())) {
		if (step == Step::Starting) retryStartupOrFail(path); else fail(); return false;
	}
	if (!local) return true;
	const auto snapshot = localSnapshot; if (!snapshot) { fail(); return false; }
	const auto actual = snapshot(); if (serial != generation || step == Step::Idle) return false;
	const auto phase = receipt ? receipt->state() : QString();
	const bool sameBefore = actual == before, sameAfter = !after.isEmpty() && actual == after;
	const bool allowedLocal = phase == "persisted" ? sameAfter :
		(phase == "applying" || phase == "applied") ? sameBefore || sameAfter : sameBefore;
	if (!allowedLocal) { fail(); return false; }
	return true;
}
void CloudWorkspaceReceive::received(const QJsonObject &value, const QByteArray &payload)
{
	CloudWorkspaceRecord root; root.value = value;
	if (!current() || !root.valid() || root.reference().value != reference.value || !reservationActive(root, reservation, target)) { fail(); return; }
	CloudWorkspaceAuthority authority(checkpointFile, reference); CloudWorkspaceBinding binding(bindingFile, reference, choiceVersion);
	CloudWorkspaceReadIntent owned(readFile, reference, target, reservation);
	if (!authority.load() || !root.follows(authority.record()) || !binding.load() || binding.revision() != bindingRevision ||
	    binding.commit() != baseline || !binding.matchesModel(before) || !owned.reload() || owned.state() != "verified" ||
	    owned.snapshot() != pin(root.ticket(target))) { fail(); return; }
	const auto serial = generation; const auto snapshot = localSnapshot; const auto actual = snapshot(); if (serial != generation || step == Step::Idle) return;
	const bool alreadyApplied = (receipt->state() == "applying" || receipt->state() == "applied") && !after.isEmpty() && actual == after;
	if (alreadyApplied) {
		const auto proof = receipt->proof();
		if (payload != raw || proof.value("baseline_pin").toObject() != pin(root.ticket(baseline)) ||
		    proof.value("target_pin").toObject() != pin(root.ticket(target)) ||
		    text(root.latestLegacyAcceptance(), "id") != binding.acceptance()) { fail(); return; }
		if (receipt->state() == "applying" && !receipt->applied(actual)) { fail(); return; }
	} else {
		if (root.head() != target) { fail(); return; }
		const auto project = projectPayload; const auto projected = project(payload, before); if (serial != generation || step == Step::Idle) return;
		if (projected.isEmpty() || projected.size() > 64 * 1024 * 1024 || !CloudHistoryRecord::validXml(projected) ||
		    (!after.isEmpty() && after != projected) || (!raw.isEmpty() && raw != payload) ||
		    !receipt->prepare(binding, root, authority.record(), owned, before, payload, projected)) { fail(); return; }
		recoveryEstablished = true; raw = payload; after = projected;
		if (!current() || (receipt->state() != "applied" && !receipt->beginApply()) || !current()) { fail(); return; }
		const auto apply = applyPayload; const bool applied = apply(raw, before, after); if (serial != generation || step == Step::Idle) return;
		if (!applied || !current()) { fail(); return; }
		const auto observed = snapshot(); if (serial != generation || step == Step::Idle) return;
		if (!receipt->applied(observed)) { fail(); return; }
	}
	step = Step::Persistence; const auto directory = recoveryDirectory; emit applied(directory);
}
void CloudWorkspaceReceive::confirmPersisted()
{
	if (step != Step::Persistence || !current()) return;
	const auto serial = generation; const auto snapshot = localSnapshot; const auto observed = snapshot(); if (serial != generation || step == Step::Idle) return;
	if (!receipt->persisted(observed)) { fail(); return; }
	readFinalRoot();
}
void CloudWorkspaceReceive::readFinalRoot()
{
	if (!current()) return;
	CloudWorkspaceAuthority authority(checkpointFile, reference);
	if (!authority.load() || !authority.record().valid()) { fail(); return; }
	step = Step::FinalRoot;
	opener.start(token, text(reference.value, "provider"), text(reference.value, "account"), reference, [this] { return current(); }, authority.record());
}
void CloudWorkspaceReceive::completed(const QJsonObject &value, const QString &version)
{
	if (step != Step::FinalRoot || !current() || !receipt || receipt->state() != "persisted") { fail(); return; }
	CloudWorkspaceRecord root; root.value = value; CloudWorkspaceAuthority authority(checkpointFile, reference);
	CloudWorkspaceBinding binding(bindingFile, reference, choiceVersion); CloudWorkspaceReadIntent owned(readFile, reference, target, reservation);
	if (!root.valid() || !authority.observe(root, version, true) || !binding.load() || !owned.reload()) { fail(); return; }
	const auto serial = generation; const auto snapshot = localSnapshot; const auto actual = snapshot(); if (serial != generation || step == Step::Idle) return;
	if (binding.revision() == bindingRevision) {
		if (!binding.advanceReceived(bindingRevision, root, authority.record(), *receipt, owned, actual, nextRevision)) { fail(); return; }
	} else if (binding.revision() != nextRevision || !binding.ownsReceived(root, authority.record(), *receipt, owned)) { fail(); return; }
	if (!current() || !binding.ownsReceived(root, authority.record(), *receipt, owned)) { fail(); return; }
	if (owned.state() == "released") { const auto result = root.value; const auto v = version, commit = target; cancel(); emit finished(result, v, commit); return; }
	step = Step::Releasing;
	reader.start(CloudWorkspaceRead::Action::Release, token, reference, target, reservation, choiceFile, choiceVersion, checkpointFile, readFile, [this] { return current(); });
}
void CloudWorkspaceReceive::released(const QJsonObject &value, const QString &version, const QString &id)
{
	if (step != Step::Releasing || id != reservation || !current()) { fail(); return; }
	CloudWorkspaceRecord root; root.value = value; CloudWorkspaceAuthority authority(checkpointFile, reference);
	CloudWorkspaceBinding binding(bindingFile, reference, choiceVersion); CloudWorkspaceReadIntent owned(readFile, reference, target, reservation);
	if (!authority.load() || !binding.load() || !binding.ownsReceived(root, authority.record(), *receipt, owned)) { fail(); return; }
	const auto result = root.value; const auto v = version, commit = target; cancel(); emit finished(result, v, commit);
}
