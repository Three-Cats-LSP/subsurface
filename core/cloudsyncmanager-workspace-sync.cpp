// SPDX-License-Identifier: GPL-2.0
#include "cloudsyncmanager.h"
#include "cloudsyncworkspacemodel.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QUuid>

namespace {
QString id() { return QUuid::createUuid().toString(QUuid::WithoutBraces); }
QString field(const QJsonObject &o, const char *key) { return o.value(QLatin1String(key)).toString(); }
QJsonObject readPlan(const QString &path)
{
	const QFileInfo info(path);
	if (!info.isFile() || info.isSymLink() || info.size() < 1 || info.size() > 16384) return {};
	QFile file(path); if (!file.open(QIODevice::ReadOnly)) return {};
	const auto bytes = file.read(16385);
	return file.error() == QFileDevice::NoError && bytes.size() <= 16384 ? QJsonDocument::fromJson(bytes).object() : QJsonObject();
}
bool safePath(const QStringList &paths)
{
	for (const auto &path : paths) if (path.isEmpty() || QFileInfo(path).isSymLink()) return false;
	return true;
}
QByteArray project(const QByteArray &raw, const QByteArray &before)
{
	CloudWorkspaceModel model;
	return model.prepare(raw, before) ? model.normalizedXml() : QByteArray();
}
bool apply(const QByteArray &raw, const QByteArray &before, const QByteArray &after)
{
	if (CloudWorkspaceModel::currentXml() != before) return false;
	CloudWorkspaceModel model;
	return model.prepare(raw, before) && model.normalizedXml() == after && model.install() && CloudWorkspaceModel::currentXml() == after;
}
}

void CloudSyncManager::resolveWorkspaceConflict(const QString &providerId, const QString &reviewId, const QString &choice)
{
	const auto review = workspaceConflictReviews.value(providerId);
	if ((choice != QLatin1String("device") && choice != QLatin1String("cloud")) || !CloudHistoryRecord::validEpoch(reviewId) || review.id != reviewId ||
	    syncInProgress() || authorizationInProgress() || accountCheckInProgress() || !workspaceRouteConfirmed(providerId)) {
		setError(tr("Review the current conflict before choosing a version. Your data was kept.")); return;
	}
	const auto local = CloudWorkspaceModel::currentXml();
	if (CloudSyncManifest::sha256(local) != review.localSha256) {
		workspaceConflictReviews.remove(providerId);
		workspaceReports[providerId].remove("syncConflict"); workspaceReports[providerId].remove("conflictReview");
		emit providersChanged();
		setError(tr("The local log changed. Review the conflict again.")); return;
	}
	workspaceConflictRequested = reviewId; workspaceConflictProvider = providerId; workspaceConflictChoice = choice;
	syncDiveLog(providerId);
	if (!workspaceSyncInProgress()) { workspaceConflictRequested.clear(); workspaceConflictProvider.clear(); workspaceConflictChoice.clear(); }
}

bool CloudSyncManager::workspaceSyncScopeCurrent()
{
	if (!workspaceSyncInProgress() || !routeLease || !routeLease->held() || routeProvider != syncProviderId ||
	    !workspaceRouteConfirmed(syncProviderId) || workspaceSyncToken.isEmpty() ||
	    tokens.value(syncProviderId).accessToken != workspaceSyncToken || verifiedAccountTokens.value(syncProviderId) != workspaceSyncToken ||
	    CloudSyncManifest::sha256(tokens.value(syncProviderId).accountId.toUtf8()) != workspaceSyncAccount) return false;
	CloudWorkspaceRoute route(workspaceRoutePath(syncProviderId) + ".json", syncProviderId);
	CloudWorkspaceSelection choice(QDir(workspaceSyncPath).filePath("selection.json"), syncProviderId, workspaceSyncAccount);
	return route.load() && route.state() == "active" && route.value() == workspaceSyncRoute && choice.inspect() &&
		choice.version() == workspaceSyncChoiceVersion && choice.selected().value == workspaceSyncReference.value;
}

