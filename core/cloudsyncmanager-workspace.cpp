// SPDX-License-Identifier: GPL-2.0
#include "cloudsyncmanager.h"
#include "cloudsyncworkspacemodel.h"
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSettings>
#include <QStandardPaths>
#include <QUuid>

namespace {
QString uuid() { return QUuid::createUuid().toString(QUuid::WithoutBraces); }
QString text(const QJsonObject &o, const char *key) { return o.value(QLatin1String(key)).toString(); }
QJsonObject readCreation(const QString &directory, const QString &provider, const QString &account, const QString &nonce)
{
	if (!CloudHistoryRecord::validEpoch(nonce)) return {};
	const auto path = QDir(directory).filePath("creations/" + nonce + ".json"); const QFileInfo info(path);
	if (!info.isFile() || info.isSymLink() || info.size() > 8192) return {};
	QFile file(path); if (!file.open(QIODevice::ReadOnly)) return {};
	const auto bytes = file.read(8193); file.close(); if (bytes.size() > 8192) return {};
	auto body = QJsonDocument::fromJson(bytes).object();
	if (text(body, "provider") != provider || text(body, "account") != account || text(body, "nonce") != nonce) return {};
	CloudWorkspaceCreationIntent intent(path, provider, account, text(body, "epoch"), nonce, text(body, "origin_sha256"));
	if (!intent.reload()) return {};
	body["state"] = intent.state(); body["id"] = intent.id(); return body;
}
}

