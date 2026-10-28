// SPDX-License-Identifier: GPL-2.0
#include "cloudsyncmanager.h"
#include "cloudsyncworkspacemodel.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QRegularExpression>
#include <QSaveFile>
#include <QTimer>
#include <QUuid>

namespace {
constexpr qint64 XmlLimit = 64 * 1024 * 1024;
QString str(const QJsonObject &o, const char *key) { return o.value(QLatin1String(key)).toString(); }
QString id() { return QUuid::createUuid().toString(QUuid::WithoutBraces); }
QByteArray json(const QJsonObject &o) { return QJsonDocument(o).toJson(QJsonDocument::Compact); }
QByteArray readFile(const QString &path, qint64 limit)
{
	const QFileInfo info(path); if (!info.isFile() || info.isSymLink() || info.size() < 1 || info.size() > limit) return {};
	QFile f(path); if (!f.open(QIODevice::ReadOnly)) return {}; const auto bytes = f.read(limit + 1);
	return bytes.size() <= limit && f.error() == QFileDevice::NoError ? bytes : QByteArray();
}
QJsonObject readObject(const QString &path, qint64 limit = 16384)
{
	return QJsonDocument::fromJson(readFile(path, limit)).object();
}
bool keepFile(const QString &path, const QByteArray &bytes, qint64 limit)
{
	if (bytes.isEmpty() || bytes.size() > limit) return false;
	const QFileInfo info(path); if (info.isSymLink()) return false;
	if (info.exists()) return readFile(path, limit) == bytes;
	QSaveFile file(path);
	return file.open(QIODevice::WriteOnly) && file.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner) &&
		file.write(bytes) == bytes.size() && file.commit() && readFile(path, limit) == bytes;
}
bool hash(const QString &s) { return QRegularExpression("^[a-f0-9]{64}$").match(s).hasMatch(); }
bool validPlan(const QJsonObject &p, const CloudWorkspaceReference &reference, const QString &ticket)
{
	const auto parent = str(p, "parent"), source = str(p, "source"), sourceReservation = str(p, "source_reservation"), applyReservation = str(p, "apply_reservation");
	const double length = p.value("before_bytes").toDouble(-1), modelLength = p.value("before_model_bytes").toDouble(-1);
	const bool ordinary = p.size() == 13 && str(p, "format") == "neo-workspace-restore-plan-development-1";
	const bool bound = p.size() == 14 && str(p, "format") == "neo-workspace-restore-plan-development-2" && p.value("binding_required") == QJsonValue(true);
	return (ordinary || bound) && p.value("schema") == QJsonValue(1) &&
		p.value("workspace").toObject() == reference.value && CloudHistoryRecord::validEpoch(str(p, "choice_version")) &&
		CloudHistoryRecord::validEpoch(ticket) && str(p, "ticket") == ticket && CloudHistoryRecord::validEpoch(parent) &&
		CloudHistoryRecord::validEpoch(source) && CloudHistoryRecord::validEpoch(sourceReservation) && CloudHistoryRecord::validEpoch(applyReservation) &&
		ticket != parent && ticket != source && ticket != sourceReservation && ticket != applyReservation && sourceReservation != applyReservation &&
		sourceReservation != source && sourceReservation != parent && applyReservation != source && applyReservation != parent &&
		hash(str(p, "before_sha256")) && length >= 1 && length <= XmlLimit && length == qint64(length) &&
		hash(str(p, "before_model_sha256")) && modelLength >= 1 && modelLength <= XmlLimit && modelLength == qint64(modelLength);
}
QString path(const QString &directory, const QString &name) { return QDir(directory).filePath(name); }
bool matchesBefore(const QJsonObject &plan, const QByteArray &bytes)
{
	return !bytes.isEmpty() && bytes.size() == qint64(plan.value("before_bytes").toDouble()) &&
		CloudSyncManifest::sha256(bytes) == str(plan, "before_sha256") && CloudHistoryRecord::validXml(bytes);
}
bool validBindingIntent(const QJsonObject &intent, const QJsonObject &plan, const CloudWorkspaceReference &reference, const QString &choice)
{
	const auto prior = intent.value("prior").toObject(), route = intent.value("route").toObject();
	return intent.size() == 8 && intent.value("schema") == QJsonValue(1) && str(intent, "format") == "neo-workspace-restore-binding-development-1" &&
		intent.value("workspace") == reference.value && str(intent, "choice_version") == choice &&
		str(intent, "plan_sha256") == CloudSyncManifest::sha256(json(plan)) && str(intent, "next_revision") == str(plan, "ticket") &&
		prior.value("workspace") == reference.value && str(prior, "choice_version") == choice && str(prior, "commit") == str(plan, "parent") &&
		CloudHistoryRecord::validEpoch(str(prior, "revision")) && str(prior, "revision") != str(plan, "ticket") &&
		CloudHistoryRecord::validEpoch(str(prior, "legacy_acceptance")) && route.value("workspace") == reference.value &&
		str(route, "choice_version") == choice && str(route, "state") == "active";
}
}