void CloudSyncManager::startWorkspaceSync(const QString &providerId)
{
	const auto *provider = descriptorForId(providerId);
	if (!provider || !acquireCloudRoute(providerId, false)) return;
	auto fail = [this] {
		setError(tr("The workspace sync route needs review. Your local log and cloud versions were kept."));
		routeLease.reset(); routeProvider.clear(); legacyRouteOperation = false;
	};
	CloudWorkspaceRoute route(workspaceRoutePath(providerId) + ".json", providerId);
	if (!workspaceRouteConfirmed(providerId) || !route.load() || route.state() != "active") { fail(); return; }
	const auto routeValue = route.value(); CloudWorkspaceReference ref; ref.value = routeValue.value("workspace").toObject();
	const auto account = field(ref.value, "account"), choiceVersion = field(routeValue, "choice_version");
	const auto path = workspaceDirectory(providerId, account), operation = field(routeValue, "operation");
	const auto activationBase = QDir(path).filePath("activations"), activationScope = QDir(activationBase).filePath(ref.key());
	const auto activationDir = QDir(activationScope).filePath(operation);
	if (!ref.matches(providerId, account) || path.isEmpty() || !safePath({path, activationBase, activationScope, activationDir})) { fail(); return; }
	CloudWorkspaceSelection choice(QDir(path).filePath("selection.json"), providerId, account);
	CloudWorkspaceAuthority checkpoint(QDir(path).filePath("roots/" + ref.key() + ".json"), ref);
	CloudWorkspaceBinding binding(QDir(path).filePath("bindings/" + ref.key() + ".json"), ref, choiceVersion);
	const auto body = readPlan(QDir(activationDir).filePath("plan.json"));
	CloudWorkspaceActivationPlan activation(activationDir, ref, choiceVersion, field(routeValue, "binding_revision"),
		field(body, "target"), field(body, "reservation"), operation, field(body, "acceptance"));
	if (!choice.inspect() || choice.version() != choiceVersion || choice.selected().value != ref.value ||
	    !checkpoint.load() || !checkpoint.record().valid() || !binding.load() || binding.value().isEmpty() ||
	    !activation.reload() || activation.state() != "completed" || activation.model().isEmpty()) { fail(); return; }
	bool restoreLimited = false, restoreInvalid = false;
	if (!workspaceRestores(path, ref, choiceVersion, &restoreLimited, &restoreInvalid).isEmpty() || restoreLimited || restoreInvalid) {
		setError(tr("Finish or review the saved workspace restore before syncing. All recovery records were kept."));
		routeLease.reset(); routeProvider.clear(); legacyRouteOperation = false; return;
	}
	const auto model = CloudWorkspaceModel::currentXml();
	if (model.isEmpty() || model.size() > 64 * 1024 * 1024 || !CloudHistoryRecord::validXml(model)) { fail(); return; }
	workspaceSyncAccount = account; workspaceSyncReference = ref; workspaceSyncRoute = routeValue; workspaceSyncPath = path;
	workspaceSyncChoiceVersion = choiceVersion; workspaceSyncModel = model;
	workspaceSyncCheckpointFile = QDir(path).filePath("roots/" + ref.key() + ".json");
	workspaceSyncBindingFile = QDir(path).filePath("bindings/" + ref.key() + ".json");
	syncProviderId = providerId; syncOperation = SyncOperation::WorkspaceSync; setError({});
	const auto serial = ++workspaceSerial; emit syncInProgressChanged();
	if (!workspaceSyncInProgress() || workspaceSerial != serial) return;
	verifiedAccountTokens.remove(providerId);
	refreshThen(*provider, providerId, [this, serial, providerId](const QString &token) {
		if (!workspaceSyncInProgress() || workspaceSerial != serial || syncProviderId != providerId) return;
		workspaceSyncToken = token;
		if (!workspaceSyncScopeCurrent()) { setError(tr("The workspace account or selection changed. Your data was kept.")); clearSyncOperation(); return; }
		if (resumeWorkspaceSync()) return;
		workspaceSyncReview.start(token, workspaceSyncReference, QDir(workspaceSyncPath).filePath("selection.json"), workspaceSyncChoiceVersion,
			workspaceSyncCheckpointFile, workspaceSyncBindingFile, workspaceSyncModel,
			[this, serial] { return workspaceSerial == serial && workspaceSyncScopeCurrent(); }, [] { return CloudWorkspaceModel::currentXml(); });
	});
}

