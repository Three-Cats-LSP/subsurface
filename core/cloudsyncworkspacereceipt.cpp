// SPDX-License-Identifier: GPL-2.0
#include "cloudsyncworkspacereceipt.h"
#include "cloudsyncmanifest.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLockFile>
#include <QRegularExpression>
#include <QSaveFile>

namespace {
constexpr qint64 Limit = 64 * 1024 * 1024;
QString text(const QJsonObject &o, const char *key) { return o.value(QLatin1String(key)).toString(); }
bool fields(const QJsonObject &o, const QStringList &keys)
{
	if (o.size() != keys.size()) return false;
	for (const auto &key : keys) if (!o.contains(key)) return false;
	return true;
}
QJsonObject digest(const QByteArray &bytes)
{
	return {{"sha256", CloudSyncManifest::sha256(bytes)}, {"bytes", double(bytes.size())}};
}
bool validDigest(const QJsonObject &d)
{
	const auto size = d.value("bytes").toDouble(-1);
	return fields(d, {"sha256", "bytes"}) && QRegularExpression("^[a-f0-9]{64}$").match(text(d, "sha256")).hasMatch() &&
		d.value("bytes").isDouble() && size >= 1 && size <= Limit && size == double(qint64(size));
}
bool accepts(const QJsonObject &d, const QByteArray &bytes)
{
	return validDigest(d) && bytes.size() == d.value("bytes").toDouble() &&
		CloudSyncManifest::sha256(bytes) == text(d, "sha256") && CloudHistoryRecord::validXml(bytes);
}
QJsonObject pin(const QJsonObject &ticket)
{
	return {{"name", ticket.value("name")}, {"object", ticket.value("object")}, {"version", ticket.value("version")},
		{"sha256", ticket.value("sha256")}, {"bytes", ticket.value("bytes")}};
}
bool validPin(const CloudWorkspaceReference &ref, const QString &commit, const QJsonObject &p)
{
	const auto version = text(p, "version");
	return fields(p, {"name", "object", "version", "sha256", "bytes"}) &&
		text(p, "name") == ref.payloadName(commit) && text(p, "object") != text(ref.value, "root_id") &&
		CloudWorkspaceReference::create(text(ref.value, "provider"), text(ref.value, "account"), text(ref.value, "epoch"), text(p, "object")).valid() &&
		!version.isEmpty() && version.size() <= 512 && !version.contains(QChar::Null) && !version.contains('\n') && !version.contains('\r') &&
		validDigest({{"sha256", p.value("sha256")}, {"bytes", p.value("bytes")}});
}
QByteArray loadXml(const QString &path, const QJsonObject &d)
{
	const QFileInfo info(path); if (!info.isFile() || info.isSymLink() || info.size() > Limit) return {};
	QFile file(path); if (!file.open(QIODevice::ReadOnly)) return {};
	const auto bytes = file.read(Limit + 1); return file.error() == QFileDevice::NoError && accepts(d, bytes) ? bytes : QByteArray();
}
bool saveXml(const QString &path, const QJsonObject &d, const QByteArray &bytes)
{
	const QFileInfo info(path); if (info.exists() || info.isSymLink()) return loadXml(path, d) == bytes;
	QSaveFile file(path);
	return accepts(d, bytes) && file.open(QIODevice::WriteOnly) &&
		file.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner) &&
		file.write(bytes) == bytes.size() && file.commit() && loadXml(path, d) == bytes;
}
}