bool CloudSyncManager::prepareWorkspaceRestoreBinding(bool resuming)
{
	if (!workspaceRestoreUsesBinding || !workspaceScopeCurrent() || workspaceRestoreDirectory.isEmpty() ||
	    str(workspaceRestorePlan, "format") != "neo-workspace-restore-plan-development-2" || workspaceRestorePlan.value("binding_required") != QJsonValue(true)) return false;
	const auto file = path(workspaceRestoreDirectory, "binding-intent.json"), expectedPlan = CloudSyncManifest::sha256(json(workspaceRestorePlan));
	const auto bindingFile = path(workspacePath, "bindings/" + workspaceReference.key() + ".json");
	CloudWorkspaceBinding binding(bindingFile, workspaceReference, workspaceChoiceVersion);
	if (!binding.load() || binding.value().isEmpty()) return false;
	const auto target = str(workspaceRestorePlan, "ticket"), parent = str(workspaceRestorePlan, "parent");
	QJsonObject intent = readObject(file);
	if (intent.isEmpty()) {
		const QFileInfo info(file); if (info.exists() || info.isSymLink()) return false;
		// A crash between saving the restore plan and this intent is recoverable
		// only before any read/upload/application record has been created.
		const auto occupied = [](const QString &p) { const QFileInfo info(p); return info.exists() || info.isSymLink(); };
		if (resuming && (occupied(path(workspaceRestoreDirectory, "source-read.json")) ||
		    occupied(path(workspaceRestoreDirectory, "upload.json")) || occupied(path(workspaceRestoreDirectory, "local")))) return false;
		const auto currentModel = CloudWorkspaceModel::currentXml();
		if (binding.commit() != parent || binding.revision() == target || !binding.matchesModel(workspaceRestoreBefore) ||
		    (currentModel != workspaceRestoreBefore && currentModel != workspaceRestoreBeforeModel)) return false;
		intent = {{"schema", 1}, {"format", "neo-workspace-restore-binding-development-1"}, {"workspace", workspaceReference.value},
			{"choice_version", workspaceChoiceVersion}, {"plan_sha256", expectedPlan}, {"prior", binding.value()},
			{"next_revision", target}, {"route", workspaceRestoreRoute}};
		if (!keepFile(file, json(intent), 16384)) return false;
	}
	if (!validBindingIntent(intent, workspaceRestorePlan, workspaceReference, workspaceChoiceVersion) || intent.value("route").toObject() != workspaceRestoreRoute) return false;
	const auto prior = intent.value("prior").toObject(), current = binding.value();
	if (prior.isEmpty() || prior.value("workspace") != workspaceReference.value || str(prior, "choice_version") != workspaceChoiceVersion ||
	    str(prior, "commit") != parent || !CloudHistoryRecord::validEpoch(str(prior, "revision")) || str(prior, "revision") == target) return false;
	if (current != prior) {
		const QJsonObject previous{{"revision", prior.value("revision")}, {"commit", prior.value("commit")}, {"raw", prior.value("raw")}, {"model", prior.value("model")}};
		if (binding.revision() != target || current.value("previous").toObject() != previous || current.value("legacy_acceptance") != prior.value("legacy_acceptance")) return false;
	}
	workspaceRestoreBindingIntent = intent; return true;
}