bool CloudSyncManager::resumeWorkspaceSync()
{
	QJsonObject pending; QString pendingDir, kind;
	for (const auto &type : {QStringLiteral("sync-uploads"), QStringLiteral("sync-receives")}) {
		const auto base = QDir(workspaceSyncPath).filePath(type), scope = QDir(base).filePath(workspaceSyncReference.key());
		if (!safePath({workspaceSyncPath, base, scope})) { setError(tr("Workspace recovery storage needs review. All records were kept.")); clearSyncOperation(); return true; }
		QDir directory(scope); const auto entries = directory.entryInfoList(QDir::AllEntries | QDir::Hidden | QDir::System | QDir::NoDotAndDotDot);
		const bool limited = entries.size() > 64;
		for (int index = 0; index < qMin(64, int(entries.size())); ++index) {
			const auto entry = entries[index];
			const auto attempt = entry.fileName();
			if (!entry.isDir() || entry.isSymLink() || !CloudHistoryRecord::validEpoch(attempt)) { setError(tr("Workspace recovery records need review. All records were kept.")); clearSyncOperation(); return true; }
			const auto dir = QDir(entry.filePath()).filePath("plan");
			if (!safePath({entry.filePath(), dir})) { setError(tr("Workspace recovery records need review. All records were kept.")); clearSyncOperation(); return true; }
			const auto body = readPlan(QDir(dir).filePath("plan.json"));
			bool valid = false, completedProof = false; QString state;
			CloudWorkspaceAuthority authority(workspaceSyncCheckpointFile, workspaceSyncReference);
			CloudWorkspaceBinding binding(workspaceSyncBindingFile, workspaceSyncReference, workspaceSyncChoiceVersion);
			if (type == "sync-uploads") {
				CloudWorkspaceUploadPlan plan(dir, workspaceSyncReference, workspaceSyncChoiceVersion, field(body, "binding_revision"), field(body, "next_revision"),
					field(body, "baseline"), field(body, "ticket"), attempt, field(body, "reviewed_head"), body.value("reviewed_binding").toObject());
				valid = plan.reload(); state = plan.state();
				if (valid && state == "completed" && authority.load() && binding.load())
					completedProof = plan.complete(binding, authority.record(), authority.record(), QDir(entry.filePath()).filePath("intent.json"));
			} else {
				CloudWorkspaceReceivePlan plan(dir, workspaceSyncReference, workspaceSyncChoiceVersion, field(body, "binding_revision"), field(body, "next_revision"), field(body, "baseline"), field(body, "target"), field(body, "reservation"), attempt, body.value("reviewed_binding").toObject());
				valid = plan.reload(); state = plan.state();
				if (valid && state == "completed" && authority.load() && binding.load()) {
					CloudWorkspaceReceiveReceipt receipt(QDir(entry.filePath()).filePath("recovery"), workspaceSyncReference, workspaceSyncChoiceVersion,
						field(body, "binding_revision"), field(body, "baseline"), field(body, "target"), field(body, "reservation"), field(body, "next_revision"), body.value("reviewed_binding").toObject());
					CloudWorkspaceReadIntent reader(QDir(entry.filePath()).filePath("read.json"), workspaceSyncReference, field(body, "target"), field(body, "reservation"));
					completedProof = plan.complete(binding, authority.record(), authority.record(), receipt, reader);
				}
			}
			if (!valid) { setError(tr("Workspace recovery records need review. All records were kept.")); clearSyncOperation(); return true; }
			if (state == "completed") {
				if (!completedProof) { setError(tr("Completed workspace records need review. All recovery files were kept.")); clearSyncOperation(); return true; }
				// Keep completed recovery records, but move them out of the
				// bounded active scan. Rename within the same local volume;
				// an interrupted move leaves either location intact.
				const auto archive = QDir(workspaceSyncPath).filePath("sync-completed"), kind = QDir(archive).filePath(type);
				const auto destination = QDir(kind).filePath(workspaceSyncReference.key()), target = QDir(destination).filePath(attempt);
				if (!safePath({archive, kind, destination, target}) || QFileInfo::exists(target) || !QDir().mkpath(destination) || !QDir().rename(entry.filePath(), target)) {
					setError(tr("Completed workspace records could not be archived. All recovery files were kept.")); clearSyncOperation(); return true;
				}
				continue;
			}
			if (!pending.isEmpty()) { setError(tr("Multiple workspace sync attempts need review. All records were kept.")); clearSyncOperation(); return true; }
			pending = body; pendingDir = entry.filePath(); kind = type;
		}
		if (limited) { setError(tr("More workspace records need review. Completed records were kept; try syncing again.")); clearSyncOperation(); return true; }
	}
	if (pending.isEmpty()) return false;
	if (!workspaceConflictRequested.isEmpty()) {
		setError(tr("Finish the saved workspace sync before resolving this conflict. All records were kept."));
		clearSyncOperation(); return true;
	}
	const auto dir = QDir(pendingDir).filePath("plan"), choice = QDir(workspaceSyncPath).filePath("selection.json");
	const auto operation = field(pending, "operation"), old = field(pending, "binding_revision"), next = field(pending, "next_revision"), baseline = field(pending, "baseline");
	const auto serial = workspaceSerial; auto guard = [this, serial] { return workspaceSerial == serial && workspaceSyncScopeCurrent(); };
	if (kind == "sync-uploads") {
		CloudWorkspaceUploadPlan plan(dir, workspaceSyncReference, workspaceSyncChoiceVersion, old, next, baseline, field(pending, "ticket"), operation,
			field(pending, "reviewed_head"), pending.value("reviewed_binding").toObject());
		if (!plan.reload() || (plan.payload().isEmpty() && !plan.prepare(workspaceSyncModel)) || plan.payload() != workspaceSyncModel) {
			setError(tr("A saved upload needs review. Your local log and recovery copy were kept.")); clearSyncOperation(); return true;
		}
		workspaceSyncUpload.start(workspaceSyncToken, workspaceSyncReference, choice, workspaceSyncChoiceVersion, workspaceSyncCheckpointFile, workspaceSyncBindingFile,
			old, next, baseline, field(pending, "ticket"), operation, dir, QDir(pendingDir).filePath("intent.json"), plan.payload(), guard,
			[] { return CloudWorkspaceModel::currentXml(); }, field(pending, "reviewed_head"), pending.value("reviewed_binding").toObject());
	} else {
		CloudWorkspaceReceivePlan plan(dir, workspaceSyncReference, workspaceSyncChoiceVersion, old, next, baseline, field(pending, "target"), field(pending, "reservation"), operation, pending.value("reviewed_binding").toObject());
		if (!workspacePersistenceAvailable || !workspacePersist || !workspacePersistenceAvailable() || !plan.reload() ||
		    (plan.before().isEmpty() && !plan.prepare(workspaceSyncModel)) || plan.before().isEmpty() ||
		    !safePath({QDir(pendingDir).filePath("recovery")})) {
			setError(tr("A saved receive needs review. Your local log and recovery copies were kept.")); clearSyncOperation(); return true;
		}
		workspaceSyncReceive.start(workspaceSyncToken, workspaceSyncReference, choice, workspaceSyncChoiceVersion, workspaceSyncCheckpointFile, workspaceSyncBindingFile,
			old, next, baseline, field(pending, "target"), field(pending, "reservation"), operation, dir, QDir(pendingDir).filePath("read.json"),
			QDir(pendingDir).filePath("recovery"), plan.before(), guard, [] { return CloudWorkspaceModel::currentXml(); }, project, apply, pending.value("reviewed_binding").toObject());
	}
	return true;
}

