// SPDX-License-Identifier: GPL-2.0
#include "cloudsyncworkspacebinding.h"
#include "cloudsyncmanifest.h"
#include "cloudsyncworkspaceupload.h"
#include "cloudsyncworkspacereceipt.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLockFile>
#include <QRegularExpression>
#include <QSaveFile>

namespace {
constexpr qint64 Limit = 64 * 1024, XmlLimit = 64 * 1024 * 1024;
QString text(const QJsonObject &o, const char *key) { return o.value(QLatin1String(key)).toString(); }
bool hash(const QString &s) { return QRegularExpression("^[a-f0-9]{64}$").match(s).hasMatch(); }
bool size(const QJsonValue &v) { const auto d = v.toDouble(-1); return v.isDouble() && d >= 1 && d <= XmlLimit && d == qint64(d); }
bool fields(const QJsonObject &o, const QStringList &keys) {
	if (o.size() != keys.size()) return false; for (const auto &key : keys) if (!o.contains(key)) return false; return true;
}
bool validXml(const QByteArray &bytes) { return !bytes.isEmpty() && bytes.size() <= XmlLimit && CloudHistoryRecord::validXml(bytes); }
QJsonObject pin(const QJsonObject &ticket) {
	return {{"name", ticket.value("name")}, {"object", ticket.value("object")}, {"version", ticket.value("version")},
		{"sha256", ticket.value("sha256")}, {"bytes", ticket.value("bytes")}};
}
bool validPin(const CloudWorkspaceReference &ref, const QString &commit, const QJsonObject &raw)
{
	const auto version = text(raw, "version");
	return fields(raw, {"name", "object", "version", "sha256", "bytes"}) &&
		text(raw, "name") == ref.payloadName(commit) &&
		CloudWorkspaceReference::create(text(ref.value, "provider"), text(ref.value, "account"), text(ref.value, "epoch"), text(raw, "object")).valid() &&
		text(raw, "object") != text(ref.value, "root_id") && !version.isEmpty() && version.size() <= 512 &&
		!version.contains(QChar::Null) && !version.contains('\n') && !version.contains('\r') &&
		hash(text(raw, "sha256")) && size(raw.value("bytes"));
}
bool validModel(const QJsonObject &model)
{
	return fields(model, {"sha256", "bytes"}) && hash(text(model, "sha256")) && size(model.value("bytes"));
}
}