QVariantList CloudSyncManager::workspaceRestores(const QString &directory, const CloudWorkspaceReference &reference, const QString &choiceVersion, bool *limited, bool *invalid) const
{
	if (limited) *limited = false; if (invalid) *invalid = false;
	QVariantList result; if (!reference.valid()) return result;
	const auto versions = path(directory, "versions"), scope = path(versions, reference.key()), restoreBase = path(scope, "restores");
	for (const auto &p : {directory, versions, scope, restoreBase}) if (QFileInfo(p).isSymLink()) { if (invalid) *invalid = true; return result; }
	const QDir base(restoreBase);
	const auto entries = base.entryInfoList(QDir::AllEntries | QDir::Hidden | QDir::System | QDir::NoDotAndDotDot, QDir::Name);
	if (limited) *limited = entries.size() >= 64;
	for (int i = 0; i < qMin(64, int(entries.size())); ++i) {
		if (!entries[i].isDir() || entries[i].isSymLink()) { if (invalid) *invalid = true; continue; }
		const auto attempt = entries[i].fileName(), dir = entries[i].absoluteFilePath(); const auto plan = readObject(path(dir, "plan.json"));
		if (!validPlan(plan, reference, attempt)) { if (invalid) *invalid = true; continue; }
		const auto upload = readObject(path(dir, "upload.json")); const auto applied = readObject(path(dir, "local/intent.json"));
		const auto source = readObject(path(dir, "source-read.json")), application = readObject(path(dir, "apply-read.json"));
		CloudWorkspaceReadIntent sourceRead(path(dir, "source-read.json"), reference, str(plan, "source"), str(plan, "source_reservation"));
		CloudWorkspaceReadIntent appRead(path(dir, "apply-read.json"), reference, attempt, str(plan, "apply_reservation"));
		if (str(applied, "state") == "persisted" && applied.value("workspace").toObject() == reference.value && str(applied, "choice_version") == str(plan, "choice_version") &&
		    str(applied, "commit") == attempt && str(applied, "reservation") == str(plan, "apply_reservation") && sourceRead.inspect() && appRead.inspect() &&
		    sourceRead.state() == "released" && appRead.state() == "released") {
			const auto bindingFile = path(dir, "binding-intent.json"), completeFile = path(dir, "binding-complete.json"); const QFileInfo bindingInfo(bindingFile);
			if (!bindingInfo.exists() && !bindingInfo.isSymLink()) {
				if (plan.value("binding_required") != QJsonValue(true)) continue; // Legacy manual restore.
				if (invalid) *invalid = true;
			}
			const auto bindingBytes = readFile(bindingFile, 16384); const auto binding = QJsonDocument::fromJson(bindingBytes).object(), completed = readObject(completeFile);
			if (!bindingBytes.isEmpty() && validBindingIntent(binding, plan, reference, str(plan, "choice_version")) && completed.size() == 4 && completed.value("schema") == QJsonValue(1) &&
			    str(completed, "format") == "neo-workspace-restore-binding-complete-development-1" && str(completed, "ticket") == attempt &&
			    str(completed, "intent_sha256") == CloudSyncManifest::sha256(bindingBytes)) continue;
			if ((bindingBytes.isEmpty() || !validBindingIntent(binding, plan, reference, str(plan, "choice_version")) || QFileInfo(completeFile).isSymLink()) && invalid) *invalid = true;
		}
		const QFileInfo uploadInfo(path(dir, "upload.json"));
		const bool corrupt = (uploadInfo.exists() || uploadInfo.isSymLink()) && (upload.isEmpty() || !QStringList{"prepared", "registering", "registered", "submitted", "identified", "verified", "uploaded", "committed"}.contains(str(upload, "state")));
		if (corrupt && invalid) *invalid = true;
		const bool uncertain = str(upload, "state") == "submitted", choiceChanged = str(plan, "choice_version") != choiceVersion;
		result.append(QVariantMap{{"id", attempt}, {"uncertain", uncertain}, {"choiceChanged", choiceChanged}, {"canResume", !uncertain && !choiceChanged && !corrupt}});
	}
	return result;
}

