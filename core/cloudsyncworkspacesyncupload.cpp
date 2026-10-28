// SPDX-License-Identifier: GPL-2.0
#include "cloudsyncworkspacesyncupload.h"
#include "cloudsyncmanifest.h"
#include <QDir>
#include <QFileInfo>
#include <QLockFile>

namespace {
QString text(const QJsonObject &o, const char *key) { return o.value(QLatin1String(key)).toString(); }
}
CloudWorkspaceSyncUpload::CloudWorkspaceSyncUpload(QNetworkAccessManager *network, QObject *parent) :
	QObject(parent), reviewer(network, this), uploader(network, this), cloudReader(network, this)
{
	deadline.setSingleShot(true); deadline.setInterval(600000);
	connect(&deadline, &QTimer::timeout, this, [this] { fail(); });
	connect(&reviewer, &CloudWorkspaceSyncReview::failed, this, [this](const QString &, bool rejected) { fail(rejected); });
	connect(&reviewer, &CloudWorkspaceSyncReview::finished, this, &CloudWorkspaceSyncUpload::reviewed);
	connect(&uploader, &CloudWorkspaceUpload::failed, this, [this](const QString &, bool rejected) { fail(rejected); });
	connect(&uploader, &CloudWorkspaceUpload::finished, this, &CloudWorkspaceSyncUpload::uploaded);
	connect(&cloudReader, &CloudWorkspaceRead::failed, this, [this](const QString &, bool rejected) { fail(rejected); });
	connect(&cloudReader, &CloudWorkspaceRead::finished, this, &CloudWorkspaceSyncUpload::cloudRead);
	connect(&cloudReader, &CloudWorkspaceRead::released, this, &CloudWorkspaceSyncUpload::cloudReleased);
}
CloudWorkspaceSyncUpload::~CloudWorkspaceSyncUpload() { cancel(); }
void CloudWorkspaceSyncUpload::cancel()
{
	++generation; step = Step::Idle; deadline.stop(); reviewer.cancel(); uploader.cancel(); cloudReader.cancel(); plan.reset(); operationLock.reset();
	reference.value = {}; originalBinding = {}; reviewedBinding = {}; reviewedHead.clear(); priorPlan = false; token.clear(); choiceFile.clear(); choiceVersion.clear(); checkpointFile.clear();
	bindingFile.clear(); bindingRevision.clear(); nextRevision.clear(); baseline.clear(); ticket.clear(); operation.clear();
	planDirectory.clear(); intentFile.clear(); model.clear(); scopeCurrent = {}; localSnapshot = {};
}
void CloudWorkspaceSyncUpload::fail(bool rejected)
{
	if (step == Step::Idle) return;
	cancel(); emit failed(tr("The workspace upload needs review. Your local data, saved snapshot and cloud versions were kept."), rejected);
}
void CloudWorkspaceSyncUpload::start(const QString &accessToken, const CloudWorkspaceReference &workspace, const QString &choice,
	const QString &choiceVer, const QString &checkpoint, const QString &binding, const QString &oldRevision, const QString &newRevision,
	const QString &base, const QString &commit, const QString &id, const QString &directory, const QString &intent,
	const QByteArray &snapshot, const std::function<bool()> &guard, const std::function<QByteArray()> &local,
	const QString &conflictHead, const QJsonObject &conflictBinding)
{
	cancel(); token = accessToken; reference = workspace; choiceFile = choice; choiceVersion = choiceVer; checkpointFile = checkpoint;
	bindingFile = binding; bindingRevision = oldRevision; nextRevision = newRevision; baseline = base; ticket = commit; operation = id;
	planDirectory = directory; intentFile = intent; model = snapshot; scopeCurrent = guard; localSnapshot = local; reviewedHead = conflictHead; reviewedBinding = conflictBinding; step = Step::InitialReview;
	if (token.isEmpty() || !reference.valid() || !CloudHistoryRecord::validEpoch(choiceVersion) ||
	    !CloudHistoryRecord::validEpoch(bindingRevision) || !CloudHistoryRecord::validEpoch(nextRevision) ||
	    !CloudHistoryRecord::validEpoch(baseline) || !CloudHistoryRecord::validEpoch(ticket) || !CloudHistoryRecord::validEpoch(operation) ||
	    choiceFile.isEmpty() || checkpointFile.isEmpty() || bindingFile.isEmpty() || planDirectory.isEmpty() || intentFile.isEmpty() ||
	    model.isEmpty() || model.size() > 64 * 1024 * 1024 || !CloudHistoryRecord::validXml(model) || !scopeCurrent || !localSnapshot) { fail(); return; }
	if (QFileInfo(planDirectory).isSymLink() || !QDir().mkpath(planDirectory)) { fail(); return; }
	operationLock = std::make_unique<QLockFile>(planDirectory + ".operation.lock");
	operationLock->setStaleLockTime(0);
	if (!operationLock->tryLock(0)) { fail(); return; }
	priorPlan = QFileInfo::exists(QDir(planDirectory).filePath("plan.json"));
	plan = std::make_unique<CloudWorkspaceUploadPlan>(planDirectory, reference, choiceVersion, bindingRevision, nextRevision, baseline, ticket, operation, reviewedHead, reviewedBinding);
	CloudWorkspaceBinding initial(bindingFile, reference, choiceVersion);
	if (!initial.load() || (initial.revision() != bindingRevision && initial.revision() != nextRevision) ||
	    (initial.revision() == nextRevision && !priorPlan)) { fail(); return; }
	if (initial.revision() == bindingRevision) { originalBinding = initial.value(); if (!reviewedHead.isEmpty() && originalBinding != reviewedBinding) { fail(); return; } }
	if (!plan->prepare(model) || !current()) { fail(); return; }
	deadline.start(); review(Step::InitialReview);
}
bool CloudWorkspaceSyncUpload::current()
{
	if (step == Step::Idle) return false;
	const auto serial = generation; const auto guard = scopeCurrent; const bool allowed = guard && guard();
	if (serial != generation || step == Step::Idle) return false;
	if (!allowed) { fail(); return false; }
	CloudWorkspaceSelection choice(choiceFile, text(reference.value, "provider"), text(reference.value, "account"));
	CloudWorkspaceBinding binding(bindingFile, reference, choiceVersion);
	if (!choice.load() || choice.version() != choiceVersion || choice.selected().value != reference.value ||
	    !binding.load() || (binding.revision() != bindingRevision && binding.revision() != nextRevision) ||
	    (binding.revision() == bindingRevision && (originalBinding.isEmpty() || binding.value() != originalBinding)) ||
	    !plan || !plan->reload() || plan->payload() != model) { fail(); return false; }
	const auto snapshot = localSnapshot; const auto actual = snapshot(); if (serial != generation || step == Step::Idle) return false;
	if (actual != model) { fail(); return false; }
	return true;
}
void CloudWorkspaceSyncUpload::review(Step next)
{
	if (!current()) return;
	step = next; reviewer.start(token, reference, choiceFile, choiceVersion, checkpointFile, bindingFile, model, [this] { return current(); }, localSnapshot);
}
void CloudWorkspaceSyncUpload::reviewed(int result, const QJsonObject &value, const QString &version, const QString &revision)
{
	if ((step != Step::InitialReview && step != Step::AfterCloudRead && step != Step::FinalReview) || !current()) { fail(); return; }
	using Action = CloudWorkspaceSyncDecision::Action; const auto action = Action(result); CloudWorkspaceRecord root; root.value = value;
	CloudWorkspaceAuthority authority(checkpointFile, reference); CloudWorkspaceBinding binding(bindingFile, reference, choiceVersion);
	if (!root.valid() || !authority.load() || !binding.load() || binding.revision() != revision ||
	    action == Action::Invalid || action == Action::LegacyChanged) { fail(); return; }
	if (binding.revision() == nextRevision) {
		if (!priorPlan) { fail(); return; }
		if (!reviewedHead.isEmpty()) { releaseCloud(); return; }
		if (!plan->complete(binding, root, authority.record(), intentFile)) { fail(); return; }
		const auto resultRoot = root.value; const auto resultVersion = version, resultTicket = ticket; const bool head = root.head() == ticket;
		cancel(); emit finished(resultRoot, resultVersion, resultTicket, head); return;
	}
	if (binding.revision() != bindingRevision || binding.commit() != baseline) { fail(); return; }
	const bool conflict = !reviewedHead.isEmpty();
	const auto parent = conflict ? reviewedHead : baseline;
	if (step == Step::FinalReview) {
		if (action != Action::Conflict || root.head() != ticket) { fail(); return; }
		if (conflict && !reviewedCloudProof(root, false)) { fail(); return; }
		const bool advanced = conflict ? binding.advanceConflictUpload(reviewedBinding, reviewedHead, root, authority.record(), model, intentFile, nextRevision) :
			binding.advanceOwnedUpload(bindingRevision, root, authority.record(), model, model, intentFile, nextRevision);
		if (!advanced || !current()) { fail(); return; }
		if (conflict) { releaseCloud(); return; }
		if (!plan->complete(binding, root, authority.record(), intentFile)) { fail(); return; }
		const auto resultRoot = root.value; const auto resultVersion = version, resultTicket = ticket; cancel(); emit finished(resultRoot, resultVersion, resultTicket, true); return;
	}
	const bool newUpload = conflict ? (action == Action::Conflict && root.head() == reviewedHead && binding.value() == reviewedBinding) :
		(action == Action::Upload && root.head() == baseline);
	if (!newUpload) {
		if (!priorPlan || action != Action::Conflict || root.head() != ticket) { fail(); return; }
		CloudWorkspaceUploadIntent owned(intentFile, reference, CloudSyncManifest::sha256(model), model.size(), ticket, parent);
		if (!owned.reload() || owned.state() == "submitted") { fail(); return; }
	}
	if (conflict && step == Step::InitialReview) {
		step = Step::ReadingCloud;
		cloudReader.start(CloudWorkspaceRead::Action::Read, token, reference, reviewedHead, operation, choiceFile, choiceVersion,
			checkpointFile, QDir(QFileInfo(planDirectory).absolutePath()).filePath("reviewed-read.json"), [this] { return current(); });
		return;
	}
	if (conflict && (step != Step::AfterCloudRead || !reviewedCloudProof(root, false))) { fail(); return; }
	step = Step::Uploading;
	uploader.start(token, reference, model, choiceFile, choiceVersion, checkpointFile, intentFile, ticket, parent, [this] { return current(); });
}

