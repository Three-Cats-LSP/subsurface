// SPDX-License-Identifier: GPL-2.0
#include "cloudsyncworkspaceactivationplan.h"
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
QByteArray readXml(const QString &path, const QJsonObject &p)
{
	const QFileInfo info(path); if (!info.isFile() || info.isSymLink() || info.size() > Limit) return {};
	QFile file(path); if (!file.open(QIODevice::ReadOnly)) return {};
	const auto bytes = file.read(Limit + 1);
	return file.error() == QFileDevice::NoError && validDigest(p) && p == digest(bytes) && CloudHistoryRecord::validXml(bytes) ? bytes : QByteArray();
}
}
CloudWorkspaceActivationPlan::CloudWorkspaceActivationPlan(const QString &path, const CloudWorkspaceReference &ref,
	const QString &choice, const QString &revision, const QString &target, const QString &reservation, const QString &operation, const QString &acceptance) :
	directory(path), reference(ref)
{
	if (ref.valid() && CloudHistoryRecord::validEpoch(choice) && CloudHistoryRecord::validEpoch(revision) && CloudHistoryRecord::validEpoch(target) &&
	    CloudHistoryRecord::validEpoch(reservation) && CloudHistoryRecord::validEpoch(operation) && CloudHistoryRecord::validEpoch(acceptance) &&
	    target != reservation && operation != target && operation != reservation)
		scope = {{"schema", 1}, {"format", "neo-workspace-activation-plan-development-1"}, {"workspace", ref.value}, {"choice_version", choice},
			{"binding_revision", revision}, {"target", target}, {"reservation", reservation}, {"operation", operation}, {"acceptance", acceptance}};
}
QString CloudWorkspaceActivationPlan::state() const { return text(saved, "state"); }
QByteArray CloudWorkspaceActivationPlan::model() const { return saved.isEmpty() ? QByteArray() : readXml(QDir(directory).filePath("model.xml"), saved.value("model").toObject()); }
bool CloudWorkspaceActivationPlan::read()
{
	saved = {}; if (scope.isEmpty() || directory.isEmpty() || QFileInfo(directory).isSymLink()) return false;
	const QFileInfo info(QDir(directory).filePath("plan.json")); if (!info.exists()) return !info.isSymLink();
	if (!info.isFile() || info.isSymLink() || info.size() < 1 || info.size() > 16384) return false;
	QFile file(info.filePath()); if (!file.open(QIODevice::ReadOnly)) return false;
	const auto bytes = file.read(16385); const auto body = QJsonDocument::fromJson(bytes).object(); auto fixed = body;
	fixed.remove("state"); fixed.remove("model");
	if (file.error() != QFileDevice::NoError || bytes.size() > 16384 || fixed != scope ||
	    !QStringList({"prepared", "ready", "requiring", "completed"}).contains(text(body, "state")) || !validDigest(body.value("model").toObject())) return false;
	const QFileInfo modelFile(QDir(directory).filePath("model.xml"));
	if (!(text(body, "state") == "prepared" && !modelFile.exists() && !modelFile.isSymLink()) &&
	    readXml(modelFile.filePath(), body.value("model").toObject()).isEmpty()) return false;
	saved = body; return true;
}
bool CloudWorkspaceActivationPlan::reload()
{
	QLockFile lock(QDir(directory).filePath("plan.json.lock")); lock.setStaleLockTime(0); return !directory.isEmpty() && lock.tryLock(0) && read() && !saved.isEmpty();
}
bool CloudWorkspaceActivationPlan::write(const QJsonObject &body)
{
	const auto bytes = QJsonDocument(body).toJson(QJsonDocument::Compact); QSaveFile file(QDir(directory).filePath("plan.json"));
	return bytes.size() <= 16384 && file.open(QIODevice::WriteOnly) && file.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner) &&
		file.write(bytes) == bytes.size() && file.commit() && read() && saved == body;
}
bool CloudWorkspaceActivationPlan::prepare(const QByteArray &value)
{
	if (scope.isEmpty() || directory.isEmpty() || QFileInfo(directory).isSymLink() || value.isEmpty() || value.size() > Limit ||
	    !CloudHistoryRecord::validXml(value) || !QDir().mkpath(directory)) return false;
	QLockFile lock(QDir(directory).filePath("plan.json.lock")); lock.setStaleLockTime(0); if (!lock.tryLock(0) || !read()) return false;
	auto next = scope; next["state"] = "prepared"; next["model"] = digest(value);
	if (saved.isEmpty()) {
		for (const auto &name : QDir(directory).entryList(QDir::AllEntries | QDir::Hidden | QDir::System | QDir::NoDotAndDotDot))
			if (name != "plan.json.lock") return false;
		if (!write(next)) return false;
	} else { auto previous = saved; previous["state"] = "prepared"; if (previous != next) return false; }
	const auto path = QDir(directory).filePath("model.xml"); const QFileInfo info(path);
	if (info.exists() || info.isSymLink()) { if (readXml(path, saved.value("model").toObject()) != value) return false; }
	else {
		QSaveFile file(path);
		if (!file.open(QIODevice::WriteOnly) || !file.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner) ||
		    file.write(value) != value.size() || !file.commit() || readXml(path, saved.value("model").toObject()) != value) return false;
	}
	if (state() != "prepared") return true;
	next = saved; next["state"] = "ready"; return write(next);
}
bool CloudWorkspaceActivationPlan::beginRequire()
{
	QLockFile lock(QDir(directory).filePath("plan.json.lock")); lock.setStaleLockTime(0);
	if (!lock.tryLock(0) || !read() || saved.isEmpty()) return false;
	if (state() == "requiring" || state() == "completed") return true;
	if (state() != "ready") return false;
	auto next = saved; next["state"] = "requiring"; return write(next);
}
bool CloudWorkspaceActivationPlan::complete(CloudWorkspaceRoute &route, CloudWorkspaceBinding &binding, const CloudWorkspaceRecord &root,
	const CloudWorkspaceRecord &checkpoint, CloudWorkspaceReadIntent &reader)
{
	QLockFile lock(QDir(directory).filePath("plan.json.lock")); lock.setStaleLockTime(0);
	if (!lock.tryLock(0) || !read() || saved.isEmpty() || !route.load() || route.state() != "active" ||
	    !reader.matches(reference, text(scope, "target"), text(scope, "reservation")) || !reader.reload() || reader.state() != "released") return false;
	const auto routeValue = route.value(); const auto bytes = model(); const auto r = root.value.value("journal").toObject().value("reservations").toObject().value(text(scope, "reservation")).toObject();
	if (routeValue.value("workspace") != scope.value("workspace") || routeValue.value("choice_version") != scope.value("choice_version") ||
	    routeValue.value("binding_revision") != scope.value("binding_revision") || routeValue.value("operation") != scope.value("operation") ||
	    text(r, "commit") != text(scope, "target") || r.value("active") != QJsonValue(false) ||
	    !binding.ownsActivation(root, checkpoint, bytes, bytes, text(scope, "binding_revision"), text(scope, "target")) || binding.acceptance() != text(scope, "acceptance") ||
	    reader.snapshot() != binding.value().value("raw").toObject()) return false;
	if (state() == "completed") return true;
	if (state() != "requiring") return false;
	auto next = saved; next["state"] = "completed"; return write(next);
}