CloudWorkspaceBinding::CloudWorkspaceBinding(const QString &file, const CloudWorkspaceReference &ref, const QString &choice) : fileName(file), reference(ref)
{
	if (ref.valid() && CloudHistoryRecord::validEpoch(choice))
		scope = {{"schema", 1}, {"format", "neo-workspace-sync-binding-development-1"}, {"workspace", ref.value}, {"choice_version", choice}};
}
QString CloudWorkspaceBinding::revision() const { return text(saved, "revision"); }
QString CloudWorkspaceBinding::commit() const { return text(saved, "commit"); }
QString CloudWorkspaceBinding::acceptance() const { return text(saved, "legacy_acceptance"); }
bool CloudWorkspaceBinding::matchesModel(const QByteArray &model) const
{
	const auto expected = saved.value("model").toObject();
	return validXml(model) && model.size() == expected.value("bytes").toDouble() && CloudSyncManifest::sha256(model) == text(expected, "sha256");
}
bool CloudWorkspaceBinding::read()
{
	saved = {}; if (scope.isEmpty() || fileName.isEmpty()) return false;
	const QFileInfo info(fileName); if (info.isSymLink()) return false; if (!info.exists()) return true;
	if (!info.isFile() || info.size() < 1 || info.size() > Limit) return false;
	QFile file(fileName); if (!file.open(QIODevice::ReadOnly)) return false;
	const auto bytes = file.read(Limit + 1); const auto body = QJsonDocument::fromJson(bytes).object();
	if (file.error() != QFileDevice::NoError || bytes.size() > Limit || !fields(body, {"schema", "format", "workspace", "choice_version", "revision", "commit", "raw", "model", "legacy_acceptance", "previous"})) return false;
	auto fixed = body; for (const auto &key : {"revision", "commit", "raw", "model", "legacy_acceptance", "previous"}) fixed.remove(QLatin1String(key));
	const auto raw = body.value("raw").toObject(), model = body.value("model").toObject(); const auto c = text(body, "commit");
	if (fixed != scope || !CloudHistoryRecord::validEpoch(text(body, "revision")) || !CloudHistoryRecord::validEpoch(c) ||
	    !CloudHistoryRecord::validEpoch(text(body, "legacy_acceptance")) || !validPin(reference, c, raw) || !validModel(model)) return false;
	const auto prior = body.value("previous").toObject();
	if (!body.value("previous").isObject() || (!prior.isEmpty() &&
	    (!fields(prior, {"revision", "commit", "raw", "model"}) || !CloudHistoryRecord::validEpoch(text(prior, "revision")) ||
	     !CloudHistoryRecord::validEpoch(text(prior, "commit")) || text(prior, "revision") == text(body, "revision") ||
	     text(prior, "commit") == c || !validPin(reference, text(prior, "commit"), prior.value("raw").toObject()) ||
	     !validModel(prior.value("model").toObject())))) return false;
	saved = body; return true;
}
bool CloudWorkspaceBinding::inspect() { return read(); }
bool CloudWorkspaceBinding::load()
{
	if (fileName.isEmpty()) return false;
	if (!QFileInfo(QFileInfo(fileName).absolutePath()).exists()) return read();
	QLockFile lock(fileName + ".lock"); return lock.tryLock(0) && read();
}
bool CloudWorkspaceBinding::write(const QJsonObject &body)
{
	const auto bytes = QJsonDocument(body).toJson(QJsonDocument::Compact); if (bytes.size() > Limit) return false;
	QSaveFile file(fileName);
	return file.open(QIODevice::WriteOnly) && file.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner) &&
		file.write(bytes) == bytes.size() && file.commit() && read() && saved == body;
}
QJsonObject CloudWorkspaceBinding::next(const CloudWorkspaceRecord &root, const QByteArray &raw, const QByteArray &model, const QString &newRevision) const
{
	return nextAt(root, root.head(), raw, model, newRevision);
}
QJsonObject CloudWorkspaceBinding::nextAt(const CloudWorkspaceRecord &root, const QString &commitId, const QByteArray &raw, const QByteArray &model, const QString &newRevision) const
{
	if (scope.isEmpty() || !root.valid() || root.reference().value != reference.value || !CloudHistoryRecord::validEpoch(newRevision) || !validXml(raw) || !validXml(model)) return {};
	const auto ticket = root.ticket(commitId), event = root.latestLegacyAcceptance();
	if (!CloudHistoryRecord::validEpoch(commitId) || text(ticket, "state") != "committed" || !CloudHistoryRecord::validEpoch(text(event, "id")) ||
	    ticket.value("bytes").toDouble() != raw.size() || text(ticket, "sha256") != CloudSyncManifest::sha256(raw)) return {};
	auto body = scope; body["revision"] = newRevision; body["commit"] = commitId; body["legacy_acceptance"] = event.value("id");
	body["raw"] = pin(ticket);
	body["model"] = QJsonObject{{"sha256", CloudSyncManifest::sha256(model)}, {"bytes", double(model.size())}}; body["previous"] = QJsonObject(); return body;
}
bool CloudWorkspaceBinding::activate(const CloudWorkspaceRecord &root, const CloudWorkspaceRecord &checkpoint, const QByteArray &raw, const QByteArray &model, const QString &newRevision)
{
	const QFileInfo parent(QFileInfo(fileName).absolutePath()); if (scope.isEmpty() || fileName.isEmpty() || parent.isSymLink() || !QDir().mkpath(parent.filePath())) return false;
	QLockFile lock(fileName + ".lock");
	if (!lock.tryLock(0) || !read() || raw != model || !checkpoint.valid() || checkpoint.json() != root.json()) return false;
	const auto body = next(root, raw, model, newRevision);
	return !body.isEmpty() && (saved.isEmpty() ? write(body) : saved == body);
}
bool CloudWorkspaceBinding::ownsActivation(const CloudWorkspaceRecord &root, const CloudWorkspaceRecord &checkpoint,
	const QByteArray &raw, const QByteArray &model, const QString &expectedRevision, const QString &target)
{
	if (fileName.isEmpty() || !CloudHistoryRecord::validEpoch(expectedRevision) || !CloudHistoryRecord::validEpoch(target) || raw != model) return false;
	QLockFile lock(fileName + ".lock");
	if (!lock.tryLock(0) || !read() || saved.isEmpty() || !saved.value("previous").toObject().isEmpty() || revision() != expectedRevision ||
	    commit() != target || !root.valid() || !checkpoint.valid() || !root.follows(checkpoint) ||
	    root.reference().value != reference.value || checkpoint.reference().value != reference.value || !matchesModel(model) ||
	    saved.value("raw").toObject() != pin(root.ticket(target)) || saved.value("raw").toObject() != pin(checkpoint.ticket(target))) return false;
	const auto t = root.ticket(target);
	if (text(t, "state") != "committed" || text(t, "sha256") != CloudSyncManifest::sha256(raw) || t.value("bytes").toDouble() != raw.size()) return false;
	bool foundRoot = false, foundCheckpoint = false;
	for (const auto &entry : root.value.value("legacy_acceptances").toArray()) if (text(entry.toObject(), "id") == acceptance()) foundRoot = true;
	for (const auto &entry : checkpoint.value.value("legacy_acceptances").toArray()) if (text(entry.toObject(), "id") == acceptance()) foundCheckpoint = true;
	return foundRoot && foundCheckpoint;
}
bool CloudWorkspaceBinding::advanceOwnedUpload(const QString &expectedRevision, const CloudWorkspaceRecord &root, const CloudWorkspaceRecord &checkpoint,
	const QByteArray &raw, const QByteArray &model, const QString &uploadIntentFile, const QString &newRevision)
{
	if (fileName.isEmpty() || !CloudHistoryRecord::validEpoch(expectedRevision) || expectedRevision == newRevision) return false;
	QLockFile lock(fileName + ".lock"); if (!lock.tryLock(0) || !read() || saved.isEmpty()) return false;
	const bool replay = revision() == newRevision && text(saved.value("previous").toObject(), "revision") == expectedRevision;
	if (!replay && revision() != expectedRevision) return false;
	const auto body = next(root, raw, model, newRevision);
	const auto old = replay ? saved.value("previous").toObject() : saved;
	if (body.isEmpty() || !checkpoint.valid() || !root.follows(checkpoint) || checkpoint.reference().value != reference.value ||
	    body.value("legacy_acceptance") != saved.value("legacy_acceptance") || text(checkpoint.latestLegacyAcceptance(), "id") != acceptance() ||
	    old.value("raw").toObject() != pin(root.ticket(text(old, "commit"))) ||
	    old.value("raw").toObject() != pin(checkpoint.ticket(text(old, "commit")))) return false;
	const auto history = root.value.value("journal").toObject().value("history").toArray(); int oldIndex = -1, newIndex = -1;
	for (int i = 0; i < history.size(); ++i) { const auto id = history[i].toString(); if (id == text(old, "commit")) oldIndex = i; if (id == text(body, "commit")) newIndex = i; }
	if (oldIndex < 0 || newIndex != oldIndex + 1 || uploadIntentFile.isEmpty()) return false;
	const auto t = root.ticket(text(body, "commit"));
	if (text(t, "parent") != text(old, "commit") || !text(t, "restore_source").isEmpty() || root.copySource(text(body, "commit")).valid() || raw != model) return false;
	CloudWorkspaceUploadIntent proof(uploadIntentFile, reference, text(t, "sha256"), qint64(t.value("bytes").toDouble()), text(body, "commit"), text(t, "parent"));
	if (!proof.reload() || proof.state() != "committed" || proof.id() != text(t, "object") || proof.version() != text(t, "version")) return false;
	auto nextBody = body; nextBody["previous"] = QJsonObject{{"revision", old.value("revision")}, {"commit", old.value("commit")}, {"raw", old.value("raw")}, {"model", old.value("model")}};
	return replay ? saved == nextBody : write(nextBody);
}
bool CloudWorkspaceBinding::advanceReceived(const QString &expectedRevision, const CloudWorkspaceRecord &root, const CloudWorkspaceRecord &checkpoint,
	CloudWorkspaceReceiveReceipt &receipt, CloudWorkspaceReadIntent &reader, const QByteArray &currentModel, const QString &newRevision)
{
	if (fileName.isEmpty() || !CloudHistoryRecord::validEpoch(expectedRevision) || expectedRevision == newRevision) return false;
	QLockFile lock(fileName + ".lock"); if (!lock.tryLock(0) || !read() || saved.isEmpty() || !receipt.reload() || receipt.state() != "persisted") return false;
	const bool replay = revision() == newRevision && text(saved.value("previous").toObject(), "revision") == expectedRevision;
	if (!replay && revision() != expectedRevision) return false;
	const auto old = replay ? saved.value("previous").toObject() : saved, proof = receipt.proof();
	const auto baseline = text(old, "commit"), target = text(proof, "target"), reservation = text(proof, "reservation");
	const auto raw = receipt.raw(), before = receipt.before(), after = receipt.after();
	const auto body = nextAt(root, target, raw, after, newRevision);
	if (body.isEmpty() || currentModel != after || !checkpoint.valid() || !root.follows(checkpoint) || checkpoint.reference().value != reference.value ||
	    proof.value("workspace") != reference.value || proof.value("choice_version") != scope.value("choice_version") ||
	    text(proof, "binding_revision") != expectedRevision || text(proof, "next_revision") != newRevision || text(proof, "baseline") != baseline ||
	    proof.value("acceptance") != saved.value("legacy_acceptance") || body.value("legacy_acceptance") != saved.value("legacy_acceptance") ||
	    text(checkpoint.latestLegacyAcceptance(), "id") != acceptance() ||
	    proof.value("baseline_pin") != old.value("raw") || old.value("raw").toObject() != pin(root.ticket(baseline)) ||
	    old.value("raw").toObject() != pin(checkpoint.ticket(baseline)) || proof.value("target_pin").toObject() != pin(root.ticket(target)) ||
	    old.value("model").toObject() != QJsonObject{{"sha256", CloudSyncManifest::sha256(before)}, {"bytes", double(before.size())}} ||
	    !reader.matches(reference, target, reservation) || !reader.reload() || reader.state() != "verified" ||
	    reader.snapshot() != pin(root.ticket(target))) return false;
	const auto r = root.value.value("journal").toObject().value("reservations").toObject().value(reservation).toObject();
	if (text(r, "commit") != target || r.value("active") != QJsonValue(true)) return false;
	const auto history = root.value.value("journal").toObject().value("history").toArray(); int oldIndex = -1, targetIndex = -1;
	for (int i = 0; i < history.size(); ++i) {
		if (history[i].toString() == baseline) oldIndex = i;
		if (history[i].toString() == target) targetIndex = i;
	}
	if (oldIndex < 0 || targetIndex <= oldIndex) return false;
	auto nextBody = body; nextBody["previous"] = QJsonObject{{"revision", old.value("revision")}, {"commit", old.value("commit")}, {"raw", old.value("raw")}, {"model", old.value("model")}};
	return replay ? saved == nextBody : write(nextBody);
}
bool CloudWorkspaceBinding::ownsReceived(const CloudWorkspaceRecord &root, const CloudWorkspaceRecord &checkpoint,
	CloudWorkspaceReceiveReceipt &receipt, CloudWorkspaceReadIntent &reader)
{
	if (fileName.isEmpty()) return false;
	QLockFile lock(fileName + ".lock");
	if (!lock.tryLock(0) || !read() || saved.isEmpty() || !receipt.reload() || receipt.state() != "persisted" ||
	    !root.valid() || !checkpoint.valid() || !root.follows(checkpoint) || root.reference().value != reference.value ||
	    checkpoint.reference().value != reference.value) return false;
	const auto proof = receipt.proof(), old = saved.value("previous").toObject();
	const auto target = text(proof, "target"), baseline = text(proof, "baseline"), reservation = text(proof, "reservation");
	const auto before = receipt.before(), raw = receipt.raw(), after = receipt.after();
	const QJsonObject beforeDigest{{"sha256", CloudSyncManifest::sha256(before)}, {"bytes", double(before.size())}};
	const QJsonObject afterDigest{{"sha256", CloudSyncManifest::sha256(after)}, {"bytes", double(after.size())}};
	if (old.isEmpty() || proof.value("workspace") != reference.value || proof.value("choice_version") != scope.value("choice_version") ||
	    text(proof, "binding_revision") != text(old, "revision") || text(proof, "next_revision") != revision() ||
	    baseline != text(old, "commit") || target != commit() || proof.value("acceptance") != saved.value("legacy_acceptance") ||
	    proof.value("baseline_pin") != old.value("raw") || proof.value("target_pin") != saved.value("raw") ||
	    old.value("raw").toObject() != pin(root.ticket(baseline)) || old.value("raw").toObject() != pin(checkpoint.ticket(baseline)) ||
	    saved.value("raw").toObject() != pin(root.ticket(target)) || old.value("model").toObject() != beforeDigest ||
	    saved.value("model").toObject() != afterDigest || !validXml(raw) ||
	    !reader.matches(reference, target, reservation) || !reader.reload() || reader.snapshot() != saved.value("raw").toObject()) return false;
	const auto r = root.value.value("journal").toObject().value("reservations").toObject().value(reservation).toObject();
	if (text(r, "commit") != target || !r.value("active").isBool()) return false;
	if (reader.state() == "verified") return r.value("active") == QJsonValue(true);
	if (reader.state() == "releasing") return true;
	return reader.state() == "released" && r.value("active") == QJsonValue(false);
}
bool CloudWorkspaceBinding::ownsUpload(const CloudWorkspaceRecord &root, const CloudWorkspaceRecord &checkpoint,
	const QString &expectedPreviousRevision, const QString &ticket, const QString &uploadIntentFile, const QByteArray &uploadedModel)
{
	if (fileName.isEmpty() || uploadIntentFile.isEmpty() || !CloudHistoryRecord::validEpoch(expectedPreviousRevision)) return false;
	QLockFile lock(fileName + ".lock");
	if (!lock.tryLock(0) || !read() || saved.isEmpty() || !root.valid() || !checkpoint.valid() ||
	    !root.follows(checkpoint) || root.reference().value != reference.value || checkpoint.reference().value != reference.value ||
	    commit() != ticket || !matchesModel(uploadedModel)) return false;
	const auto old = saved.value("previous").toObject(), t = root.ticket(ticket); const auto previousCommit = text(old, "commit");
	if (old.isEmpty() || text(old, "revision") != expectedPreviousRevision || old.value("raw").toObject() != pin(root.ticket(previousCommit)) ||
	    old.value("raw").toObject() != pin(checkpoint.ticket(previousCommit)) || saved.value("raw").toObject() != pin(t) ||
	    text(t, "state") != "committed" || text(t, "parent") != previousCommit || !text(t, "restore_source").isEmpty() ||
	    root.copySource(ticket).valid() || text(t, "sha256") != CloudSyncManifest::sha256(uploadedModel) ||
	    t.value("bytes").toDouble() != uploadedModel.size()) return false;
	const auto history = root.value.value("journal").toObject().value("history").toArray(); int oldIndex = -1, newIndex = -1;
	for (int i = 0; i < history.size(); ++i) { if (history[i].toString() == previousCommit) oldIndex = i; if (history[i].toString() == ticket) newIndex = i; }
	if (oldIndex < 0 || newIndex != oldIndex + 1) return false;
	CloudWorkspaceUploadIntent proof(uploadIntentFile, reference, text(t, "sha256"), qint64(t.value("bytes").toDouble()), ticket, previousCommit);
	return proof.reload() && proof.state() == "committed" && proof.id() == text(t, "object") && proof.version() == text(t, "version");
}