void CloudSyncManager::workspaceSyncReviewed(int result, const QJsonObject &value, const QString &, const QString &revision)
{
	using Action = CloudWorkspaceSyncDecision::Action;
	if (!workspaceSyncScopeCurrent() || CloudWorkspaceModel::currentXml() != workspaceSyncModel) { setError(tr("The workspace or local log changed. Your data was kept.")); clearSyncOperation(); return; }
	CloudWorkspaceRecord root; root.value = value;
	CloudWorkspaceBinding binding(workspaceSyncBindingFile, workspaceSyncReference, workspaceSyncChoiceVersion);
	if (!root.valid() || root.reference().value != workspaceSyncReference.value || !binding.load() || binding.revision() != revision) {
		setError(tr("The workspace sync baseline needs review. Your data was kept.")); clearSyncOperation(); return;
	}
	const auto action = Action(result); const auto provider = syncProviderId;
	WorkspaceConflictReview selected;
	if (!workspaceConflictRequested.isEmpty()) {
		selected = workspaceConflictReviews.value(provider);
		if (workspaceConflictProvider != provider || selected.id != workspaceConflictRequested ||
		    selected.choiceVersion != workspaceSyncChoiceVersion || selected.reference.value != workspaceSyncReference.value ||
		    selected.localSha256 != CloudSyncManifest::sha256(workspaceSyncModel) || selected.head != root.head() ||
		    selected.binding != binding.value() || action != Action::Conflict) {
			workspaceConflictReviews.remove(provider);
			workspaceReports[provider].remove("syncConflict"); workspaceReports[provider].remove("conflictReview");
			setError(tr("The conflict changed. Review both versions again; your data was kept."));
			clearSyncOperation(); return;
		}
	}
	if (action == Action::Conflict && selected.id.isEmpty()) {
		WorkspaceConflictReview review;
		review.id = id(); review.choiceVersion = workspaceSyncChoiceVersion; review.reference = workspaceSyncReference;
		review.localSha256 = CloudSyncManifest::sha256(workspaceSyncModel); review.head = root.head(); review.binding = binding.value();
		workspaceConflictReviews[provider] = review;
		workspaceReports[provider] = {{"choiceVersion", workspaceSyncChoiceVersion}, {"syncConflict", true}, {"conflictReview", review.id}};
		setError(tr("This device and the workspace both changed. Both versions were kept; review them before syncing."));
		clearSyncOperation(); return;
	}
	workspaceReports[provider].remove("syncConflict");
	if (action == Action::InSync) { recordSuccessfulSync(provider); clearSyncOperation(); emit diveLogSyncFinished(provider, "up-to-date"); return; }
	if (action != Action::Upload && action != Action::Receive && selected.id.isEmpty()) {
		setError(action == Action::LegacyChanged ? tr("The previous cloud log changed. Both branches were kept; review them before continuing.") :
			tr("The workspace needs review. Your local log and cloud versions were kept."));
		clearSyncOperation(); return;
	}
	const bool upload = action == Action::Upload || (!selected.id.isEmpty() && workspaceConflictChoice == "device");
	if (!upload && (!workspacePersistenceAvailable || !workspacePersist || !workspacePersistenceAvailable())) {
		setError(tr("The local dive log cannot be saved safely. Cloud and local versions were kept.")); clearSyncOperation(); return;
	}
	const auto operation = id(), type = upload ? QStringLiteral("sync-uploads") : QStringLiteral("sync-receives");
	const auto base = QDir(workspaceSyncPath).filePath(type), scope = QDir(base).filePath(workspaceSyncReference.key()), attempt = QDir(scope).filePath(operation), plan = QDir(attempt).filePath("plan");
	if (!safePath({workspaceSyncPath, base, scope, attempt, plan, QDir(attempt).filePath("recovery")})) { setError(tr("Workspace recovery storage needs review. Your data was kept.")); clearSyncOperation(); return; }
	const auto serial = workspaceSerial; const auto next = id(); auto guard = [this, serial] { return workspaceSerial == serial && workspaceSyncScopeCurrent(); };
	const auto choice = QDir(workspaceSyncPath).filePath("selection.json");
	if (upload) {
		workspaceSyncUpload.start(workspaceSyncToken, workspaceSyncReference, choice, workspaceSyncChoiceVersion, workspaceSyncCheckpointFile, workspaceSyncBindingFile,
			revision, next, binding.commit(), id(), operation, plan, QDir(attempt).filePath("intent.json"), workspaceSyncModel, guard,
			[] { return CloudWorkspaceModel::currentXml(); }, selected.head, selected.binding);
	} else {
		workspaceSyncReceive.start(workspaceSyncToken, workspaceSyncReference, choice, workspaceSyncChoiceVersion, workspaceSyncCheckpointFile, workspaceSyncBindingFile,
			revision, next, binding.commit(), root.head(), id(), operation, plan, QDir(attempt).filePath("read.json"), QDir(attempt).filePath("recovery"),
			workspaceSyncModel, guard, [] { return CloudWorkspaceModel::currentXml(); }, project, apply, selected.binding);
	}
}