bool CloudWorkspaceSyncUpload::reviewedCloudProof(const CloudWorkspaceRecord &root, bool released)
{
	CloudWorkspaceReadIntent reader(QDir(QFileInfo(planDirectory).absolutePath()).filePath("reviewed-read.json"), reference, reviewedHead, operation);
	const auto target = root.ticket(reviewedHead);
	const QJsonObject expected{{"name", target.value("name")}, {"object", target.value("object")},
		{"version", target.value("version")}, {"sha256", target.value("sha256")}, {"bytes", target.value("bytes")}};
	const auto record = root.value.value("journal").toObject().value("reservations").toObject().value(operation).toObject();
	return root.valid() && root.reference().value == reference.value && reader.reload() &&
		reader.state() == (released ? "released" : "verified") && reader.snapshot() == expected &&
		text(record, "commit") == reviewedHead && record.value("active") == QJsonValue(!released);
}
void CloudWorkspaceSyncUpload::cloudRead(const QJsonObject &value, const QString &, const QString &commit, const QString &reservation, const QByteArray &payload)
{
	CloudWorkspaceRecord root; root.value = value;
	if (step != Step::ReadingCloud || commit != reviewedHead || reservation != operation || payload.isEmpty() ||
	    !current() || !reviewedCloudProof(root, false) || (root.head() != reviewedHead && root.head() != ticket)) { fail(); return; }
	review(Step::AfterCloudRead);
}
void CloudWorkspaceSyncUpload::releaseCloud()
{
	if (!current()) return;
	step = Step::ReleasingCloud;
	cloudReader.start(CloudWorkspaceRead::Action::Release, token, reference, reviewedHead, operation, choiceFile, choiceVersion,
		checkpointFile, QDir(QFileInfo(planDirectory).absolutePath()).filePath("reviewed-read.json"), [this] { return current(); });
}
void CloudWorkspaceSyncUpload::cloudReleased(const QJsonObject &value, const QString &version, const QString &reservation)
{
	CloudWorkspaceRecord root; root.value = value;
	CloudWorkspaceAuthority authority(checkpointFile, reference);
	CloudWorkspaceBinding binding(bindingFile, reference, choiceVersion);
	if (step != Step::ReleasingCloud || reservation != operation || !current() || !reviewedCloudProof(root, true) ||
	    !authority.load() || !binding.load() || !plan->complete(binding, root, authority.record(), intentFile)) { fail(); return; }
	const auto resultRoot = root.value; const auto resultTicket = ticket; const bool head = root.head() == ticket;
	cancel(); emit finished(resultRoot, version, resultTicket, head);
}
void CloudWorkspaceSyncUpload::uploaded(const QJsonObject &, const QString &, const QString &id, bool headMatches)
{
	if (step != Step::Uploading || id != ticket || !headMatches || !current()) { fail(); return; }
	review(Step::FinalReview);
}
