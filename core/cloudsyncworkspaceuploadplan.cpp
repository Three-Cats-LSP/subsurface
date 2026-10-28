// SPDX-License-Identifier: GPL-2.0
#include "cloudsyncworkspaceuploadplan.h"
#include "cloudsyncmanifest.h"
#include "cloudsyncworkspaceread.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QLockFile>
#include <QRegularExpression>
#include <QSaveFile>

namespace {
constexpr qint64 Limit = 64 * 1024 * 1024;
QString text(const QJsonObject &o, const char *key) { return o.value(QLatin1String(key)).toString(); }
bool validDigest(const QJsonObject &p)
{
	const auto size = p.value("bytes").toDouble(-1);
	return p.size() == 2 && QRegularExpression("^[a-f0-9]{64}$").match(text(p, "sha256")).hasMatch() &&
		p.value("bytes").isDouble() && size >= 1 && size <= Limit && size == double(qint64(size));
}
QJsonObject digest(const QByteArray &bytes) { return {{"sha256", CloudSyncManifest::sha256(bytes)}, {"bytes", double(bytes.size())}}; }
QByteArray readXml(const QString &path, const QJsonObject &p)
{
	const QFileInfo info(path); if (!info.isFile() || info.isSymLink() || info.size() > Limit) return {};
	QFile file(path); if (!file.open(QIODevice::ReadOnly)) return {};
	const auto bytes = file.read(Limit + 1);
	return file.error() == QFileDevice::NoError && validDigest(p) && p == digest(bytes) && CloudHistoryRecord::validXml(bytes) ? bytes : QByteArray();
}
}
CloudWorkspaceUploadPlan::CloudWorkspaceUploadPlan(const QString &path, const CloudWorkspaceReference &ref,
	const QString &choice, const QString &revision, const QString &next, const QString &baseline,
	const QString &ticket, const QString &operation, const QString &reviewedHead, const QJsonObject &reviewedBinding) : directory(path)
{
	const bool ordinary = reviewedHead.isEmpty() && reviewedBinding.isEmpty();
	const bool conflict = CloudHistoryRecord::validEpoch(reviewedHead) && reviewedHead != baseline && reviewedHead != ticket &&
		operation != reviewedHead && reviewedBinding.value("workspace") == ref.value &&
		text(reviewedBinding, "choice_version") == choice && text(reviewedBinding, "revision") == revision &&
		text(reviewedBinding, "commit") == baseline && CloudHistoryRecord::validEpoch(text(reviewedBinding, "legacy_acceptance"));
	if (ref.valid() && CloudHistoryRecord::validEpoch(choice) && CloudHistoryRecord::validEpoch(revision) && CloudHistoryRecord::validEpoch(next) &&
	    CloudHistoryRecord::validEpoch(baseline) && CloudHistoryRecord::validEpoch(ticket) && CloudHistoryRecord::validEpoch(operation) &&
	    revision != next && baseline != ticket && operation != baseline && operation != ticket && (ordinary || conflict)) {
		scope = {{"schema", 1}, {"format", ordinary ? "neo-workspace-upload-plan-development-1" : "neo-workspace-conflict-local-plan-development-1"},
			{"workspace", ref.value}, {"choice_version", choice}, {"binding_revision", revision}, {"next_revision", next},
			{"baseline", baseline}, {"ticket", ticket}, {"operation", operation}};
		if (conflict) { scope["reviewed_head"] = reviewedHead; scope["reviewed_binding"] = reviewedBinding; }
	}
}
QString CloudWorkspaceUploadPlan::state() const { return text(saved, "state"); }
QByteArray CloudWorkspaceUploadPlan::payload() const { return saved.isEmpty() ? QByteArray() : readXml(QDir(directory).filePath("payload.xml"), saved.value("payload").toObject()); }
bool CloudWorkspaceUploadPlan::read()
{
	saved = {}; if (scope.isEmpty() || directory.isEmpty() || QFileInfo(directory).isSymLink()) return false;
	const QFileInfo info(QDir(directory).filePath("plan.json")); if (!info.exists()) return !info.isSymLink();
	if (!info.isFile() || info.isSymLink() || info.size() < 1 || info.size() > 16384) return false;
	QFile file(info.filePath()); if (!file.open(QIODevice::ReadOnly)) return false;
	const auto bytes = file.read(16385); const auto body = QJsonDocument::fromJson(bytes).object(); auto fixed = body;
	fixed.remove("state"); fixed.remove("payload");
	if (file.error() != QFileDevice::NoError || bytes.size() > 16384 || fixed != scope ||
	    !QStringList({"prepared", "ready", "completed"}).contains(text(body, "state")) || !validDigest(body.value("payload").toObject())) return false;
	const QFileInfo payloadFile(QDir(directory).filePath("payload.xml"));
	if (!(text(body, "state") == "prepared" && !payloadFile.exists() && !payloadFile.isSymLink()) &&
	    readXml(payloadFile.filePath(), body.value("payload").toObject()).isEmpty()) return false;
	saved = body; return true;
}
bool CloudWorkspaceUploadPlan::reload()
{
	QLockFile lock(QDir(directory).filePath("plan.json.lock")); lock.setStaleLockTime(0); return !directory.isEmpty() && lock.tryLock(0) && read() && !saved.isEmpty();
}
bool CloudWorkspaceUploadPlan::write(const QJsonObject &body)
{
	const auto bytes = QJsonDocument(body).toJson(QJsonDocument::Compact); QSaveFile file(QDir(directory).filePath("plan.json"));
	return bytes.size() <= 16384 && file.open(QIODevice::WriteOnly) && file.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner) &&
		file.write(bytes) == bytes.size() && file.commit() && read() && saved == body;
}
bool CloudWorkspaceUploadPlan::prepare(const QByteArray &model)
{
	if (scope.isEmpty() || directory.isEmpty() || QFileInfo(directory).isSymLink() || model.isEmpty() || model.size() > Limit ||
	    !CloudHistoryRecord::validXml(model) || !QDir().mkpath(directory)) return false;
	QLockFile lock(QDir(directory).filePath("plan.json.lock")); lock.setStaleLockTime(0); if (!lock.tryLock(0) || !read()) return false;
	auto next = scope; next["state"] = "prepared"; next["payload"] = digest(model);
	if (saved.isEmpty()) {
		for (const auto &name : QDir(directory).entryList(QDir::AllEntries | QDir::Hidden | QDir::System | QDir::NoDotAndDotDot))
			if (name != "plan.json.lock") return false;
		if (!write(next)) return false;
	} else { auto previous = saved; previous["state"] = "prepared"; if (previous != next) return false; }
	const auto path = QDir(directory).filePath("payload.xml"); const QFileInfo info(path);
	if (info.exists() || info.isSymLink()) { if (readXml(path, saved.value("payload").toObject()) != model) return false; }
	else {
		QSaveFile file(path);
		if (!file.open(QIODevice::WriteOnly) || !file.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner) ||
		    file.write(model) != model.size() || !file.commit() || readXml(path, saved.value("payload").toObject()) != model) return false;
	}
	if (state() != "prepared") return true;
	next = saved; next["state"] = "ready"; return write(next);
}
bool CloudWorkspaceUploadPlan::complete(CloudWorkspaceBinding &binding, const CloudWorkspaceRecord &root,
	const CloudWorkspaceRecord &checkpoint, const QString &uploadIntentFile)
{
	QLockFile lock(QDir(directory).filePath("plan.json.lock")); lock.setStaleLockTime(0);
	if (!lock.tryLock(0) || !read() || saved.isEmpty()) return false;
	const auto model = payload();
	const bool conflict = text(scope, "format") == "neo-workspace-conflict-local-plan-development-1";
	if (conflict) {
		CloudWorkspaceReference ref; ref.value = scope.value("workspace").toObject();
		const auto head = text(scope, "reviewed_head"), reservation = text(scope, "operation");
		const auto readFile = QDir(QFileInfo(directory).absolutePath()).filePath("reviewed-read.json");
		CloudWorkspaceReadIntent reader(readFile, ref, head, reservation);
		const auto target = root.ticket(head);
		const QJsonObject expected{{"name", target.value("name")}, {"object", target.value("object")},
			{"version", target.value("version")}, {"sha256", target.value("sha256")}, {"bytes", target.value("bytes")}};
		const auto record = root.value.value("journal").toObject().value("reservations").toObject().value(reservation).toObject();
		if (!reader.reload() || reader.state() != "released" || reader.snapshot() != expected ||
		    text(record, "commit") != head || record.value("active") != QJsonValue(false)) return false;
	}
	const bool owned = conflict ? binding.ownsConflictUpload(scope.value("reviewed_binding").toObject(), text(scope, "reviewed_head"),
		root, checkpoint, model, text(scope, "ticket"), uploadIntentFile, text(scope, "next_revision")) :
		binding.ownsUpload(root, checkpoint, text(scope, "binding_revision"), text(scope, "ticket"), uploadIntentFile, model);
	if (model.isEmpty() || !owned ||
	    binding.value().value("workspace") != scope.value("workspace") || binding.value().value("choice_version") != scope.value("choice_version") ||
	    binding.revision() != text(scope, "next_revision") || text(binding.value().value("previous").toObject(), "commit") != text(scope, "baseline")) return false;
	if (state() == "completed") return true;
	if (state() != "ready") return false;
	auto next = saved; next["state"] = "completed"; return write(next);
}