CloudWorkspaceReceiveReceipt::CloudWorkspaceReceiveReceipt(const QString &path, const CloudWorkspaceReference &ref,
	const QString &choice, const QString &revision, const QString &baseline, const QString &target, const QString &reservation, const QString &nextRevision) :
	directory(path), reference(ref)
{
	if (ref.valid() && CloudHistoryRecord::validEpoch(choice) && CloudHistoryRecord::validEpoch(revision) &&
	    CloudHistoryRecord::validEpoch(baseline) && CloudHistoryRecord::validEpoch(target) &&
	    CloudHistoryRecord::validEpoch(reservation) && CloudHistoryRecord::validEpoch(nextRevision) && revision != nextRevision &&
	    baseline != target && reservation != baseline && reservation != target)
		scope = {{"schema", 1}, {"format", "neo-workspace-receive-development-1"}, {"workspace", ref.value},
			{"choice_version", choice}, {"binding_revision", revision}, {"next_revision", nextRevision},
			{"baseline", baseline}, {"target", target}, {"reservation", reservation}};
}
QString CloudWorkspaceReceiveReceipt::state() const { return text(saved, "state"); }
QByteArray CloudWorkspaceReceiveReceipt::before() const { return saved.isEmpty() ? QByteArray() : loadXml(QDir(directory).filePath("before.xml"), saved.value("before").toObject()); }
QByteArray CloudWorkspaceReceiveReceipt::raw() const { return saved.isEmpty() ? QByteArray() : loadXml(QDir(directory).filePath("raw.xml"), saved.value("raw").toObject()); }
QByteArray CloudWorkspaceReceiveReceipt::after() const { return saved.isEmpty() ? QByteArray() : loadXml(QDir(directory).filePath("after.xml"), saved.value("after").toObject()); }
bool CloudWorkspaceReceiveReceipt::valid(const QJsonObject &body) const
{
	auto fixed = body;
	for (const auto &key : {"state", "acceptance", "baseline_pin", "target_pin", "before", "raw", "after"}) fixed.remove(QLatin1String(key));
	const auto targetPin = body.value("target_pin").toObject(), rawDigest = body.value("raw").toObject();
	return !scope.isEmpty() && fixed == scope && QStringList({"prepared", "ready", "applying", "applied", "persisted"}).contains(text(body, "state")) &&
		CloudHistoryRecord::validEpoch(text(body, "acceptance")) &&
		validPin(reference, text(scope, "baseline"), body.value("baseline_pin").toObject()) &&
		validPin(reference, text(scope, "target"), targetPin) &&
		body.value("baseline_pin").toObject().value("object") != targetPin.value("object") &&
		validDigest(body.value("before").toObject()) && validDigest(rawDigest) && validDigest(body.value("after").toObject()) &&
		rawDigest.value("sha256") == targetPin.value("sha256") && rawDigest.value("bytes") == targetPin.value("bytes");
}
bool CloudWorkspaceReceiveReceipt::read()
{
	saved = {}; if (scope.isEmpty() || directory.isEmpty() || QFileInfo(directory).isSymLink()) return false;
	const QFileInfo info(QDir(directory).filePath("intent.json")); if (!info.exists()) return !info.isSymLink();
	if (!info.isFile() || info.isSymLink() || info.size() > 16384) return false;
	QFile file(info.filePath()); if (!file.open(QIODevice::ReadOnly)) return false;
	const auto bytes = file.read(16385); const auto body = QJsonDocument::fromJson(bytes).object();
	if (file.error() != QFileDevice::NoError || bytes.size() > 16384 || !valid(body)) return false;
	for (const auto &name : {"before", "raw", "after"}) {
		const QFileInfo payload(QDir(directory).filePath(QLatin1String(name) + ".xml"));
		if (text(body, "state") == "prepared" && !payload.exists() && !payload.isSymLink()) continue;
		if (loadXml(payload.filePath(), body.value(QLatin1String(name)).toObject()).isEmpty()) return false;
	}
	saved = body; return true;
}
bool CloudWorkspaceReceiveReceipt::reload()
{
	QLockFile lock(QDir(directory).filePath("intent.json.lock")); lock.setStaleLockTime(0);
	return !directory.isEmpty() && lock.tryLock(0) && read() && !saved.isEmpty();
}
bool CloudWorkspaceReceiveReceipt::write(const QJsonObject &body)
{
	if (!valid(body)) return false;
	const auto bytes = QJsonDocument(body).toJson(QJsonDocument::Compact); QSaveFile file(QDir(directory).filePath("intent.json"));
	return bytes.size() <= 16384 && file.open(QIODevice::WriteOnly) && file.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner) &&
		file.write(bytes) == bytes.size() && file.commit() && read() && saved == body;
}
bool CloudWorkspaceReceiveReceipt::prepare(const CloudWorkspaceBinding &binding, const CloudWorkspaceRecord &root,
	const CloudWorkspaceRecord &checkpoint, CloudWorkspaceReadIntent &readIntent,
	const QByteArray &beforeModel, const QByteArray &targetRaw, const QByteArray &afterModel)
{
	if (scope.isEmpty() || directory.isEmpty() || QFileInfo(directory).isSymLink() || !root.valid() || !checkpoint.valid() ||
	    root.reference().value != reference.value || checkpoint.reference().value != reference.value || !root.follows(checkpoint) ||
	    root.head() != text(scope, "target") || binding.revision() != text(scope, "binding_revision") ||
	    binding.commit() != text(scope, "baseline") || binding.value().value("workspace") != reference.value ||
	    binding.value().value("choice_version") != scope.value("choice_version") || !binding.matchesModel(beforeModel) ||
	    text(root.latestLegacyAcceptance(), "id") != binding.acceptance() ||
	    text(checkpoint.latestLegacyAcceptance(), "id") != binding.acceptance()) return false;
	const auto baselinePin = pin(root.ticket(text(scope, "baseline"))), targetPin = pin(root.ticket(text(scope, "target")));
	const auto reservation = root.value.value("journal").toObject().value("reservations").toObject().value(text(scope, "reservation")).toObject();
	const auto history = root.value.value("journal").toObject().value("history").toArray();
	int oldIndex = -1, targetIndex = -1;
	for (int i = 0; i < history.size(); ++i) {
		if (history[i].toString() == text(scope, "baseline")) oldIndex = i;
		if (history[i].toString() == text(scope, "target")) targetIndex = i;
	}
	if (oldIndex < 0 || targetIndex <= oldIndex || binding.value().value("raw") != baselinePin ||
	    pin(checkpoint.ticket(text(scope, "baseline"))) != baselinePin || text(root.ticket(text(scope, "target")), "state") != "committed" ||
	    text(reservation, "commit") != text(scope, "target") || reservation.value("active") != QJsonValue(true) ||
	    !readIntent.matches(reference, text(scope, "target"), text(scope, "reservation")) ||
	    !readIntent.reload() || readIntent.state() != "verified" || readIntent.snapshot() != targetPin ||
	    !CloudHistoryRecord::validXml(beforeModel) || !CloudHistoryRecord::validXml(targetRaw) || !CloudHistoryRecord::validXml(afterModel)) return false;
	auto next = scope; next["state"] = "prepared"; next["acceptance"] = binding.acceptance();
	next["baseline_pin"] = baselinePin; next["target_pin"] = targetPin;
	next["before"] = digest(beforeModel); next["raw"] = digest(targetRaw); next["after"] = digest(afterModel);
	if (!valid(next) || !QDir().mkpath(directory)) return false;
	QLockFile lock(QDir(directory).filePath("intent.json.lock")); lock.setStaleLockTime(0); if (!lock.tryLock(0) || !read()) return false;
	if (saved.isEmpty()) {
		for (const auto &name : QDir(directory).entryList(QDir::AllEntries | QDir::Hidden | QDir::System | QDir::NoDotAndDotDot))
			if (name != "intent.json.lock") return false;
		if (!write(next)) return false;
	} else { auto previous = saved; previous["state"] = "prepared"; if (previous != next) return false; }
	if (!saveXml(QDir(directory).filePath("before.xml"), saved.value("before").toObject(), beforeModel) ||
	    !saveXml(QDir(directory).filePath("raw.xml"), saved.value("raw").toObject(), targetRaw) ||
	    !saveXml(QDir(directory).filePath("after.xml"), saved.value("after").toObject(), afterModel)) return false;
	if (state() != "prepared") return true;
	next = saved; next["state"] = "ready"; return write(next);
}
bool CloudWorkspaceReceiveReceipt::transition(const QString &from, const QString &to, const QByteArray &observed)
{
	QLockFile lock(QDir(directory).filePath("intent.json.lock")); lock.setStaleLockTime(0);
	if (!lock.tryLock(0) || !read() || saved.isEmpty()) return false;
	if (!observed.isEmpty() && !accepts(saved.value("after").toObject(), observed)) return false;
	if (state() == to) return true;
	if (state() != from) return false;
	auto next = saved; next["state"] = to; return write(next);
}
bool CloudWorkspaceReceiveReceipt::beginApply() { return transition("ready", "applying"); }
bool CloudWorkspaceReceiveReceipt::applied(const QByteArray &observed) { return !observed.isEmpty() && transition("applying", "applied", observed); }
bool CloudWorkspaceReceiveReceipt::persisted(const QByteArray &observed) { return !observed.isEmpty() && transition("applied", "persisted", observed); }
