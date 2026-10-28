// SPDX-License-Identifier: GPL-2.0
#include "cloudsyncworkspacereceiveplan.h"
#include "cloudsyncmanifest.h"
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
bool matches(const QJsonObject &p, const QByteArray &bytes)
{
	return validDigest(p) && p == digest(bytes) && CloudHistoryRecord::validXml(bytes);
}
QByteArray readXml(const QString &path, const QJsonObject &p)
{
	const QFileInfo info(path); if (!info.isFile() || info.isSymLink() || info.size() > Limit) return {};
	QFile file(path); if (!file.open(QIODevice::ReadOnly)) return {};
	const auto bytes = file.read(Limit + 1); return file.error() == QFileDevice::NoError && matches(p, bytes) ? bytes : QByteArray();
}
}
CloudWorkspaceReceivePlan::CloudWorkspaceReceivePlan(const QString &path, const CloudWorkspaceReference &ref,
	const QString &choice, const QString &revision, const QString &next, const QString &baseline,
	const QString &target, const QString &reservation, const QString &operation) : directory(path)
{
	if (ref.valid() && CloudHistoryRecord::validEpoch(choice) && CloudHistoryRecord::validEpoch(revision) &&
	    CloudHistoryRecord::validEpoch(next) && CloudHistoryRecord::validEpoch(baseline) && CloudHistoryRecord::validEpoch(target) &&
	    CloudHistoryRecord::validEpoch(reservation) && CloudHistoryRecord::validEpoch(operation) && revision != next &&
	    baseline != target && reservation != baseline && reservation != target && operation != baseline && operation != target && operation != reservation)
		scope = {{"schema", 1}, {"format", "neo-workspace-receive-plan-development-1"}, {"workspace", ref.value},
			{"choice_version", choice}, {"binding_revision", revision}, {"next_revision", next}, {"baseline", baseline},
			{"target", target}, {"reservation", reservation}, {"operation", operation}};
}
QString CloudWorkspaceReceivePlan::state() const { return text(saved, "state"); }
QByteArray CloudWorkspaceReceivePlan::before() const
{
	return saved.isEmpty() ? QByteArray() : readXml(QDir(directory).filePath("before.xml"), saved.value("before").toObject());
}
bool CloudWorkspaceReceivePlan::read()
{
	saved = {}; if (scope.isEmpty() || directory.isEmpty() || QFileInfo(directory).isSymLink()) return false;
	const QFileInfo info(QDir(directory).filePath("plan.json")); if (!info.exists()) return !info.isSymLink();
	if (!info.isFile() || info.isSymLink() || info.size() < 1 || info.size() > 16384) return false;
	QFile file(info.filePath()); if (!file.open(QIODevice::ReadOnly)) return false;
	const auto bytes = file.read(16385); const auto body = QJsonDocument::fromJson(bytes).object(); auto fixed = body;
	fixed.remove("state"); fixed.remove("before");
	if (file.error() != QFileDevice::NoError || bytes.size() > 16384 || fixed != scope ||
	    !QStringList({"prepared", "ready", "completed"}).contains(text(body, "state")) || !validDigest(body.value("before").toObject())) return false;
	const QFileInfo payload(QDir(directory).filePath("before.xml"));
	if (!(text(body, "state") == "prepared" && !payload.exists() && !payload.isSymLink()) &&
	    readXml(payload.filePath(), body.value("before").toObject()).isEmpty()) return false;
	saved = body; return true;
}
bool CloudWorkspaceReceivePlan::reload()
{
	QLockFile lock(QDir(directory).filePath("plan.json.lock")); lock.setStaleLockTime(0); return !directory.isEmpty() && lock.tryLock(0) && read() && !saved.isEmpty();
}
bool CloudWorkspaceReceivePlan::write(const QJsonObject &body)
{
	const auto bytes = QJsonDocument(body).toJson(QJsonDocument::Compact); QSaveFile file(QDir(directory).filePath("plan.json"));
	return bytes.size() <= 16384 && file.open(QIODevice::WriteOnly) && file.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner) &&
		file.write(bytes) == bytes.size() && file.commit() && read() && saved == body;
}
bool CloudWorkspaceReceivePlan::prepare(const QByteArray &model)
{
	if (scope.isEmpty() || directory.isEmpty() || QFileInfo(directory).isSymLink() || model.isEmpty() || model.size() > Limit ||
	    !CloudHistoryRecord::validXml(model) || !QDir().mkpath(directory)) return false;
	QLockFile lock(QDir(directory).filePath("plan.json.lock")); lock.setStaleLockTime(0); if (!lock.tryLock(0) || !read()) return false;
	auto next = scope; next["state"] = "prepared"; next["before"] = digest(model);
	if (saved.isEmpty()) {
		for (const auto &name : QDir(directory).entryList(QDir::AllEntries | QDir::Hidden | QDir::System | QDir::NoDotAndDotDot))
			if (name != "plan.json.lock") return false;
		if (!write(next)) return false;
	} else { auto previous = saved; previous["state"] = "prepared"; if (previous != next) return false; }
	const auto path = QDir(directory).filePath("before.xml"); const QFileInfo info(path);
	if (info.exists() || info.isSymLink()) { if (readXml(path, saved.value("before").toObject()) != model) return false; }
	else {
		QSaveFile file(path);
		if (!file.open(QIODevice::WriteOnly) || !file.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner) ||
		    file.write(model) != model.size() || !file.commit() || readXml(path, saved.value("before").toObject()) != model) return false;
	}
	if (state() != "prepared") return true;
	next = saved; next["state"] = "ready"; return write(next);
}
bool CloudWorkspaceReceivePlan::complete(CloudWorkspaceBinding &binding, const CloudWorkspaceRecord &root, const CloudWorkspaceRecord &checkpoint,
	CloudWorkspaceReceiveReceipt &receipt, CloudWorkspaceReadIntent &reader)
{
	QLockFile lock(QDir(directory).filePath("plan.json.lock")); lock.setStaleLockTime(0);
	if (!lock.tryLock(0) || !read() || saved.isEmpty() || !receipt.reload() || receipt.state() != "persisted" ||
	    !reader.reload() || reader.state() != "released") return false;
	const auto proof = receipt.proof();
	for (const auto &key : {"workspace", "choice_version", "binding_revision", "next_revision", "baseline", "target", "reservation"})
		if (proof.value(QLatin1String(key)) != scope.value(QLatin1String(key))) return false;
	if (proof.value("before") != saved.value("before") || receipt.before() != before() || !binding.ownsReceived(root, checkpoint, receipt, reader)) return false;
	if (state() == "completed") return true;
	if (state() != "ready") return false;
	auto next = saved; next["state"] = "completed"; return write(next);
}