void CloudSyncManager::startWorkspaceRestore(const QJsonObject &value)
{
	CloudWorkspaceRecord root; root.value = value;
	if (!workspaceScopeCurrent() || !workspacePersistenceAvailable || !workspacePersistenceAvailable()) { failWorkspace(tr("The local destination changed. Your log and restore records were kept.")); return; }
	const auto versions = path(workspacePath, "versions"), scope = path(versions, workspaceReference.key()), base = path(scope, "restores");
	if (QFileInfo(workspacePath).isSymLink() || QFileInfo(versions).isSymLink() || QFileInfo(scope).isSymLink() || QFileInfo(base).isSymLink()) {
		failWorkspace(tr("The restore storage location needs review. Existing files were kept.")); return;
	}
	if (workspaceAction == "version-restore") {
		const auto head = root.ticket(root.head());
		if (root.head() != workspaceRestoreReview.parent || root.head().isEmpty() || !workspaceRestoreReview.commits.contains(workspaceRestoreSource) ||
		    !root.value.value("journal").toObject().value("history").toArray().contains(workspaceRestoreSource) ||
		    head.value("sha256") != QJsonValue(syncLocalSha256) || head.value("bytes") != QJsonValue(double(syncLocalPayload.size()))) {
			failWorkspace(tr("Save the current local log as a workspace version, then review versions before restoring.")); return;
		}
		bool limited = false; workspaceRestores(workspacePath, workspaceReference, workspaceChoiceVersion, &limited);
		if (limited || !QDir().mkpath(base)) { failWorkspace(tr("Saved restore attempts have reached their safety limit. All attempts were kept.")); return; }
		CloudWorkspaceModel baselineModel;
		if (!baselineModel.prepare(syncLocalPayload, syncLocalPayload)) { failWorkspace(tr("The current local log could not be prepared for recovery.")); return; }
		workspaceRestoreBeforeModel = baselineModel.normalizedXml();
		const auto ticket = id(), sourceReservation = id(), applyReservation = id();
		workspaceRestoreDirectory = path(base, ticket); workspaceRestoreBefore = syncLocalPayload;
		workspaceRestorePlan = {{"schema", 1}, {"format", "neo-workspace-restore-plan-development-1"}, {"workspace", workspaceReference.value},
			{"choice_version", workspaceChoiceVersion}, {"parent", root.head()}, {"source", workspaceRestoreSource}, {"ticket", ticket},
			{"source_reservation", sourceReservation}, {"apply_reservation", applyReservation}, {"before_sha256", syncLocalSha256}, {"before_bytes", double(syncLocalPayload.size())},
			{"before_model_sha256", CloudSyncManifest::sha256(workspaceRestoreBeforeModel)}, {"before_model_bytes", double(workspaceRestoreBeforeModel.size())}};
		if (workspaceRestoreUsesBinding) { workspaceRestorePlan["format"] = "neo-workspace-restore-plan-development-2"; workspaceRestorePlan["binding_required"] = true; }
		if (!validPlan(workspaceRestorePlan, workspaceReference, ticket) || !QDir(base).mkdir(ticket) ||
		    !keepFile(path(workspaceRestoreDirectory, "before.xml"), workspaceRestoreBefore, XmlLimit) ||
		    !keepFile(path(workspaceRestoreDirectory, "before-model.xml"), workspaceRestoreBeforeModel, XmlLimit) ||
		    !keepFile(path(workspaceRestoreDirectory, "plan.json"), json(workspaceRestorePlan), 16384)) {
			failWorkspace(tr("The restore plan could not be saved. Existing records and your local log were kept.")); return;
		}
		if (workspaceRestoreUsesBinding && !prepareWorkspaceRestoreBinding(false)) { failWorkspace(tr("The restore binding could not be prepared. All recovery records were kept.")); return; }
	} else {
		workspaceRestoreDirectory = path(base, workspaceVersionTicket);
		if (QFileInfo(workspaceRestoreDirectory).isSymLink()) { failWorkspace(tr("The saved restore needs review. Its records were kept.")); return; }
		workspaceRestorePlan = readObject(path(workspaceRestoreDirectory, "plan.json"));
		workspaceRestoreBefore = readFile(path(workspaceRestoreDirectory, "before.xml"), XmlLimit);
		workspaceRestoreBeforeModel = readFile(path(workspaceRestoreDirectory, "before-model.xml"), XmlLimit);
		CloudWorkspaceModel baselineModel;
		if (!validPlan(workspaceRestorePlan, workspaceReference, workspaceVersionTicket) || str(workspaceRestorePlan, "choice_version") != workspaceChoiceVersion || !matchesBefore(workspaceRestorePlan, workspaceRestoreBefore) ||
		    workspaceRestoreBeforeModel.size() != qint64(workspaceRestorePlan.value("before_model_bytes").toDouble()) || CloudSyncManifest::sha256(workspaceRestoreBeforeModel) != str(workspaceRestorePlan, "before_model_sha256") ||
		    !baselineModel.prepare(workspaceRestoreBefore, workspaceRestoreBefore) || baselineModel.normalizedXml() != workspaceRestoreBeforeModel) {
			failWorkspace(tr("The saved restore or workspace choice needs review. All records were kept.")); return;
		}
		if (workspaceRestoreUsesBinding && !prepareWorkspaceRestoreBinding(true)) { failWorkspace(tr("The saved restore binding needs review. All recovery records were kept.")); return; }
		syncLocalSha256 = str(workspaceRestorePlan, "before_sha256");
		const auto uploadPath = path(workspaceRestoreDirectory, "upload.json"); const QFileInfo uploadInfo(uploadPath);
		if (uploadInfo.exists() || uploadInfo.isSymLink()) {
			if (!loadWorkspaceRestoreProjection()) { failWorkspace(tr("The saved restore projection needs review. Its records were kept.")); return; }
			CloudWorkspaceReadIntent sourceRead(path(workspaceRestoreDirectory, "source-read.json"), workspaceReference, str(workspaceRestorePlan, "source"), str(workspaceRestorePlan, "source_reservation"));
			if (!sourceRead.inspect()) { failWorkspace(tr("The saved source proof needs review. Its records were kept.")); return; }
			const QJsonObject proof = {{"source", str(workspaceRestorePlan, "source")}, {"reservation", str(workspaceRestorePlan, "source_reservation")}, {"snapshot", sourceRead.snapshot()}};
			CloudWorkspaceUploadIntent upload(uploadPath, workspaceReference, CloudSyncManifest::sha256(workspaceRestoreSourceBytes), workspaceRestoreSourceBytes.size(),
				str(workspaceRestorePlan, "ticket"), str(workspaceRestorePlan, "parent"), proof);
			if (!upload.inspect()) { failWorkspace(tr("The saved upload proof needs review. Its records were kept.")); return; }
			if (upload.state() == "submitted") { failWorkspace(tr("The upload identity was not confirmed. The attempt was kept; review versions to start a separate restore.")); return; }
			if (upload.state() == "committed") { beginWorkspaceRestoreApply(); return; }
		}
		if (workspaceRestoreSnapshot() != workspaceRestoreBefore || (root.head() != str(workspaceRestorePlan, "parent") && root.head() != str(workspaceRestorePlan, "ticket"))) {
			failWorkspace(tr("The local log or workspace changed. The saved restore and your edits were kept.")); return;
		}
	}
	workspaceRestoreStep = WorkspaceRestoreStep::Source;
	const auto serial = workspaceSerial;
	workspaceRestoreReader.start(CloudWorkspaceRead::Action::Read, workspaceToken, workspaceReference, str(workspaceRestorePlan, "source"), str(workspaceRestorePlan, "source_reservation"),
		path(workspacePath, "selection.json"), workspaceChoiceVersion, path(workspacePath, "roots/" + workspaceReference.key() + ".json"),
		path(workspaceRestoreDirectory, "source-read.json"), [this, serial] { return workspaceSerial == serial && workspaceScopeCurrent() && workspacePersistenceAvailable && workspacePersistenceAvailable(); });
}

