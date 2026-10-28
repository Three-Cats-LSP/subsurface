// SPDX-License-Identifier: GPL-2.0
#include "cloudsyncworkspacesyncreceive.h"
#include <QDir>
#include <QFileInfo>
#include <QLockFile>

namespace {
QString text(const QJsonObject &o, const char *key) { return o.value(QLatin1String(key)).toString(); }
}
CloudWorkspaceSyncReceive::CloudWorkspaceSyncReceive(QNetworkAccessManager *network, QObject *parent) :
	QObject(parent), reviewer(network, this), receiver(network, this)
{
	deadline.setSingleShot(true); deadline.setInterval(600000);
	connect(&deadline, &QTimer::timeout, this, [this] { fail(); });
	connect(&reviewer, &CloudWorkspaceSyncReview::failed, this, [this](const QString &, bool rejected) { fail(rejected); });
	connect(&reviewer, &CloudWorkspaceSyncReview::finished, this, &CloudWorkspaceSyncReceive::reviewed);
	connect(&receiver, &CloudWorkspaceReceive::failed, this, [this](const QString &, bool rejected) { fail(rejected); });
	connect(&receiver, &CloudWorkspaceReceive::applied, this, [this](const QString &directory) {
		if (step != Step::Receiving || !current()) { fail(); return; } emit applied(directory);
	});
	connect(&receiver, &CloudWorkspaceReceive::finished, this, &CloudWorkspaceSyncReceive::received);
}
CloudWorkspaceSyncReceive::~CloudWorkspaceSyncReceive() { cancel(); }
void CloudWorkspaceSyncReceive::cancel()
{
	++generation; step = Step::Idle; deadline.stop(); reviewer.cancel(); receiver.cancel(); plan.reset(); operationLock.reset();
	reference.value = {}; reviewedBinding = {}; priorPlan = false; token.clear(); choiceFile.clear(); choiceVersion.clear(); checkpointFile.clear(); bindingFile.clear();
	bindingRevision.clear(); nextRevision.clear(); baseline.clear(); target.clear(); reservation.clear(); operation.clear();
	planDirectory.clear(); readFile.clear(); recoveryDirectory.clear(); before.clear(); scopeCurrent = {}; localSnapshot = {}; projectPayload = {}; applyPayload = {};
}
void CloudWorkspaceSyncReceive::fail(bool rejected)
{
	if (step == Step::Idle) return;
	cancel(); emit failed(tr("The workspace receive needs review. Your local data, saved recovery copies and cloud versions were kept."), rejected);
}
void CloudWorkspaceSyncReceive::start(const QString &accessToken, const CloudWorkspaceReference &workspace, const QString &choice,
	const QString &choiceVer, const QString &checkpoint, const QString &binding, const QString &oldRevision, const QString &newRevision,
	const QString &base, const QString &commit, const QString &id, const QString &operationId, const QString &directory,
	const QString &readIntent, const QString &recovery, const QByteArray &beforeModel, const std::function<bool()> &guard,
	const std::function<QByteArray()> &snapshot, const std::function<QByteArray(const QByteArray &, const QByteArray &)> &project,
	const std::function<bool(const QByteArray &, const QByteArray &, const QByteArray &)> &apply, const QJsonObject &reviewed)
{
	cancel(); token = accessToken; reference = workspace; choiceFile = choice; choiceVersion = choiceVer; checkpointFile = checkpoint;
	bindingFile = binding; bindingRevision = oldRevision; nextRevision = newRevision; baseline = base; target = commit; reservation = id; operation = operationId;
	planDirectory = directory; readFile = readIntent; recoveryDirectory = recovery; before = beforeModel; reviewedBinding = reviewed;
	scopeCurrent = guard; localSnapshot = snapshot; projectPayload = project; applyPayload = apply; step = Step::InitialReview;
	if (token.isEmpty() || !reference.valid() || !CloudHistoryRecord::validEpoch(choiceVersion) ||
	    !CloudHistoryRecord::validEpoch(bindingRevision) || !CloudHistoryRecord::validEpoch(nextRevision) ||
	    !CloudHistoryRecord::validEpoch(baseline) || !CloudHistoryRecord::validEpoch(target) || !CloudHistoryRecord::validEpoch(reservation) ||
	    !CloudHistoryRecord::validEpoch(operation) || choiceFile.isEmpty() || checkpointFile.isEmpty() || bindingFile.isEmpty() ||
	    planDirectory.isEmpty() || readFile.isEmpty() || recoveryDirectory.isEmpty() || before.isEmpty() ||
	    before.size() > 64 * 1024 * 1024 || !CloudHistoryRecord::validXml(before) || !scopeCurrent || !localSnapshot || !projectPayload || !applyPayload) { fail(); return; }
	if (QFileInfo(planDirectory).isSymLink() || !QDir().mkpath(planDirectory)) { fail(); return; }
	operationLock = std::make_unique<QLockFile>(planDirectory + ".operation.lock"); operationLock->setStaleLockTime(0); if (!operationLock->tryLock(0)) { fail(); return; }
	priorPlan = QFileInfo::exists(QDir(planDirectory).filePath("plan.json"));
	plan = std::make_unique<CloudWorkspaceReceivePlan>(planDirectory, reference, choiceVersion, bindingRevision, nextRevision, baseline, target, reservation, operation, reviewedBinding);
	if (!plan->prepare(before) || !current()) { fail(); return; }
	const auto serial = generation; const auto snapshotFn = localSnapshot; const auto actual = snapshotFn(); if (serial != generation || step == Step::Idle) return;
	if (actual != before) {
		CloudWorkspaceReceiveReceipt receipt(recoveryDirectory, reference, choiceVersion, bindingRevision, baseline, target, reservation, nextRevision, reviewedBinding);
		if (!priorPlan || !receipt.reload() || (receipt.state() != "applying" && receipt.state() != "applied" && receipt.state() != "persisted") ||
		    receipt.before() != before || receipt.after() != actual) { fail(); return; }
	}
	deadline.start(); review(Step::InitialReview, actual);
}
bool CloudWorkspaceSyncReceive::current()
{
	if (step == Step::Idle) return false;
	const auto serial = generation; const auto guard = scopeCurrent; const bool allowed = guard && guard();
	if (serial != generation || step == Step::Idle) return false;
	if (!allowed) { fail(); return false; }
	CloudWorkspaceSelection choice(choiceFile, text(reference.value, "provider"), text(reference.value, "account"));
	if (!choice.load() || choice.version() != choiceVersion || choice.selected().value != reference.value ||
	    !plan || !plan->reload() || plan->before() != before) { fail(); return false; }
	return true;
}
void CloudWorkspaceSyncReceive::review(Step next, const QByteArray &expectedModel)
{
	if (!current()) return;
	step = next; reviewer.start(token, reference, choiceFile, choiceVersion, checkpointFile, bindingFile, expectedModel, [this] { return current(); }, localSnapshot);
}
void CloudWorkspaceSyncReceive::reviewed(int result, const QJsonObject &value, const QString &, const QString &revision)
{
	if ((step != Step::InitialReview && step != Step::PostReview) || !current()) { fail(); return; }
	using Action = CloudWorkspaceSyncDecision::Action; const auto action = Action(result); CloudWorkspaceRecord root; root.value = value;
	if (!root.valid() || action == Action::Invalid || action == Action::LegacyChanged ||
	    (revision != bindingRevision && revision != nextRevision)) { fail(); return; }
	CloudWorkspaceBinding binding(bindingFile, reference, choiceVersion);
	if (!binding.load() || binding.revision() != revision ||
	    (!reviewedBinding.isEmpty() && revision == bindingRevision && binding.value() != reviewedBinding)) { fail(); return; }
	CloudWorkspaceReceiveReceipt receipt(recoveryDirectory, reference, choiceVersion, bindingRevision, baseline, target, reservation, nextRevision, reviewedBinding);
	const bool known = receipt.reload();
	if (step == Step::PostReview) {
		// The selected model has already been installed and saved. Finish its
		// owned receipt even if a peer advanced the head, keeping that newer
		// version current so the next ordinary sync can receive it.
		if (revision != bindingRevision || !known || receipt.state() != "applied" || receipt.before() != before ||
		    text(root.ticket(target), "state") != "committed" || (action != Action::Conflict && action != Action::Receive)) { fail(); return; }
		step = Step::Receiving; receiver.confirmPersisted(); return;
	}
	if (revision == bindingRevision) {
		const bool initialReceive = root.head() == target && (action == Action::Receive || (!reviewedBinding.isEmpty() && action == Action::Conflict));
		const bool recovering = priorPlan && known && (receipt.state() == "applying" || receipt.state() == "applied" || receipt.state() == "persisted") &&
			receipt.before() == before && (action == Action::Conflict || action == Action::Receive);
		if (!initialReceive && !recovering) { fail(); return; }
	} else if (!priorPlan || !known || receipt.state() != "persisted") { fail(); return; }
	step = Step::Receiving;
	receiver.start(token, reference, baseline, target, reservation, choiceFile, choiceVersion, checkpointFile, bindingFile, bindingRevision, nextRevision,
		readFile, recoveryDirectory, before, [this] { return current(); }, localSnapshot, projectPayload, applyPayload, reviewedBinding);
}
void CloudWorkspaceSyncReceive::confirmPersisted()
{
	if (step != Step::Receiving || !current()) return;
	CloudWorkspaceReceiveReceipt receipt(recoveryDirectory, reference, choiceVersion, bindingRevision, baseline, target, reservation, nextRevision, reviewedBinding);
	if (!receipt.reload() || receipt.state() != "applied" || receipt.before() != before || receipt.after().isEmpty()) { fail(); return; }
	const auto serial = generation; const auto snapshot = localSnapshot; const auto actual = snapshot(); if (serial != generation || step == Step::Idle) return;
	if (actual != receipt.after()) { fail(); return; }
	review(Step::PostReview, actual);
}
void CloudWorkspaceSyncReceive::received(const QJsonObject &value, const QString &version, const QString &commit)
{
	if (step != Step::Receiving || commit != target || !current()) { fail(); return; }
	CloudWorkspaceRecord root; root.value = value; CloudWorkspaceAuthority authority(checkpointFile, reference);
	CloudWorkspaceBinding binding(bindingFile, reference, choiceVersion);
	CloudWorkspaceReceiveReceipt receipt(recoveryDirectory, reference, choiceVersion, bindingRevision, baseline, target, reservation, nextRevision, reviewedBinding);
	CloudWorkspaceReadIntent owned(readFile, reference, target, reservation);
	if (!root.valid() || !authority.load() || !binding.load() || !plan->complete(binding, root, authority.record(), receipt, owned)) { fail(); return; }
	const auto result = root.value; const auto resultVersion = version, resultTarget = target; const bool head = root.head() == target;
	cancel(); emit finished(result, resultVersion, resultTarget, head);
}