QString CloudSyncManager::workspaceDirectory(const QString &providerId, const QString &account) const
{
	const auto base = workspaceDataRootOverride.isEmpty() ? QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation) : workspaceDataRootOverride;
	if (base.isEmpty() || account.isEmpty()) return {};
	const auto scope = CloudSyncManifest::sha256((QSettings().fileName() + "|" + providerId + "|" + account).toUtf8());
	return QDir(base).filePath("neo-workspaces/" + scope);
}
QList<QJsonObject> CloudSyncManager::workspaceCreations(const QString &directory, const QString &providerId, const QString &account, bool *limited, bool *invalid) const
{
	if (limited) *limited = false; if (invalid) *invalid = false;
	QList<QJsonObject> result; int count = 0;
	QDirIterator files(QDir(directory).filePath("creations"), {"*.json"}, QDir::Files | QDir::NoSymLinks);
	while (files.hasNext()) {
		files.next(); if (++count > 64) break;
		const auto body = readCreation(directory, providerId, account, files.fileInfo().completeBaseName());
		if (body.isEmpty()) { if (invalid) *invalid = true; } else result.append(body);
	}
	if (limited) *limited = count >= 64;
	return result;
}
QList<QJsonObject> CloudSyncManager::workspaceCopies(const QString &directory, const CloudWorkspaceReference &reference, bool *limited, bool *invalid) const
{
	if (limited) *limited = false; if (invalid) *invalid = false;
	QList<QJsonObject> result; int count = 0;
	if (!reference.valid()) return result;
	QDirIterator files(QDir(directory).filePath("copies/" + reference.key()), {"intent.json"}, QDir::Files | QDir::NoSymLinks, QDirIterator::Subdirectories);
	while (files.hasNext()) {
		files.next(); if (++count > 64) break;
		const auto path = files.filePath(); const QFileInfo info(path); const auto parent = info.dir();
		const auto ticket = parent.dirName();
		QFile file(path); QJsonObject body;
		if (CloudHistoryRecord::validEpoch(ticket) && info.size() <= 8192 &&
		    parent.absolutePath() == QDir(directory).filePath("copies/" + reference.key() + "/" + ticket) && file.open(QIODevice::ReadOnly)) {
			body = QJsonDocument::fromJson(file.read(8193)).object(); file.close();
		}
		const auto source = CloudWorkspaceCopySource::load(parent.filePath("source.json"));
		CloudWorkspaceCopyIntent intent(path, reference, source, ticket, text(body, "parent"));
		if (body.isEmpty() || !source.valid() || !intent.inspect()) { if (invalid) *invalid = true; continue; }
		body["source"] = source.value; result.append(body);
	}
	if (limited) *limited = count >= 64;
	return result;
}
QVariantMap CloudSyncManager::workspaceReport(const QString &providerId) const
{
	if (!workspaceEnabled() || tokens.value(providerId).accountId.isEmpty()) return {};
	// The running service owns metadata reads/writes. Do not let UI polling
	// contend with its atomic save/readback or create a lock during termination.
	if (workspaceInProgress() && syncProviderId == providerId) return {};
	const auto account = CloudSyncManifest::sha256(tokens.value(providerId).accountId.toUtf8()), directory = workspaceDirectory(providerId, account);
	if (directory.isEmpty()) return {};
	CloudWorkspaceSelection choice(QDir(directory).filePath("selection.json"), providerId, account);
	if (!choice.inspect()) return {{"recoveryRequired", true}};
	auto report = workspaceReports.value(providerId);
	if (report.value("choiceVersion").toString() != choice.version()) report.clear();
	const auto selected = choice.selected(); const auto known = choice.knownReferences();
	report["selected"] = selected.valid(); report["reference"] = selected.valid() ? QString::fromUtf8(selected.json()) : QString();
	QVariantList saved;
	for (auto it = known.begin(); it != known.end(); ++it) {
		CloudWorkspaceReference ref; ref.value = it.value().toObject();
		saved.append(QVariantMap{{"key", ref.key()}, {"reference", QString::fromUtf8(ref.json())}, {"current", ref.key() == selected.key()}});
	}
	report["known"] = saved;
	bool copyLimited = false, copyInvalid = false; QVariantList copies;
	for (const auto &body : workspaceCopies(directory, selected, &copyLimited, &copyInvalid)) {
		const auto state = text(body, "state"); if (state == "committed") continue;
		copies.append(QVariantMap{{"id", text(body, "ticket")}, {"uncertain", state == "submitted"}, {"canResume", state != "submitted"}});
	}
	report["copies"] = copies; report["copyLimitReached"] = copyLimited; report["copyRecoveryRequired"] = copyInvalid;
	bool uploadLimited = false, uploadInvalid = false; QVariantList uploads;
	for (const auto &body : workspaceUploads(directory, selected, &uploadLimited, &uploadInvalid)) {
		const auto state = text(body, "state"); if (state == "committed") continue;
		uploads.append(QVariantMap{{"id", text(body, "ticket")}, {"uncertain", state == "submitted"}, {"canResume", state != "submitted"}});
	}
	report["uploads"] = uploads; report["uploadLimitReached"] = uploadLimited; report["uploadRecoveryRequired"] = uploadInvalid;
	bool restoreLimited = false, restoreInvalid = false;
	report["restores"] = workspaceRestores(directory, selected, choice.version(), &restoreLimited, &restoreInvalid);
	report["restoreLimitReached"] = restoreLimited; report["restoreRecoveryRequired"] = restoreInvalid;
	bool limited = false, invalid = false; QVariantList attempts;
	for (const auto &body : workspaceCreations(directory, providerId, account, &limited, &invalid)) {
		const auto ref = CloudWorkspaceReference::create(providerId, account, text(body, "epoch"), text(body, "id"));
		if (text(body, "state") == "bound" && known.contains(ref.key())) continue;
		attempts.append(QVariantMap{{"id", text(body, "nonce")}, {"uncertain", text(body, "state") == "submitted"},
			{"canResume", text(body, "state") == "prepared" || ref.valid()}});
	}
	report["setups"] = attempts; report["setupLimitReached"] = limited; report["recoveryRequired"] = invalid;
	return report;
}
bool CloudSyncManager::workspaceScopeCurrent()
{
	const bool current = workspaceInProgress() && workspaceEnabled() && !workspaceToken.isEmpty() && !autoSyncEnabled() && primaryProviderId().isEmpty() && backupProviderId().isEmpty() &&
		tokens.value(syncProviderId).accessToken == workspaceToken && verifiedAccountTokens.value(syncProviderId) == workspaceToken &&
		CloudSyncManifest::sha256(tokens.value(syncProviderId).accountId.toUtf8()) == workspaceAccount;
	if (!current) return false;
	if (workspaceAction == "version-save" || workspaceAction == "version-resume-save" ||
	    (workspaceAction == "version-restore" && workspaceRestoreStep == WorkspaceRestoreStep::None))
		return CloudSyncManifest::sha256(CloudWorkspaceModel::currentXml()) == syncLocalSha256;
	if ((workspaceAction == "version-restore" || workspaceAction == "version-resume-restore") &&
	    (workspaceRestoreStep == WorkspaceRestoreStep::Source || workspaceRestoreStep == WorkspaceRestoreStep::Publishing))
		return workspaceRestoreSnapshot() == workspaceRestoreBefore;
	return (workspaceAction != "inspect-copy" && workspaceAction != "copy" && workspaceAction != "resume-copy") || localDiveLogUnchanged();
}
void CloudSyncManager::failWorkspace(const QString &message, bool rejected)
{
	if (!workspaceInProgress()) return;
	if (rejected) fileStore.operationError(descriptorForId(syncProviderId)->type, {}, message, true);
	else { setError(message); clearSyncOperation(); }
	emit providersChanged();
}
void CloudSyncManager::cancelWorkspace()
{
	if (!workspaceInProgress()) return;
	cancelAccountCheck(); tokenClient.cancel(); pendingTokenContinuation = {}; refreshProviderId.clear();
	clearSyncOperation(); setError({}); emit providersChanged();
}
void CloudSyncManager::runWorkspace(const QString &providerId, const QString &action, const QString &argument)
{
	const auto *provider = descriptorForId(providerId);
	if (!workspaceEnabled() || !provider || provider->type == CloudSyncProviderType::SubsurfaceCloud || syncInProgress() ||
	    authorizationInProgress() || accountCheckInProgress() || pendingTokenContinuation) return;
	if (action == "refresh") { workspaceReports.remove(providerId); workspaceCopyReviews.remove(providerId); workspaceVersionReviews.remove(providerId); setError({}); emit providersChanged(); return; }
	if (autoSyncEnabled() || !primaryProviderId().isEmpty() || !backupProviderId().isEmpty()) {
		setError(tr("Experimental workspaces require automatic sync off and no primary or backup assignment. Use an isolated test profile.")); return;
	}
	const bool versioning = action == "version-list" || action == "version-save" || action == "version-resume-save" || action == "version-restore" || action == "version-resume-restore";
	if (action != "create" && action != "join" && action != "review" && action != "resume" && action != "inspect-copy" && action != "copy" && action != "resume-copy" && !versioning) { setError(tr("Choose a valid workspace action.")); return; }
	if ((action == "version-resume-save" || action == "version-resume-restore") && !CloudHistoryRecord::validEpoch(argument)) { setError(tr("The saved version attempt is invalid.")); return; }
	WorkspaceVersionReview versionReview; QString restoreSource;
	if (action == "version-restore") {
		const auto request = argument.size() <= 1024 ? QJsonDocument::fromJson(argument.toUtf8()).object() : QJsonObject();
		versionReview = workspaceVersionReviews.value(providerId); restoreSource = text(request, "source");
		if (request.size() != 2 || text(request, "review") != versionReview.id || !CloudHistoryRecord::validEpoch(versionReview.id) ||
		    !CloudHistoryRecord::validEpoch(restoreSource) || !versionReview.commits.contains(restoreSource)) {
			setError(tr("Review the workspace versions again before confirming a restore.")); return;
		}
	}
	if ((action == "version-restore" || action == "version-resume-restore") && (!workspacePersistenceAvailable || !workspacePersist || !workspacePersistenceAvailable())) {
		setError(tr("Open this profile's local dive log before restoring a workspace version.")); return;
	}
	const bool copying = action == "copy" || action == "resume-copy", inspecting = action == "inspect-copy";
	auto reviewed = action == "copy" ? workspaceCopyReviews.value(providerId) : WorkspaceCopyReview();
	if (action == "copy" && (!CloudHistoryRecord::validEpoch(argument) || reviewed.id != argument || !reviewed.source.valid())) {
		setError(tr("Review the current cloud log before confirming a copy.")); return;
	}
	if (action == "resume-copy" && !CloudHistoryRecord::validEpoch(argument)) { setError(tr("The saved copy attempt is invalid.")); return; }
	CloudWorkspaceReference reference;
	QString expectedAccount = tokens.value(providerId).accountId.isEmpty() ? QString() : CloudSyncManifest::sha256(tokens.value(providerId).accountId.toUtf8());
	if (action == "join") {
		if (argument.size() > 2048) { setError(tr("The workspace reference is too long.")); return; }
		reference = CloudWorkspaceReference::parse(argument.toUtf8());
		if (!reference.valid() || text(reference.value, "provider") != providerId) { setError(tr("This workspace reference is invalid or belongs to a different provider.")); return; }
		expectedAccount = text(reference.value, "account");
	}
	if (action == "resume" && !CloudHistoryRecord::validEpoch(argument)) { setError(tr("The saved workspace setup is invalid.")); return; }
	QString expectedVersion;
	if (!expectedAccount.isEmpty()) {
		const auto path = workspaceDirectory(providerId, expectedAccount);
		if (path.isEmpty()) { setError(tr("Workspace recovery records cannot be stored on this device.")); return; }
		CloudWorkspaceSelection choice(QDir(path).filePath("selection.json"), providerId, expectedAccount);
		if (!((copying || versioning) ? choice.inspect() : choice.load())) { setError(tr("The saved workspace choice needs recovery. All records were kept.")); return; }
		expectedVersion = choice.version();
		if (action == "review" || inspecting || copying || versioning) reference = choice.selected();
	}
	if ((action == "review" || inspecting || copying || versioning) && !reference.valid()) { setError(tr("Choose a workspace before verifying it.")); return; }
	QByteArray local;
	if (versioning) {
		local = CloudWorkspaceModel::currentXml();
		if (local.isEmpty() || local.size() > 64 * 1024 * 1024 || !CloudHistoryRecord::validXml(local)) { setError(tr("The current dive log cannot be saved as a workspace version.")); return; }
		if (action == "version-restore" && (versionReview.choiceVersion != expectedVersion || versionReview.reference.value != reference.value ||
		    versionReview.localSha256 != CloudSyncManifest::sha256(local))) {
			setError(tr("The local log or workspace choice changed. Review versions again.")); return;
		}
	}
	if (inspecting || copying) {
		local = serializeCurrentDiveLog(); if (local.isEmpty()) return;
		if (action == "copy" && (reviewed.choiceVersion != expectedVersion || reviewed.reference.value != reference.value || reviewed.localSha256 != CloudSyncManifest::sha256(local))) {
			setError(tr("The workspace choice or local log changed. Review the cloud log again before copying.")); return;
		}
	}
	clearStorageReview(providerId); setError({});
	syncProviderId = providerId; syncOperation = SyncOperation::Workspace; workspaceAction = action; workspaceChoiceVersion = expectedVersion; workspaceReference = reference;
	if (inspecting || copying) { syncLocalSha256 = CloudSyncManifest::sha256(local); workspaceCopyReview = reviewed; }
	if (versioning) { syncLocalPayload = local; syncLocalSha256 = CloudSyncManifest::sha256(local); }
	if (action == "version-restore") { workspaceRestoreReview = versionReview; workspaceRestoreSource = restoreSource; }
	// Keep the resume ticket separate from payload data; it is an immutable UUID.
	if (action == "version-resume-save" || action == "version-resume-restore") workspaceVersionTicket = argument;
	const auto serial = ++workspaceSerial; emit syncInProgressChanged(); verifiedAccountTokens.remove(providerId);
	if (!workspaceInProgress() || workspaceSerial != serial) return;
	refreshThen(*provider, providerId, [this, providerId, action, argument, expectedAccount, serial](const QString &token) {
		if (!workspaceInProgress() || syncProviderId != providerId || workspaceSerial != serial) return;
		workspaceToken = token; workspaceAccount = CloudSyncManifest::sha256(tokens.value(providerId).accountId.toUtf8()); workspacePath = workspaceDirectory(providerId, workspaceAccount);
		if (!workspaceScopeCurrent() || workspacePath.isEmpty() || (!expectedAccount.isEmpty() && expectedAccount != workspaceAccount)) {
			failWorkspace(tr("The connected account or workspace settings changed. Your local log was kept.")); return;
		}
		CloudWorkspaceSelection choice(QDir(workspacePath).filePath("selection.json"), providerId, workspaceAccount);
		const bool copying = action == "copy" || action == "resume-copy", versioning = action.startsWith("version-");
		if (!((copying || versioning) ? choice.inspect() : choice.load()) || choice.version() != workspaceChoiceVersion) { failWorkspace(tr("The workspace choice changed. Review it and try again; all records were kept.")); return; }
		if (versioning) {
			if (choice.selected().value != workspaceReference.value) { failWorkspace(tr("The selected workspace changed. All version attempts were kept.")); return; }
			CloudWorkspaceAuthority checkpoint(QDir(workspacePath).filePath("roots/" + workspaceReference.key() + ".json"), workspaceReference);
			if (!checkpoint.load() || !checkpoint.record().valid()) { failWorkspace(tr("This workspace's recovery record is missing or invalid. It was not reset.")); return; }
			workspaceCheckpointRequired = true;
			workspaceReader.start(token, providerId, workspaceAccount, workspaceReference, [this, serial] { return workspaceSerial == serial && workspaceScopeCurrent(); }, checkpoint.record()); return;
		}
		if (action == "copy" || action == "resume-copy") {
			if (choice.selected().value != workspaceReference.value) { failWorkspace(tr("The workspace choice changed. All copy attempts were kept.")); return; }
			bool copyLimited = false, copyInvalid = false;
			const auto copies = workspaceCopies(workspacePath, workspaceReference, &copyLimited, &copyInvalid);
			if (copyInvalid || (action == "copy" && copyLimited)) { failWorkspace(tr("Saved workspace copies need recovery or have reached their safety limit. All records were kept.")); return; }
			QString ticket = uuid(), parent = workspaceCopyReview.parent; auto source = workspaceCopyReview.source;
			if (action == "resume-copy") {
				QJsonObject saved; for (const auto &body : copies) if (text(body, "ticket") == argument) saved = body;
				if (saved.isEmpty()) { failWorkspace(tr("The saved copy could not be read. Its records were kept.")); return; }
				ticket = argument; parent = text(saved, "parent"); source.value = saved.value("source").toObject();
			}
			const auto directory = QDir(workspacePath).filePath("copies/" + workspaceReference.key() + "/" + ticket);
			workspaceCopy.start(token, workspaceReference, source, QDir(workspacePath).filePath("selection.json"), workspaceChoiceVersion,
				QDir(workspacePath).filePath("roots/" + workspaceReference.key() + ".json"), QDir(directory).filePath("intent.json"), QDir(directory).filePath("source.json"), ticket, parent,
				[this, serial] { return workspaceSerial == serial && workspaceScopeCurrent(); });
			return;
		}
		bool limited = false, invalid = false; const auto setups = workspaceCreations(workspacePath, providerId, workspaceAccount, &limited, &invalid);
		if (invalid) { failWorkspace(tr("A saved workspace setup needs recovery. All records were kept.")); return; }
		if (action == "create" || action == "resume") {
			QString epoch = uuid(), nonce = uuid(), origin;
			if (action == "create") {
				if (limited) { failWorkspace(tr("Too many workspace setups are saved. Existing records were kept.")); return; }
				origin = CloudSyncManifest::sha256(("neo-workspace-genesis|" + providerId + "|" + workspaceAccount + "|" + epoch + "|" + nonce).toUtf8());
			} else {
				const auto saved = readCreation(workspacePath, providerId, workspaceAccount, argument);
				if (saved.isEmpty()) { failWorkspace(tr("The saved workspace setup cannot be read. All records were kept.")); return; }
				epoch = text(saved, "epoch"); nonce = argument; origin = text(saved, "origin_sha256");
			}
			workspaceCreator.start(QDir(workspacePath).filePath("creations/" + nonce + ".json"), QDir(workspacePath).filePath("roots"), providerId, token,
				workspaceAccount, epoch, nonce, origin, [this, serial] { return workspaceSerial == serial && workspaceScopeCurrent(); });
		} else {
			CloudWorkspaceAuthority checkpoint(QDir(workspacePath).filePath("roots/" + workspaceReference.key() + ".json"), workspaceReference);
			workspaceCheckpointRequired = choice.knownReferences().contains(workspaceReference.key());
			for (const auto &saved : setups) {
				const auto ref = CloudWorkspaceReference::create(providerId, workspaceAccount, text(saved, "epoch"), text(saved, "id"));
				if (text(saved, "state") == "bound" && ref.key() == workspaceReference.key()) workspaceCheckpointRequired = true;
			}
			if (!checkpoint.load() || (workspaceCheckpointRequired && !checkpoint.record().valid())) { failWorkspace(tr("This workspace's recovery record is missing or invalid. It was not reset.")); return; }
			workspaceCheckpointRequired |= checkpoint.record().valid();
			workspaceReader.start(token, providerId, workspaceAccount, workspaceReference, [this, serial] { return workspaceSerial == serial && workspaceScopeCurrent(); }, checkpoint.record());
		}
	});
}
void CloudSyncManager::finishWorkspace(const QJsonObject &value, const QString &version)
{
	if (!workspaceInProgress()) return;
	if (workspaceAction.startsWith("version-")) { finishWorkspaceVersions(value, version); return; }
	CloudWorkspaceRecord root; root.value = value; const auto ref = root.reference();
	if (!workspaceScopeCurrent() || !root.valid() || !ref.matches(syncProviderId, workspaceAccount) || (workspaceReference.valid() && workspaceReference.value != ref.value)) {
		failWorkspace(tr("The workspace or account changed before it could be selected. Your local log was kept.")); return;
	}
	CloudWorkspaceAuthority checkpoint(QDir(workspacePath).filePath("roots/" + ref.key() + ".json"), ref);
	const bool requireExisting = workspaceCheckpointRequired || workspaceAction == "create" || workspaceAction == "resume" || workspaceAction == "copy" || workspaceAction == "resume-copy";
	if (!checkpoint.observe(root, version, requireExisting) || !workspaceScopeCurrent()) { failWorkspace(tr("The workspace recovery record could not be saved. The previous choice and local log were kept.")); return; }
	CloudWorkspaceSelection choice(QDir(workspacePath).filePath("selection.json"), syncProviderId, workspaceAccount);
	const bool selected = workspaceAction == "review" || workspaceAction == "inspect-copy" || workspaceAction == "copy" || workspaceAction == "resume-copy"
		? choice.load() && choice.version() == workspaceChoiceVersion && choice.selected().key() == ref.key()
		: choice.select(workspaceChoiceVersion, ref);
	if (!selected) { failWorkspace(tr("The workspace choice changed. The verified workspace was kept for recovery; review and try again.")); return; }
	workspaceReports[syncProviderId] = {{"verified", true}, {"choiceVersion", choice.version()}, {"verifiedAt", QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs)},
		{"versions", root.value.value("journal").toObject().value("history").toArray().size()}, {"empty", root.head().isEmpty()}};
	if (workspaceAction == "inspect-copy") {
		workspaceCopyReview.parent = root.head();
		const auto serial = workspaceSerial;
		workspaceSource.start(workspaceToken, syncProviderId, workspaceAccount, [this, serial] { return workspaceSerial == serial && workspaceScopeCurrent(); }); return;
	}
	if (workspaceAction == "copy" || workspaceAction == "resume-copy") {
		workspaceReports[syncProviderId]["copied"] = true; workspaceReports[syncProviderId]["legacyChanged"] = workspaceLegacyChanged;
	}
	const auto providerId = syncProviderId; clearSyncOperation(); emit providersChanged(); emit workspaceFinished(providerId);
}
void CloudSyncManager::finishWorkspaceSource(const QJsonObject &source)
{
	if (!workspaceInProgress() || workspaceAction != "inspect-copy") return;
	CloudWorkspaceCopySource from; from.value = source;
	CloudWorkspaceSelection choice(QDir(workspacePath).filePath("selection.json"), syncProviderId, workspaceAccount);
	if (!workspaceScopeCurrent() || !from.matches(syncProviderId, workspaceAccount) || !choice.load() || choice.version() != workspaceChoiceVersion || choice.selected().value != workspaceReference.value) {
		failWorkspace(tr("The cloud log or workspace changed during review. Your local log was kept.")); return;
	}
	workspaceCopyReview.id = uuid(); workspaceCopyReview.choiceVersion = workspaceChoiceVersion; workspaceCopyReview.reference = workspaceReference;
	workspaceCopyReview.source = from; workspaceCopyReview.localSha256 = syncLocalSha256;
	workspaceCopyReviews[syncProviderId] = workspaceCopyReview;
	const auto object = source.value("payload").toObject();
	workspaceReports[syncProviderId]["copyReview"] = QVariantMap{{"id", workspaceCopyReview.id}, {"bytes", object.value("bytes").toVariant()}, {"sha256", text(object, "sha256")}};
	const auto providerId = syncProviderId; clearSyncOperation(); emit providersChanged(); emit workspaceFinished(providerId);
}