bool CloudSyncManager::prepareWorkspaceRestoreProjection(const QByteArray &payload)
{
	CloudWorkspaceReadIntent read(path(workspaceRestoreDirectory, "source-read.json"), workspaceReference, str(workspaceRestorePlan, "source"), str(workspaceRestorePlan, "source_reservation"));
	if (!read.reload() || read.state() != "verified" || str(read.snapshot(), "sha256") != CloudSyncManifest::sha256(payload) || read.snapshot().value("bytes") != QJsonValue(double(payload.size()))) return false;
	CloudWorkspaceModel model; if (!model.prepare(payload, workspaceRestoreBefore)) return false;
	const auto normalized = model.normalizedXml(); if (normalized.isEmpty() || normalized.size() > XmlLimit) return false;
	const QJsonObject proof = {{"schema", 1}, {"format", "neo-workspace-model-projection-development-1"}, {"plan_sha256", CloudSyncManifest::sha256(json(workspaceRestorePlan))},
		{"source_snapshot", read.snapshot()}, {"model_sha256", CloudSyncManifest::sha256(normalized)}, {"model_bytes", double(normalized.size())}};
	return keepFile(path(workspaceRestoreDirectory, "source.xml"), payload, XmlLimit) && keepFile(path(workspaceRestoreDirectory, "model.xml"), normalized, XmlLimit) &&
		keepFile(path(workspaceRestoreDirectory, "projection.json"), json(proof), 16384) && loadWorkspaceRestoreProjection();
}

bool CloudSyncManager::loadWorkspaceRestoreProjection()
{
	const auto proof = readObject(path(workspaceRestoreDirectory, "projection.json"));
	const auto raw = readFile(path(workspaceRestoreDirectory, "source.xml"), XmlLimit), normalized = readFile(path(workspaceRestoreDirectory, "model.xml"), XmlLimit);
	CloudWorkspaceReadIntent source(path(workspaceRestoreDirectory, "source-read.json"), workspaceReference, str(workspaceRestorePlan, "source"), str(workspaceRestorePlan, "source_reservation"));
	if (proof.size() != 6 || proof.value("schema") != QJsonValue(1) || str(proof, "format") != "neo-workspace-model-projection-development-1" ||
	    str(proof, "plan_sha256") != CloudSyncManifest::sha256(json(workspaceRestorePlan)) || !source.inspect() || proof.value("source_snapshot").toObject() != source.snapshot() ||
	    str(source.snapshot(), "sha256") != CloudSyncManifest::sha256(raw) || source.snapshot().value("bytes") != QJsonValue(double(raw.size())) ||
	    str(proof, "model_sha256") != CloudSyncManifest::sha256(normalized) || proof.value("model_bytes") != QJsonValue(double(normalized.size()))) return false;
	CloudWorkspaceModel model; if (!model.prepare(raw, workspaceRestoreBefore) || model.normalizedXml() != normalized) return false;
	workspaceRestoreSourceBytes = raw; workspaceRestoreModelBytes = normalized; return true;
}

void CloudSyncManager::workspaceRestoreReadFinished(const QJsonObject &, const QString &, const QString &commit, const QString &reservation, const QByteArray &payload)
{
	if (!workspaceInProgress() || workspaceRestoreStep != WorkspaceRestoreStep::Source || commit != str(workspaceRestorePlan, "source") || reservation != str(workspaceRestorePlan, "source_reservation")) return;
	if (!workspaceScopeCurrent() || !workspacePersistenceAvailable || !workspacePersistenceAvailable() || !prepareWorkspaceRestoreProjection(payload)) {
		failWorkspace(tr("This version could not be prepared safely. Your local log and recovery records were kept.")); return;
	}
	workspaceRestoreStep = WorkspaceRestoreStep::Publishing; const auto serial = workspaceSerial;
	workspaceUpload.startRestore(workspaceToken, workspaceReference, workspaceRestoreSourceBytes, path(workspacePath, "selection.json"), workspaceChoiceVersion,
		path(workspacePath, "roots/" + workspaceReference.key() + ".json"), path(workspaceRestoreDirectory, "upload.json"), str(workspaceRestorePlan, "ticket"), str(workspaceRestorePlan, "parent"),
		path(workspaceRestoreDirectory, "source-read.json"), commit, reservation,
		[this, serial] { return workspaceSerial == serial && workspaceScopeCurrent() && workspacePersistenceAvailable && workspacePersistenceAvailable(); });
}

void CloudSyncManager::workspaceRestorePublished(const QJsonObject &, const QString &, const QString &ticket, bool headMatches)
{
	if (!workspaceInProgress() || workspaceRestoreStep != WorkspaceRestoreStep::Publishing || ticket != str(workspaceRestorePlan, "ticket")) return;
	if (!headMatches || !workspaceScopeCurrent() || !loadWorkspaceRestoreProjection()) { failWorkspace(tr("A later workspace version appeared. The restored cloud copy and your local log were kept.")); return; }
	beginWorkspaceRestoreApply();
}