void CloudSyncManager::workspaceSyncApplied(const QString &)
{
	const auto serial = workspaceSerial;
	if (!workspaceSyncScopeCurrent() || !workspacePersistenceAvailable || !workspacePersist || !workspacePersistenceAvailable() || !workspacePersist()) {
		if (workspaceSyncInProgress() && workspaceSerial == serial) { setError(tr("The received dive log could not be saved. Recovery copies were kept.")); clearSyncOperation(); }
		return;
	}
	if (workspaceSyncInProgress() && workspaceSerial == serial) workspaceSyncReceive.confirmPersisted();
}

void CloudSyncManager::workspaceSyncCompleted(const QJsonObject &, const QString &, const QString &, bool headMatches, bool received)
{
	if (!workspaceSyncScopeCurrent()) { setError(tr("The workspace changed during sync. Your data and recovery records were kept.")); clearSyncOperation(); return; }
	const auto provider = syncProviderId;
	workspaceConflictReviews.remove(provider);
	if (headMatches) recordSuccessfulSync(provider);
	clearSyncOperation();
	if (!headMatches) {
		setError(tr("The workspace changed again during sync. The saved versions were kept; sync again to review the latest version."));
		return;
	}
	emit diveLogSyncFinished(provider, received ? "downloaded" : "uploaded");
}
