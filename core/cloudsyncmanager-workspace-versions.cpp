// SPDX-License-Identifier: GPL-2.0
#include "cloudsyncmanager.h"
#include "cloudsyncworkspacemodel.h"
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QUuid>

namespace {
QString text(const QJsonObject &object, const char *key) { return object.value(QLatin1String(key)).toString(); }
}

QList<QJsonObject> CloudSyncManager::workspaceUploads(const QString &directory, const CloudWorkspaceReference &reference, bool *limited, bool *invalid) const
{
	if (limited) *limited = false; if (invalid) *invalid = false;
	QList<QJsonObject> result; if (!reference.valid()) return result;
	const QDir uploads(QDir(directory).filePath("versions/" + reference.key() + "/uploads"));
	const auto entries = uploads.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot | QDir::NoSymLinks, QDir::Name);
	if (limited) *limited = entries.size() >= 64;
	for (int i = 0; i < qMin(64, int(entries.size())); ++i) {
		const auto ticket = entries[i].fileName(), path = QDir(entries[i].absoluteFilePath()).filePath("intent.json");
		const QFileInfo info(path); QFile file(path); QJsonObject body;
		if (CloudHistoryRecord::validEpoch(ticket) && info.isFile() && !info.isSymLink() && info.size() <= 8192 && file.open(QIODevice::ReadOnly))
			body = QJsonDocument::fromJson(file.read(8193)).object();
		const double length = body.value("payload_bytes").toDouble(-1);
		const bool validLength = length >= 1 && length <= 64 * 1024 * 1024 && length == qint64(length);
		CloudWorkspaceUploadIntent intent(path, reference, text(body, "payload_sha256"), validLength ? qint64(length) : -1, ticket, text(body, "parent"));
		// A restore uses its own plan and paths. Never resume it as an ordinary save.
		if (body.isEmpty() || !intent.inspect()) { if (invalid) *invalid = true; continue; }
		result.append(body);
	}
	return result;
}

void CloudSyncManager::runWorkspaceVersion(const QString &providerId, const QString &action, const QString &argument)
{
	if (action != "list" && action != "save" && action != "resume-save" && action != "restore" && action != "resume-restore") { setError(tr("Choose a valid workspace version action.")); return; }
	runWorkspace(providerId, "version-" + action, argument);
}

void CloudSyncManager::finishWorkspaceVersions(const QJsonObject &value, const QString &version, const QString &completedTicket, bool headMatches)
{
	if (!workspaceInProgress() || !workspaceAction.startsWith("version-")) return;
	CloudWorkspaceRecord root; root.value = value; const auto reference = root.reference();
	CloudWorkspaceSelection choice(QDir(workspacePath).filePath("selection.json"), syncProviderId, workspaceAccount);
	if (!workspaceScopeCurrent() || !root.valid() || reference.value != workspaceReference.value || !reference.matches(syncProviderId, workspaceAccount) ||
	    !choice.inspect() || choice.version() != workspaceChoiceVersion || choice.selected().value != reference.value) {
		failWorkspace(tr("The workspace, account or local log changed. All saved versions and attempts were kept.")); return;
	}
	const auto checkpointFile = QDir(workspacePath).filePath("roots/" + reference.key() + ".json");
	CloudWorkspaceAuthority checkpoint(checkpointFile, reference);
	if (!checkpoint.observe(root, version, true)) { failWorkspace(tr("The workspace recovery record could not be updated. All versions were kept.")); return; }
	if (workspaceAction == "version-restore" || workspaceAction == "version-resume-restore") { startWorkspaceRestore(value); return; }
	if (completedTicket.isEmpty() && workspaceAction != "version-list") {
		bool limited = false;
		const auto uploads = workspaceUploads(workspacePath, reference, &limited);
		QString ticket = workspaceVersionTicket, parent = root.head(); QJsonObject owned;
		for (const auto &body : uploads) {
			if ((workspaceAction == "version-resume-save" && text(body, "ticket") == ticket) ||
			    (workspaceAction == "version-save" && text(body, "ticket") == root.head() && text(body, "state") == "committed" &&
			     text(body, "payload_sha256") == syncLocalSha256 && body.value("payload_bytes") == QJsonValue(double(syncLocalPayload.size())))) { owned = body; break; }
		}
		if (workspaceAction == "version-resume-save" && owned.isEmpty()) { failWorkspace(tr("The saved version attempt cannot be read. Its records were kept.")); return; }
		if (!owned.isEmpty()) {
			if (text(owned, "state") == "submitted" || text(owned, "payload_sha256") != syncLocalSha256 || owned.value("payload_bytes") != QJsonValue(double(syncLocalPayload.size()))) {
				failWorkspace(tr("This attempt has an unknown upload result or different local contents. It was kept. Save the current log as a separate version.")); return;
			}
			ticket = text(owned, "ticket"); parent = text(owned, "parent");
		} else {
			if (limited) { failWorkspace(tr("Saved version attempts have reached their safety limit. Existing attempts were kept.")); return; }
			ticket = QUuid::createUuid().toString(QUuid::WithoutBraces);
		}
		const auto intentFile = QDir(workspacePath).filePath("versions/" + reference.key() + "/uploads/" + ticket + "/intent.json");
		const auto serial = workspaceSerial;
		workspaceUpload.start(workspaceToken, reference, syncLocalPayload, QDir(workspacePath).filePath("selection.json"), workspaceChoiceVersion,
			checkpointFile, intentFile, ticket, parent, [this, serial] { return workspaceSerial == serial && workspaceScopeCurrent(); }); return;
	}
	QVariantList rows; const auto history = root.value.value("journal").toObject().value("history").toArray();
	for (int i = int(history.size()) - 1; i >= 0; --i) {
		const auto id = history[i].toString(); const auto ticket = root.ticket(id);
		rows.append(QVariantMap{{"id", id}, {"number", i + 1}, {"head", id == root.head()}, {"bytes", ticket.value("bytes").toVariant()},
			{"restored", !text(ticket, "restore_source").isEmpty()}});
	}
	WorkspaceVersionReview review; review.id = QUuid::createUuid().toString(QUuid::WithoutBraces); review.choiceVersion = workspaceChoiceVersion;
	review.reference = reference; review.parent = root.head(); const auto local = CloudWorkspaceModel::currentXml(); review.localSha256 = CloudSyncManifest::sha256(local);
	for (const auto &entry : history) review.commits.append(entry.toString());
	workspaceVersionReviews[syncProviderId] = review;
	const auto head = root.ticket(root.head());
	const bool localProtected = !root.head().isEmpty() && head.value("sha256") == QJsonValue(review.localSha256) && head.value("bytes") == QJsonValue(double(local.size()));
	workspaceReports[syncProviderId] = {{"verified", true}, {"choiceVersion", workspaceChoiceVersion}, {"verifiedAt", QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs)},
		{"versions", history.size()}, {"empty", root.head().isEmpty()}, {"versionRows", rows}, {"versionReview", review.id}, {"localProtected", localProtected}};
	if (!completedTicket.isEmpty()) {
		workspaceReports[syncProviderId]["savedVersion"] = completedTicket;
		workspaceReports[syncProviderId]["savedVersionIsHead"] = headMatches;
	}
	const auto providerId = syncProviderId; clearSyncOperation(); emit providersChanged(); emit workspaceFinished(providerId);
}