QByteArray CloudSyncManager::workspaceRestoreSnapshot() const
{
	const auto actual = CloudWorkspaceModel::currentXml();
	if (workspaceRestoreProjected && actual == workspaceRestoreModelBytes) return workspaceRestoreSourceBytes;
	return !workspaceRestoreBeforeModel.isEmpty() && actual == workspaceRestoreBeforeModel ? workspaceRestoreBefore : actual;
}

void CloudSyncManager::beginWorkspaceRestoreApply()
{
	if (!workspacePersistenceAvailable || !workspacePersistenceAvailable()) { failWorkspace(tr("The local destination changed. The restore records were kept.")); return; }
	const auto localDir = path(workspaceRestoreDirectory, "local"), receiptPath = path(localDir, "intent.json");
	workspaceRestoreProjected = false;
	if (QFileInfo::exists(receiptPath)) {
		CloudWorkspaceApplyIntent receipt(localDir, workspaceReference, workspaceChoiceVersion, str(workspaceRestorePlan, "ticket"), str(workspaceRestorePlan, "apply_reservation"));
		if (!receipt.inspect() || (receipt.state() != "prepared" && (receipt.before() != workspaceRestoreBefore || receipt.after() != workspaceRestoreSourceBytes))) {
			failWorkspace(tr("The local restore receipt needs review. All recovery copies were kept.")); return;
		}
		workspaceRestoreProjected = receipt.state() == "applying" || receipt.state() == "applied" || receipt.state() == "persisted";
	}
	workspaceRestoreStep = WorkspaceRestoreStep::Applying; const auto serial = workspaceSerial;
	workspaceRestoreApply.start(workspaceToken, workspaceReference, str(workspaceRestorePlan, "ticket"), str(workspaceRestorePlan, "apply_reservation"),
		path(workspacePath, "selection.json"), workspaceChoiceVersion, path(workspacePath, "roots/" + workspaceReference.key() + ".json"),
		path(workspaceRestoreDirectory, "source-read.json"), path(workspaceRestoreDirectory, "upload.json"), path(workspaceRestoreDirectory, "apply-read.json"), localDir, workspaceRestoreBefore,
		[this, serial] { return workspaceSerial == serial && workspaceScopeCurrent() && workspacePersistenceAvailable && workspacePersistenceAvailable(); },
		[this] { return workspaceRestoreSnapshot(); }, [this](const QByteArray &payload) {
			if (payload != workspaceRestoreSourceBytes || !workspaceScopeCurrent() || !workspacePersistenceAvailable || !workspacePersistenceAvailable()) return false;
			const auto actual = CloudWorkspaceModel::currentXml();
			if (actual != workspaceRestoreBefore && actual != workspaceRestoreBeforeModel) return false;
			CloudWorkspaceModel staged; if (!staged.prepare(payload, actual) || staged.normalizedXml() != workspaceRestoreModelBytes || !staged.install()) return false;
			workspaceRestoreProjected = true; return CloudWorkspaceModel::currentXml() == workspaceRestoreModelBytes;
		});
}

void CloudSyncManager::workspaceRestoreApplyFailed(const QString &message, bool rejected)
{
	// A prepared or ready receipt with both verified recovery copies can be retried
	// without creating another cloud version. The coordinator re-verifies the
	// exact account, choice, head and reservations on each attempt. Do not retry
	// an ambiguous, applied, corrupt or locally edited state.
	if (!rejected && workspaceInProgress() && workspaceRestoreStep == WorkspaceRestoreStep::Applying &&
	    workspaceRestoreApplyRetries < 2 && workspaceScopeCurrent() && workspacePersistenceAvailable && workspacePersistenceAvailable() &&
	    workspaceRestoreSnapshot() == workspaceRestoreBefore) {
		const auto localDir = path(workspaceRestoreDirectory, "local");
		CloudWorkspaceApplyIntent receipt(localDir, workspaceReference, workspaceChoiceVersion, str(workspaceRestorePlan, "ticket"), str(workspaceRestorePlan, "apply_reservation"));
		if (receipt.inspect() && (receipt.state() == "prepared" || receipt.state() == "ready") && receipt.before() == workspaceRestoreBefore && receipt.after() == workspaceRestoreSourceBytes) {
			const auto serial = workspaceSerial;
			const int delay = 250 * ++workspaceRestoreApplyRetries;
			QTimer::singleShot(delay, this, [this, serial, message] {
				if (workspaceSerial != serial || !workspaceInProgress() || workspaceRestoreStep != WorkspaceRestoreStep::Applying) return;
				if (!workspaceScopeCurrent() || !workspacePersistenceAvailable || !workspacePersistenceAvailable() || workspaceRestoreSnapshot() != workspaceRestoreBefore) {
					failWorkspace(message); return;
				}
				beginWorkspaceRestoreApply();
			});
			return;
		}
	}
	failWorkspace(message, rejected);
}

