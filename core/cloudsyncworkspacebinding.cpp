// SPDX-License-Identifier: GPL-2.0
#include "cloudsyncworkspacebinding.h"
#include "cloudsyncmanifest.h"
#include "cloudsyncworkspaceupload.h"
#include "cloudsyncworkspacereceipt.h"
#include "cloudsyncworkspaceapply.h"
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
bool receivedBeforeMatches(const QJsonObject &old, const QJsonObject &proof, const QByteArray &before,
	const QJsonObject &scope, const QJsonValue &acceptance)
{
	const QJsonObject digest{{"sha256", CloudSyncManifest::sha256(before)}, {"bytes", double(before.size())}};
	if (!validXml(before)) return false;
	if (text(proof, "format") == "neo-workspace-receive-development-1")
		return !proof.contains("reviewed_binding") && old.value("model") == digest;
	if (text(proof, "format") != "neo-workspace-conflict-receive-development-1") return false;
	const auto reviewed = proof.value("reviewed_binding").toObject();
	if (reviewed.value("workspace") != scope.value("workspace") || reviewed.value("choice_version") != scope.value("choice_version") ||
	    reviewed.value("legacy_acceptance") != acceptance || reviewed.value("model") == digest) return false;
	for (const auto &key : {"revision", "commit", "raw", "model"})
		if (reviewed.value(QLatin1String(key)) != old.value(QLatin1String(key))) return false;
	return true;
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
	QLockFile lock(fileName + ".lock"); lock.setStaleLockTime(0); return lock.tryLock(0) && read();
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
	QLockFile lock(fileName + ".lock"); lock.setStaleLockTime(0);
	if (!lock.tryLock(0) || !read() || raw != model || !checkpoint.valid() || checkpoint.json() != root.json()) return false;
	const auto body = next(root, raw, model, newRevision);
	return !body.isEmpty() && (saved.isEmpty() ? write(body) : saved == body);
}
bool CloudWorkspaceBinding::ownsActivation(const CloudWorkspaceRecord &root, const CloudWorkspaceRecord &checkpoint,
	const QByteArray &raw, const QByteArray &model, const QString &expectedRevision, const QString &target)
{
	if (fileName.isEmpty() || !CloudHistoryRecord::validEpoch(expectedRevision) || !CloudHistoryRecord::validEpoch(target) || raw != model) return false;
	QLockFile lock(fileName + ".lock"); lock.setStaleLockTime(0);
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
	QLockFile lock(fileName + ".lock"); lock.setStaleLockTime(0); if (!lock.tryLock(0) || !read() || saved.isEmpty()) return false;
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
bool CloudWorkspaceBinding::advanceConflictUpload(const QJsonObject &reviewedBinding, const QString &reviewedHead,
	const CloudWorkspaceRecord &root, const CloudWorkspaceRecord &checkpoint, const QByteArray &model,
	const QString &uploadIntentFile, const QString &newRevision)
{
	const auto oldRevision = text(reviewedBinding, "revision"), baseline = text(reviewedBinding, "commit"), ticket = root.head();
	if (fileName.isEmpty() || uploadIntentFile.isEmpty() || !CloudHistoryRecord::validEpoch(oldRevision) ||
	    !CloudHistoryRecord::validEpoch(newRevision) || !CloudHistoryRecord::validEpoch(reviewedHead) ||
	    !CloudHistoryRecord::validEpoch(baseline) || oldRevision == newRevision || reviewedHead == baseline ||
	    !validXml(model) || reviewedBinding.value("model").toObject() ==
		QJsonObject{{"sha256", CloudSyncManifest::sha256(model)}, {"bytes", double(model.size())}}) return false;
	QLockFile lock(fileName + ".lock"); lock.setStaleLockTime(0);
	if (!lock.tryLock(0) || !read() || saved.isEmpty()) return false;
	const QJsonObject previous{{"revision", reviewedBinding.value("revision")}, {"commit", reviewedBinding.value("commit")},
		{"raw", reviewedBinding.value("raw")}, {"model", reviewedBinding.value("model")}};
	const bool replay = revision() == newRevision && saved.value("previous").toObject() == previous;
	if (!replay && saved != reviewedBinding) return false;
	const auto body = next(root, model, model, newRevision), t = root.ticket(ticket);
	if (body.isEmpty() || !checkpoint.valid() || !root.follows(checkpoint) || checkpoint.reference().value != reference.value ||
	    reviewedBinding.value("workspace") != reference.value || reviewedBinding.value("choice_version") != scope.value("choice_version") ||
	    body.value("legacy_acceptance") != reviewedBinding.value("legacy_acceptance") ||
	    text(checkpoint.latestLegacyAcceptance(), "id") != text(reviewedBinding, "legacy_acceptance") ||
	    reviewedBinding.value("raw").toObject() != pin(root.ticket(baseline)) ||
	    reviewedBinding.value("raw").toObject() != pin(checkpoint.ticket(baseline)) ||
	    pin(root.ticket(reviewedHead)) != pin(checkpoint.ticket(reviewedHead)) ||
	    text(root.ticket(reviewedHead), "state") != "committed" || text(t, "state") != "committed" ||
	    text(t, "parent") != reviewedHead || !text(t, "restore_source").isEmpty() || root.copySource(ticket).valid()) return false;
	const auto history = root.value.value("journal").toObject().value("history").toArray();
	int baselineIndex = -1, headIndex = -1, ticketIndex = -1;
	for (int i = 0; i < history.size(); ++i) {
		if (history[i].toString() == baseline) baselineIndex = i;
		if (history[i].toString() == reviewedHead) headIndex = i;
		if (history[i].toString() == ticket) ticketIndex = i;
	}
	if (baselineIndex < 0 || headIndex <= baselineIndex || ticketIndex != headIndex + 1) return false;
	CloudWorkspaceUploadIntent owned(uploadIntentFile, reference, text(t, "sha256"), qint64(t.value("bytes").toDouble()), ticket, reviewedHead);
	if (!owned.reload() || owned.state() != "committed" || owned.id() != text(t, "object") || owned.version() != text(t, "version")) return false;
	auto nextBody = body; nextBody["previous"] = previous;
	return replay ? saved == nextBody : write(nextBody);
}

bool CloudWorkspaceBinding::ownsConflictUpload(const QJsonObject &reviewedBinding, const QString &reviewedHead,
	const CloudWorkspaceRecord &root, const CloudWorkspaceRecord &checkpoint, const QByteArray &model,
	const QString &ticket, const QString &uploadIntentFile, const QString &newRevision)
{
	const auto oldRevision = text(reviewedBinding, "revision"), baseline = text(reviewedBinding, "commit");
	if (fileName.isEmpty() || uploadIntentFile.isEmpty() || !CloudHistoryRecord::validEpoch(oldRevision) ||
	    !CloudHistoryRecord::validEpoch(newRevision) || !CloudHistoryRecord::validEpoch(reviewedHead) ||
	    !CloudHistoryRecord::validEpoch(baseline) || !CloudHistoryRecord::validEpoch(ticket) ||
	    oldRevision == newRevision || reviewedHead == baseline || !validXml(model) ||
	    reviewedBinding.value("model").toObject() ==
		QJsonObject{{"sha256", CloudSyncManifest::sha256(model)}, {"bytes", double(model.size())}}) return false;
	QLockFile lock(fileName + ".lock"); lock.setStaleLockTime(0);
	const QJsonObject previous{{"revision", reviewedBinding.value("revision")}, {"commit", reviewedBinding.value("commit")},
		{"raw", reviewedBinding.value("raw")}, {"model", reviewedBinding.value("model")}};
	if (!lock.tryLock(0) || !read() || saved.isEmpty() || revision() != newRevision || commit() != ticket ||
	    saved.value("previous").toObject() != previous || saved.value("legacy_acceptance") != reviewedBinding.value("legacy_acceptance") ||
	    reviewedBinding.value("workspace") != reference.value || reviewedBinding.value("choice_version") != scope.value("choice_version") ||
	    !root.valid() || !checkpoint.valid() || !root.follows(checkpoint) || root.reference().value != reference.value ||
	    checkpoint.reference().value != reference.value || !matchesModel(model)) return false;
	const auto t = root.ticket(ticket);
	if (reviewedBinding.value("raw").toObject() != pin(root.ticket(baseline)) ||
	    reviewedBinding.value("raw").toObject() != pin(checkpoint.ticket(baseline)) ||
	    pin(root.ticket(reviewedHead)) != pin(checkpoint.ticket(reviewedHead)) ||
	    saved.value("raw").toObject() != pin(t) || text(t, "state") != "committed" ||
	    text(t, "parent") != reviewedHead || !text(t, "restore_source").isEmpty() || root.copySource(ticket).valid() ||
	    text(t, "sha256") != CloudSyncManifest::sha256(model) || t.value("bytes").toDouble() != model.size()) return false;
	const auto history = root.value.value("journal").toObject().value("history").toArray();
	int baselineIndex = -1, headIndex = -1, ticketIndex = -1;
	for (int i = 0; i < history.size(); ++i) {
		if (history[i].toString() == baseline) baselineIndex = i;
		if (history[i].toString() == reviewedHead) headIndex = i;
		if (history[i].toString() == ticket) ticketIndex = i;
	}
	if (baselineIndex < 0 || headIndex <= baselineIndex || ticketIndex != headIndex + 1) return false;
	CloudWorkspaceUploadIntent owned(uploadIntentFile, reference, text(t, "sha256"), qint64(t.value("bytes").toDouble()), ticket, reviewedHead);
	return owned.reload() && owned.state() == "committed" && owned.id() == text(t, "object") && owned.version() == text(t, "version");
}

bool CloudWorkspaceBinding::advanceReceived(const QString &expectedRevision, const CloudWorkspaceRecord &root, const CloudWorkspaceRecord &checkpoint,
	CloudWorkspaceReceiveReceipt &receipt, CloudWorkspaceReadIntent &reader, const QByteArray &currentModel, const QString &newRevision)
{
	if (fileName.isEmpty() || !CloudHistoryRecord::validEpoch(expectedRevision) || expectedRevision == newRevision) return false;
	QLockFile lock(fileName + ".lock"); lock.setStaleLockTime(0); if (!lock.tryLock(0) || !read() || saved.isEmpty() || !receipt.reload() || receipt.state() != "persisted") return false;
	const bool replay = revision() == newRevision && text(saved.value("previous").toObject(), "revision") == expectedRevision;
	if (!replay && revision() != expectedRevision) return false;
	const auto old = replay ? saved.value("previous").toObject() : saved, proof = receipt.proof();
	if (!replay && proof.contains("reviewed_binding") && saved != proof.value("reviewed_binding").toObject()) return false;
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
	    !receivedBeforeMatches(old, proof, before, scope, saved.value("legacy_acceptance")) ||
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
	QLockFile lock(fileName + ".lock"); lock.setStaleLockTime(0);
	if (!lock.tryLock(0) || !read() || saved.isEmpty() || !receipt.reload() || receipt.state() != "persisted" ||
	    !root.valid() || !checkpoint.valid() || !root.follows(checkpoint) || root.reference().value != reference.value ||
	    checkpoint.reference().value != reference.value) return false;
	const auto proof = receipt.proof(), old = saved.value("previous").toObject();
	const auto target = text(proof, "target"), baseline = text(proof, "baseline"), reservation = text(proof, "reservation");
	const auto before = receipt.before(), raw = receipt.raw(), after = receipt.after();
	const QJsonObject afterDigest{{"sha256", CloudSyncManifest::sha256(after)}, {"bytes", double(after.size())}};
	if (old.isEmpty() || proof.value("workspace") != reference.value || proof.value("choice_version") != scope.value("choice_version") ||
	    text(proof, "binding_revision") != text(old, "revision") || text(proof, "next_revision") != revision() ||
	    baseline != text(old, "commit") || target != commit() || proof.value("acceptance") != saved.value("legacy_acceptance") ||
	    proof.value("baseline_pin") != old.value("raw") || proof.value("target_pin") != saved.value("raw") ||
	    old.value("raw").toObject() != pin(root.ticket(baseline)) || old.value("raw").toObject() != pin(checkpoint.ticket(baseline)) ||
	    saved.value("raw").toObject() != pin(root.ticket(target)) || !receivedBeforeMatches(old, proof, before, scope, saved.value("legacy_acceptance")) ||
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
	QLockFile lock(fileName + ".lock"); lock.setStaleLockTime(0);
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

bool CloudWorkspaceBinding::advanceRestored(const QString &expectedRevision, const CloudWorkspaceRecord &root, const CloudWorkspaceRecord &checkpoint,
	CloudWorkspaceApplyIntent &receipt, CloudWorkspaceReadIntent &sourceReader, CloudWorkspaceReadIntent &applyReader,
	const QString &uploadIntentFile, const QByteArray &beforeModel, const QByteArray &currentModel,
	const QJsonObject &projection, const QString &planSha256, const QString &newRevision)
{
	if (fileName.isEmpty() || uploadIntentFile.isEmpty() || !CloudHistoryRecord::validEpoch(expectedRevision) ||
	    !CloudHistoryRecord::validEpoch(newRevision) || expectedRevision == newRevision || !validXml(beforeModel) || !validXml(currentModel)) return false;
	QLockFile lock(fileName + ".lock"); lock.setStaleLockTime(0);
	if (!lock.tryLock(0) || !read() || saved.isEmpty() || !receipt.inspect() || receipt.state() != "persisted") return false;
	const bool replay = revision() == newRevision && text(saved.value("previous").toObject(), "revision") == expectedRevision;
	if (!replay && revision() != expectedRevision) return false;
	const auto old = replay ? saved.value("previous").toObject() : saved, proof = receipt.proof();
	const auto baseline = text(old, "commit"), target = text(proof, "commit"), source = text(proof, "source");
	const auto sourceReservation = text(proof, "source_reservation"), applyReservation = text(proof, "reservation");
	const auto beforeRaw = receipt.before(), afterRaw = receipt.after();
	const auto body = nextAt(root, target, afterRaw, currentModel, newRevision);
	const QJsonObject beforeDigest{{"sha256", CloudSyncManifest::sha256(beforeModel)}, {"bytes", double(beforeModel.size())}};
	if (!fields(projection, {"schema", "format", "plan_sha256", "source_snapshot", "model_sha256", "model_bytes"}) ||
	    projection.value("schema") != QJsonValue(1) || text(projection, "format") != "neo-workspace-model-projection-development-1" ||
	    !hash(planSha256) || text(projection, "plan_sha256") != planSha256 || projection.value("source_snapshot") != proof.value("source_snapshot") ||
	    text(projection, "model_sha256") != CloudSyncManifest::sha256(currentModel) || projection.value("model_bytes") != QJsonValue(double(currentModel.size())) ||
	    body.isEmpty() || !checkpoint.valid() || !root.follows(checkpoint) || checkpoint.reference().value != reference.value ||
	    proof.value("workspace") != reference.value || proof.value("choice_version") != scope.value("choice_version") ||
	    text(proof, "parent") != baseline || proof.value("before") != old.value("raw") || old.value("model").toObject() != beforeDigest ||
	    old.value("raw").toObject() != pin(root.ticket(baseline)) || old.value("raw").toObject() != pin(checkpoint.ticket(baseline)) ||
	    text(old.value("raw").toObject(), "sha256") != CloudSyncManifest::sha256(beforeRaw) ||
	    proof.value("after").toObject() != pin(root.ticket(target)) || proof.value("after").toObject() != pin(checkpoint.ticket(target)) ||
	    proof.value("source_snapshot").toObject() != pin(root.ticket(source)) || proof.value("source_snapshot").toObject() != pin(checkpoint.ticket(source)) ||
	    body.value("legacy_acceptance") != saved.value("legacy_acceptance") || text(checkpoint.latestLegacyAcceptance(), "id") != acceptance() ||
	    !sourceReader.matches(reference, source, sourceReservation) || !sourceReader.reload() || sourceReader.state() != "released" ||
	    sourceReader.snapshot() != proof.value("source_snapshot").toObject() || !applyReader.matches(reference, target, applyReservation) ||
	    !applyReader.reload() || applyReader.state() != "released" || applyReader.snapshot() != proof.value("after").toObject()) return false;
	const auto reservationsReleased = [&](const CloudWorkspaceRecord &record) {
		const auto entries = record.value.value("journal").toObject().value("reservations").toObject();
		const auto sourceEntry = entries.value(sourceReservation).toObject(), applyEntry = entries.value(applyReservation).toObject();
		return text(sourceEntry, "commit") == source && sourceEntry.value("active") == QJsonValue(false) &&
			text(applyEntry, "commit") == target && applyEntry.value("active") == QJsonValue(false);
	};
	if (!reservationsReleased(root) || !reservationsReleased(checkpoint)) return false;
	const auto ticket = root.ticket(target), sourceTicket = root.ticket(source);
	if (text(ticket, "state") != "committed" || text(ticket, "parent") != baseline || text(ticket, "restore_source") != source ||
	    text(ticket, "reservation") != sourceReservation || root.copySource(target).valid() || text(sourceTicket, "state") != "committed") return false;
	const auto history = root.value.value("journal").toObject().value("history").toArray(); int oldIndex = -1, targetIndex = -1;
	for (int i = 0; i < history.size(); ++i) { if (history[i].toString() == baseline) oldIndex = i; if (history[i].toString() == target) targetIndex = i; }
	if (oldIndex < 0 || targetIndex != oldIndex + 1) return false;
	const QJsonObject restore{{"source", source}, {"reservation", sourceReservation}, {"snapshot", sourceReader.snapshot()}};
	CloudWorkspaceUploadIntent upload(uploadIntentFile, reference, CloudSyncManifest::sha256(afterRaw), afterRaw.size(), target, baseline, restore);
	if (!upload.reload() || upload.state() != "committed" || upload.id() != text(ticket, "object") || upload.version() != text(ticket, "version")) return false;
	auto nextBody = body; nextBody["previous"] = QJsonObject{{"revision", old.value("revision")}, {"commit", old.value("commit")}, {"raw", old.value("raw")}, {"model", old.value("model")}};
	return replay ? saved == nextBody : write(nextBody);
}
