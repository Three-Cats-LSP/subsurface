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
	QObject(parent), reviewer(network, this), uploader(network, this)
{
	deadline.setSingleShot(true); deadline.setInterval(600000);
	connect(&deadline, &QTimer::timeout, this, [this] { fail(); });
	connect(&reviewer, &CloudWorkspaceSyncReview::failed, this, [this](const QString &, bool rejected) { fail(rejected); });
	connect(&reviewer, &CloudWorkspaceSyncReview::finished, this, &CloudWorkspaceSyncUpload::reviewed);
	connect(&uploader, &CloudWorkspaceUpload::failed, this, [this](const QString &, bool rejected) { fail(rejected); });
	connect(&uploader, &CloudWorkspaceUpload::finished, this, &CloudWorkspaceSyncUpload::uploaded);
}
CloudWorkspaceSyncUpload::~CloudWorkspaceSyncUpload() { cancel(); }
void CloudWorkspaceSyncUpload::cancel()
{
	++generation; step = Step::Idle; deadline.stop(); reviewer.cancel(); uploader.cancel(); plan.reset(); operationLock.reset();
	reference.value = {}; originalBinding = {}; priorPlan = false; token.clear(); choiceFile.clear(); choiceVersion.clear(); checkpointFile.clear();
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
	const QByteArray &snapshot, const std::function<bool()> &guard, const std::function<QByteArray()> &local)
{
	cancel(); token = accessToken; reference = workspace; choiceFile = choice; choiceVersion = choiceVer; checkpointFile = checkpoint;
	bindingFile = binding; bindingRevision = oldRevision; nextRevision = newRevision; baseline = base; ticket = commit; operation = id;
	planDirectory = directory; intentFile = intent; model = snapshot; scopeCurrent = guard; localSnapshot = local; step = Step::InitialReview;
	if (token.isEmpty() || !reference.valid() || !CloudHistoryRecord::validEpoch(choiceVersion) ||
	    !CloudHistoryRecord::validEpoch(bindingRevision) || !CloudHistoryRecord::validEpoch(nextRevision) ||
	    !CloudHistoryRecord::validEpoch(baseline) || !CloudHistoryRecord::validEpoch(ticket) || !CloudHistoryRecord::validEpoch(operation) ||
	    choiceFile.isEmpty() || checkpointFile.isEmpty() || bindingFile.isEmpty() || planDirectory.isEmpty() || intentFile.isEmpty() ||
	    model.isEmpty() || model.size() > 64 * 1024 * 1024 || !CloudHistoryRecord::validXml(model) || !scopeCurrent || !localSnapshot) { fail(); return; }
	if (QFileInfo(planDirectory).isSymLink() || !QDir().mkpath(planDirectory)) { fail(); return; }
	operationLock = std::make_unique<QLockFile>(planDirectory + ".operation.lock");
	if (!operationLock->tryLock(0)) { fail(); return; }
	priorPlan = QFileInfo::exists(QDir(planDirectory).filePath("plan.json"));
	plan = std::make_unique<CloudWorkspaceUploadPlan>(planDirectory, reference, choiceVersion, bindingRevision, nextRevision, baseline, ticket, operation);
	CloudWorkspaceBinding initial(bindingFile, reference, choiceVersion);
	if (!initial.load() || (initial.revision() != bindingRevision && initial.revision() != nextRevision) ||
	    (initial.revision() == nextRevision && !priorPlan)) { fail(); return; }
	if (initial.revision() == bindingRevision) originalBinding = initial.value();
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
	if ((step != Step::InitialReview && step != Step::FinalReview) || !current()) { fail(); return; }
	using Action = CloudWorkspaceSyncDecision::Action; const auto action = Action(result); CloudWorkspaceRecord root; root.value = value;
	CloudWorkspaceAuthority authority(checkpointFile, reference); CloudWorkspaceBinding binding(bindingFile, reference, choiceVersion);
	if (!root.valid() || !authority.load() || !binding.load() || binding.revision() != revision ||
	    action == Action::Invalid || action == Action::LegacyChanged) { fail(); return; }
	if (binding.revision() == nextRevision) {
		if (!priorPlan || !plan->complete(binding, root, authority.record(), intentFile)) { fail(); return; }
		const auto resultRoot = root.value; const auto resultVersion = version, resultTicket = ticket; const bool head = root.head() == ticket;
		cancel(); emit finished(resultRoot, resultVersion, resultTicket, head); return;
	}
	if (binding.revision() != bindingRevision || binding.commit() != baseline) { fail(); return; }
	if (step == Step::FinalReview) {
		if (action != Action::Conflict || root.head() != ticket ||
		    !binding.advanceOwnedUpload(bindingRevision, root, authority.record(), model, model, intentFile, nextRevision) ||
		    !plan->complete(binding, root, authority.record(), intentFile) || !current()) { fail(); return; }
		const auto resultRoot = root.value; const auto resultVersion = version, resultTicket = ticket; cancel(); emit finished(resultRoot, resultVersion, resultTicket, true); return;
	}
	const bool newUpload = action == Action::Upload && root.head() == baseline;
	if (!newUpload) {
		if (!priorPlan || action != Action::Conflict || root.head() != ticket) { fail(); return; }
		CloudWorkspaceUploadIntent owned(intentFile, reference, CloudSyncManifest::sha256(model), model.size(), ticket, baseline);
		if (!owned.reload() || owned.state() == "submitted") { fail(); return; }
	}
	step = Step::Uploading;
	uploader.start(token, reference, model, choiceFile, choiceVersion, checkpointFile, intentFile, ticket, baseline, [this] { return current(); });
}
void CloudWorkspaceSyncUpload::uploaded(const QJsonObject &, const QString &, const QString &id, bool headMatches)
{
	if (step != Step::Uploading || id != ticket || !headMatches || !current()) { fail(); return; }
	review(Step::FinalReview);
}