void CloudSyncManager::workspaceRestoreApplied(const QString &directory)
{
	if (!workspaceInProgress() || workspaceRestoreStep != WorkspaceRestoreStep::Applying || directory != path(workspaceRestoreDirectory, "local")) return;
	const auto current = CloudWorkspaceModel::currentXml();
	if (!workspaceScopeCurrent() || !workspacePersistenceAvailable || !workspacePersistenceAvailable() || !workspacePersist ||
	    (current != workspaceRestoreModelBytes && current != workspaceRestoreSourceBytes) || !workspacePersist() || CloudWorkspaceModel::currentXml() != current) {
		failWorkspace(tr("The restored log could not be saved locally. Recovery copies and cloud versions were kept.")); return;
	}
	workspaceRestoreApply.confirmPersisted();
}

void CloudSyncManager::workspaceRestoreCompleted(const QJsonObject &, const QString &, const QString &commit, bool headMatches, bool localMatches)
{
	if (!workspaceInProgress() || workspaceRestoreStep != WorkspaceRestoreStep::Applying || commit != str(workspaceRestorePlan, "ticket")) return;
	workspaceRestoreHeadMatches = headMatches; workspaceRestoreLocalMatches = localMatches;
	workspaceRestoreStep = WorkspaceRestoreStep::ReleaseApply; const auto serial = workspaceSerial;
	workspaceRestoreReader.start(CloudWorkspaceRead::Action::Release, workspaceToken, workspaceReference, commit, str(workspaceRestorePlan, "apply_reservation"),
		path(workspacePath, "selection.json"), workspaceChoiceVersion, path(workspacePath, "roots/" + workspaceReference.key() + ".json"), path(workspaceRestoreDirectory, "apply-read.json"),
		[this, serial] { return workspaceSerial == serial && workspaceScopeCurrent(); });
}

void CloudSyncManager::workspaceRestoreReleased(const QJsonObject &value, const QString &, const QString &reservation)
{
	if (!workspaceInProgress()) return;
	const auto serial = workspaceSerial;
	if (workspaceRestoreStep == WorkspaceRestoreStep::ReleaseApply && reservation == str(workspaceRestorePlan, "apply_reservation")) {
		workspaceRestoreStep = WorkspaceRestoreStep::ReleaseSource;
		workspaceRestoreReader.start(CloudWorkspaceRead::Action::Release, workspaceToken, workspaceReference, str(workspaceRestorePlan, "source"), str(workspaceRestorePlan, "source_reservation"),
			path(workspacePath, "selection.json"), workspaceChoiceVersion, path(workspacePath, "roots/" + workspaceReference.key() + ".json"), path(workspaceRestoreDirectory, "source-read.json"),
			[this, serial] { return workspaceSerial == serial && workspaceScopeCurrent(); }); return;
	}
	if (workspaceRestoreStep != WorkspaceRestoreStep::ReleaseSource || reservation != str(workspaceRestorePlan, "source_reservation")) return;
	CloudWorkspaceRecord root; root.value = value; if (!root.valid() || root.reference().value != workspaceReference.value || !workspaceScopeCurrent()) { failWorkspace(tr("Restore cleanup needs review. All records were kept.")); return; }
	if (workspaceRestoreUsesBinding) {
		if (!loadWorkspaceRestoreProjection() || CloudWorkspaceModel::currentXml() != workspaceRestoreModelBytes) { failWorkspace(tr("The restored local log changed. All recovery records were kept.")); return; }
		workspaceRestoreStep = WorkspaceRestoreStep::PostBindingReview;
		const auto serial = workspaceSerial;
		workspaceSyncReview.start(workspaceToken, workspaceReference, path(workspacePath, "selection.json"), workspaceChoiceVersion,
			path(workspacePath, "roots/" + workspaceReference.key() + ".json"), path(workspacePath, "bindings/" + workspaceReference.key() + ".json"),
			workspaceRestoreModelBytes, [this, serial] { return workspaceSerial == serial && workspaceScopeCurrent(); }, [] { return CloudWorkspaceModel::currentXml(); });
		return;
	}
	finishWorkspaceRestoreRelease(value);
}

