// SPDX-License-Identifier: GPL-2.0
#include "cloudsyncworkspaceroute.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QLockFile>
#include <QSaveFile>

namespace {
QString text(const QJsonObject &o, const char *key) { return o.value(QLatin1String(key)).toString(); }
bool fields(const QJsonObject &o, const QStringList &keys)
{
	if (o.size() != keys.size()) return false;
	for (const auto &key : keys) if (!o.contains(key)) return false;
	return true;
}
}
CloudWorkspaceRouteLease::CloudWorkspaceRouteLease(const QString &file) : fileName(file) {}
CloudWorkspaceRouteLease::~CloudWorkspaceRouteLease() = default;
bool CloudWorkspaceRouteLease::held() const { return lock && lock->isLocked(); }
void CloudWorkspaceRouteLease::release() { if (lock) lock->unlock(); lock.reset(); }
bool CloudWorkspaceRouteLease::acquire()
{
	if (held()) return true;
	const QFileInfo info(fileName), parent(info.absolutePath());
	if (fileName.isEmpty() || !info.isAbsolute() || info.isSymLink() || parent.isSymLink() || !QDir().mkpath(parent.filePath())) return false;
	lock = std::make_unique<QLockFile>(fileName); lock->setStaleLockTime(0);
	if (info.exists()) {
		qint64 pid = 0; QString host, application;
		if (!info.isFile() || !lock->getLockInfo(&pid, &host, &application)) { lock.reset(); return false; }
	}
	if (!lock->tryLock(0)) { lock.reset(); return false; }
	return true;
}
CloudWorkspaceRoute::CloudWorkspaceRoute(const QString &file, const QString &id) : fileName(file), provider(id) {}
QString CloudWorkspaceRoute::state() const { return text(saved, "state"); }
bool CloudWorkspaceRoute::read()
{
	saved = {};
	if (fileName.isEmpty() || (provider != "google-drive" && provider != "dropbox")) return false;
	const QFileInfo parent(QFileInfo(fileName).absolutePath()), info(fileName);
	if (parent.isSymLink() || info.isSymLink()) return false;
	if (!info.exists()) return true;
	if (!info.isFile() || info.size() < 1 || info.size() > 8192) return false;
	QFile file(fileName); if (!file.open(QIODevice::ReadOnly)) return false;
	const auto bytes = file.read(8193); const auto body = QJsonDocument::fromJson(bytes).object();
	CloudWorkspaceReference ref; ref.value = body.value("workspace").toObject();
	if (file.error() != QFileDevice::NoError || bytes.size() > 8192 ||
	    !fields(body, {"schema", "format", "provider", "state", "workspace", "choice_version", "binding_revision", "operation"}) ||
	    body.value("schema") != QJsonValue(1) || text(body, "format") != "neo-workspace-route-development-1" ||
	    text(body, "provider") != provider || (text(body, "state") != "preparing" && text(body, "state") != "active") ||
	    !ref.valid() || text(ref.value, "provider") != provider || !CloudHistoryRecord::validEpoch(text(body, "choice_version")) ||
	    !CloudHistoryRecord::validEpoch(text(body, "binding_revision")) || !CloudHistoryRecord::validEpoch(text(body, "operation"))) return false;
	saved = body; return true;
}
bool CloudWorkspaceRoute::load()
{
	if (fileName.isEmpty()) return false;
	const QFileInfo parent(QFileInfo(fileName).absolutePath());
	if (!parent.exists()) return read();
	QLockFile lock(fileName + ".lock"); return lock.tryLock(0) && read();
}
bool CloudWorkspaceRoute::legacyAllowed(bool markerRequired) { return load() && saved.isEmpty() && !markerRequired; }
bool CloudWorkspaceRoute::write(const QJsonObject &body)
{
	const auto bytes = QJsonDocument(body).toJson(QJsonDocument::Compact); QSaveFile file(fileName);
	return bytes.size() <= 8192 && file.open(QIODevice::WriteOnly) && file.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner) &&
		file.write(bytes) == bytes.size() && file.commit() && read() && saved == body;
}
bool CloudWorkspaceRoute::prepare(const CloudWorkspaceReference &ref, const QString &choice, const QString &revision, const QString &operation)
{
	const QFileInfo parent(QFileInfo(fileName).absolutePath());
	if (fileName.isEmpty() || parent.isSymLink() || !ref.valid() || text(ref.value, "provider") != provider ||
	    !CloudHistoryRecord::validEpoch(choice) || !CloudHistoryRecord::validEpoch(revision) || !CloudHistoryRecord::validEpoch(operation) ||
	    !QDir().mkpath(parent.filePath())) return false;
	QLockFile lock(fileName + ".lock"); if (!lock.tryLock(0) || !read()) return false;
	QJsonObject body{{"schema", 1}, {"format", "neo-workspace-route-development-1"}, {"provider", provider}, {"state", "preparing"},
		{"workspace", ref.value}, {"choice_version", choice}, {"binding_revision", revision}, {"operation", operation}};
	if (!saved.isEmpty()) { body["state"] = state(); return saved == body; }
	return write(body);
}
bool CloudWorkspaceRoute::activate(CloudWorkspaceBinding &binding)
{
	if (fileName.isEmpty()) return false;
	QLockFile lock(fileName + ".lock"); if (!lock.tryLock(0) || !read() || saved.isEmpty()) return false;
	if (!binding.load()) return false;
	const auto b = binding.value();
	if (b.isEmpty() || b.value("workspace") != saved.value("workspace") || b.value("choice_version") != saved.value("choice_version") ||
	    b.value("revision") != saved.value("binding_revision") || binding.commit().isEmpty() || binding.acceptance().isEmpty()) return false;
	if (state() == "active") return true;
	auto body = saved; body["state"] = "active"; return write(body);
}