void CloudSyncManager::workspaceRestoreBindingReviewed(int result, const QJsonObject &value, const QString &version, const QString &revision)
{
	using Action = CloudWorkspaceSyncDecision::Action;
	if (!workspaceInProgress() || !workspaceRestoreUsesBinding || !workspaceScopeCurrent()) { failWorkspace(tr("The workspace changed during restore. All records were kept.")); return; }
	CloudWorkspaceRecord root; root.value = value; CloudWorkspaceBinding binding(path(workspacePath, "bindings/" + workspaceReference.key() + ".json"), workspaceReference, workspaceChoiceVersion);
	const auto action = Action(result);
	if (!root.valid() || root.reference().value != workspaceReference.value || !binding.load() || binding.revision() != revision ||
	    action == Action::Invalid || action == Action::LegacyChanged) { failWorkspace(tr("The workspace or earlier cloud branch changed. All restore records were kept.")); return; }
	if (workspaceRestoreStep == WorkspaceRestoreStep::PreflightBinding) {
		if (workspaceAction == "version-restore" && (action != Action::InSync || root.head() != binding.commit() || !binding.matchesModel(CloudWorkspaceModel::currentXml()))) {
			failWorkspace(tr("Sync the current local log before restoring a workspace version.")); return;
		}
		workspaceRestoreStep = WorkspaceRestoreStep::None; finishWorkspaceVersions(value, version); return;
	}
	if (workspaceRestoreStep != WorkspaceRestoreStep::PostBindingReview || workspaceRestoreBindingIntent.isEmpty() ||
	    !loadWorkspaceRestoreProjection() || CloudWorkspaceModel::currentXml() != workspaceRestoreModelBytes) {
		failWorkspace(tr("The saved restore binding needs review. All recovery records were kept.")); return;
	}
	CloudWorkspaceAuthority authority(path(workspacePath, "roots/" + workspaceReference.key() + ".json"), workspaceReference);
	CloudWorkspaceApplyIntent receipt(path(workspaceRestoreDirectory, "local"), workspaceReference, workspaceChoiceVersion,
		str(workspaceRestorePlan, "ticket"), str(workspaceRestorePlan, "apply_reservation"));
	CloudWorkspaceReadIntent source(path(workspaceRestoreDirectory, "source-read.json"), workspaceReference,
		str(workspaceRestorePlan, "source"), str(workspaceRestorePlan, "source_reservation"));
	CloudWorkspaceReadIntent application(path(workspaceRestoreDirectory, "apply-read.json"), workspaceReference,
		str(workspaceRestorePlan, "ticket"), str(workspaceRestorePlan, "apply_reservation"));
	const auto prior = workspaceRestoreBindingIntent.value("prior").toObject(), projection = readObject(path(workspaceRestoreDirectory, "projection.json"));
	const auto intentBytes = readFile(path(workspaceRestoreDirectory, "binding-intent.json"), 16384);
	if (intentBytes != json(workspaceRestoreBindingIntent)) { failWorkspace(tr("The restore binding record changed. All recovery records were kept.")); return; }
	const auto currentBinding = binding.value();
	const QJsonObject previous{{"revision", prior.value("revision")}, {"commit", prior.value("commit")}, {"raw", prior.value("raw")}, {"model", prior.value("model")}};
	const bool samePrior = currentBinding == prior;
	const bool sameReplay = binding.revision() == str(workspaceRestoreBindingIntent, "next_revision") && currentBinding.value("previous").toObject() == previous &&
		currentBinding.value("legacy_acceptance") == prior.value("legacy_acceptance");
	if (!samePrior && !sameReplay) { failWorkspace(tr("The workspace binding changed during restore. All recovery records were kept.")); return; }
	if (!authority.load() || !binding.advanceRestored(str(prior, "revision"), root, authority.record(), receipt, source, application,
	    path(workspaceRestoreDirectory, "upload.json"), workspaceRestoreBefore, workspaceRestoreModelBytes, projection,
	    CloudSyncManifest::sha256(json(workspaceRestorePlan)), str(workspaceRestoreBindingIntent, "next_revision"))) {
		failWorkspace(tr("The restored version could not be bound safely. All recovery records were kept.")); return;
	}
	const QJsonObject completed{{"schema", 1}, {"format", "neo-workspace-restore-binding-complete-development-1"},
		{"ticket", str(workspaceRestorePlan, "ticket")}, {"intent_sha256", CloudSyncManifest::sha256(intentBytes)}};
	if (readFile(path(workspaceRestoreDirectory, "binding-intent.json"), 16384) != intentBytes ||
	    !keepFile(path(workspaceRestoreDirectory, "binding-complete.json"), json(completed), 16384)) {
		failWorkspace(tr("The restore acknowledgement needs recovery. All records were kept.")); return;
	}
	finishWorkspaceRestoreRelease(value);
}

void CloudSyncManager::finishWorkspaceRestoreRelease(const QJsonObject &value)
{
	CloudWorkspaceRecord root; root.value = value;
	if (!workspaceInProgress() || !root.valid() || root.reference().value != workspaceReference.value || !workspaceScopeCurrent()) { failWorkspace(tr("Restore cleanup needs review. All records were kept.")); return; }
	QVariantList rows; const auto history = root.value.value("journal").toObject().value("history").toArray();
	for (int i = int(history.size()) - 1; i >= 0; --i) { const auto ticket = history[i].toString(); const auto t = root.ticket(ticket);
		rows.append(QVariantMap{{"id", ticket}, {"number", i + 1}, {"head", ticket == root.head()}, {"bytes", t.value("bytes").toVariant()}, {"restored", !str(t, "restore_source").isEmpty()}}); }
	workspaceReports[syncProviderId] = {{"verified", true}, {"choiceVersion", workspaceChoiceVersion}, {"versions", history.size()}, {"empty", root.head().isEmpty()},
		{"versionRows", rows}, {"restoredVersion", true}, {"restoreHeadMatches", workspaceRestoreHeadMatches && root.head() == str(workspaceRestorePlan, "ticket")},
		{"restoreLocalMatches", workspaceRestoreLocalMatches && workspaceRestoreSnapshot() == workspaceRestoreSourceBytes}};
	const auto providerId = syncProviderId; clearSyncOperation(); emit providersChanged(); emit workspaceFinished(providerId);
}
