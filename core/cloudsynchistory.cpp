// SPDX-License-Identifier: GPL-2.0
#include "cloudsynchistory.h"
#include "cloudsyncmanifest.h"
#include <QJsonArray>
#include <QJsonDocument>
#include <QRegularExpression>
#include <QSet>
#include <QUuid>
#include <QXmlStreamReader>

namespace {
constexpr qint64 PayloadLimit = 64 * 1024 * 1024;
constexpr int RecordLimit = 1024 * 1024;
constexpr int EntryLimit = 512;
const QString LegacyName = QStringLiteral("subsurface-neo-sync.json");
QString uuid() { return QUuid::createUuid().toString(QUuid::WithoutBraces); }
bool hash(const QString &s) { return QRegularExpression(QStringLiteral("^[a-f0-9]{64}$")).match(s).hasMatch(); }
bool text(const QString &s) { return !s.isEmpty() && s.size() <= 512 && !s.contains(QChar::Null); }
bool fields(const QJsonObject &o, const QStringList &names) {
	if (o.size() != names.size()) return false;
	for (const auto &name : names) if (!o.contains(name)) return false;
	return true;
}
bool integer(const QJsonValue &v, qint64 minimum, qint64 maximum) {
	return v.isDouble() && v.toDouble() >= minimum && v.toDouble() <= maximum && v.toDouble() == double(qint64(v.toDouble()));
}
QString string(const QJsonObject &o, const char *key) { return o.value(QLatin1String(key)).toString(); }
}

bool CloudHistoryRecord::validEpoch(const QString &id)
{
	static const QRegularExpression pattern(QStringLiteral("^[a-f0-9]{8}-[a-f0-9]{4}-[a-f0-9]{4}-[a-f0-9]{4}-[a-f0-9]{12}$"));
	return pattern.match(id).hasMatch();
}
QString CloudHistoryRecord::rootName(const QString &epoch) { return QStringLiteral("subsurface-neo-history-%1.json").arg(epoch); }
QString CloudHistoryRecord::payloadName(const QString &epoch, const QString &id) { return QStringLiteral("subsurface-neo-history-%1-%2.xml").arg(epoch, id); }
bool CloudHistoryRecord::validXml(const QByteArray &payload)
{
	if (payload.isEmpty() || payload.size() > PayloadLimit) return false;
	QXmlStreamReader xml(payload);
	bool root = false;
	while (!xml.atEnd()) {
		xml.readNext();
		if (xml.isDTD()) return false;
		if (xml.isStartElement() && !root) {
			if (xml.name() != QLatin1String("divelog") || !xml.namespaceUri().isEmpty()) return false;
			root = true;
		}
	}
	return root && !xml.hasError();
}
CloudHistoryRecord CloudHistoryRecord::create(const QString &provider, const QString &account, const QString &epoch, const QString &legacyHash)
{
	CloudHistoryRecord r;
	r.value = {{"schema", 3}, {"format", "neo-history-development-1"}, {"provider", provider},
		{"account", account}, {"epoch", epoch}, {"legacy_sha256", legacyHash}, {"sequence", 0},
		{"head", ""}, {"history", QJsonArray()}, {"tickets", QJsonObject()}, {"reservations", QJsonObject()}};
	if (!r.valid()) r.value = {};
	return r;
}
CloudHistoryRecord CloudHistoryRecord::parse(const QByteArray &data)
{
	CloudHistoryRecord r;
	if (data.size() > RecordLimit) return r;
	r.value = QJsonDocument::fromJson(data).object();
	if (!r.valid()) r.value = {};
	return r;
}
QByteArray CloudHistoryRecord::json() const { return QJsonDocument(value).toJson(QJsonDocument::Compact); }
QString CloudHistoryRecord::head() const { return string(value, "head"); }
QJsonObject CloudHistoryRecord::ticket(const QString &id) const { return value.value("tickets").toObject().value(id).toObject(); }
bool CloudHistoryRecord::valid() const
{
	if (!fields(value, {"schema", "format", "provider", "account", "epoch", "legacy_sha256", "sequence", "head", "history", "tickets", "reservations"}) ||
	    value.value("schema") != QJsonValue(3) || string(value, "format") != "neo-history-development-1" ||
	    (string(value, "provider") != "google-drive" && string(value, "provider") != "dropbox") ||
	    !hash(string(value, "account")) || !validEpoch(string(value, "epoch")) || !hash(string(value, "legacy_sha256")) ||
	    !integer(value.value("sequence"), 0, 1000000) || !value.value("head").isString() ||
	    !value.value("history").isArray() || !value.value("tickets").isObject() || !value.value("reservations").isObject()) return false;
	const auto history = value.value("history").toArray();
	const auto tickets = value.value("tickets").toObject(), reservations = value.value("reservations").toObject();
	if (tickets.size() > EntryLimit || reservations.size() > EntryLimit || history.size() > tickets.size() || json().size() > RecordLimit) return false;
	QSet<QString> committed, objects;
	QString previous;
	for (const auto &entry : history) {
		const QString id = entry.toString();
		const auto t = ticket(id);
		if (!validEpoch(id) || committed.contains(id) || string(t, "state") != "committed" || string(t, "parent") != previous) return false;
		const QString source = string(t, "restore_source");
		if (!source.isEmpty() && !committed.contains(source)) return false;
		committed.insert(id); previous = id;
	}
	if (head() != previous) return false;
	for (auto it = tickets.begin(); it != tickets.end(); ++it) {
		const auto t = it.value().toObject();
		const QString state = string(t, "state"), parent = string(t, "parent"), source = string(t, "restore_source"), reservation = string(t, "reservation");
		if (!validEpoch(it.key()) || !fields(t, {"name", "state", "parent", "sha256", "bytes", "object", "version", "restore_source", "reservation", "created_at"}) ||
		    string(t, "name") != payloadName(string(value, "epoch"), it.key()) || !hash(string(t, "sha256")) ||
		    !integer(t.value("bytes"), 1, PayloadLimit) || !QDateTime::fromString(string(t, "created_at"), Qt::ISODateWithMs).isValid() ||
		    (state != "registered" && state != "uploaded" && state != "committed") ||
		    (!parent.isEmpty() && (!committed.contains(parent) || parent == it.key())) ||
		    (state == "committed") != committed.contains(it.key())) return false;
		for (const auto &key : {"parent", "object", "version", "restore_source", "reservation"}) if (!t.value(QLatin1String(key)).isString()) return false;
		if (state == "registered") {
			if (!string(t, "object").isEmpty() || !string(t, "version").isEmpty()) return false;
		} else {
			const QString object = string(t, "object");
			if (!text(object) || !text(string(t, "version")) || objects.contains(object)) return false;
			objects.insert(object);
		}
		if (source.isEmpty() != reservation.isEmpty()) return false;
		if (!source.isEmpty()) {
			const auto r = reservations.value(reservation).toObject();
			if (!committed.contains(source) || source == it.key() || string(r, "commit") != source ||
			    t.value("sha256") != ticket(source).value("sha256") || t.value("bytes") != ticket(source).value("bytes") ||
			    (state != "committed" && !r.value("active").toBool())) return false;
		}
	}
	for (auto it = reservations.begin(); it != reservations.end(); ++it) {
		const auto r = it.value().toObject();
		if (!validEpoch(it.key()) || tickets.contains(it.key()) || !fields(r, {"commit", "active"}) ||
		    !committed.contains(string(r, "commit")) || !r.value("active").isBool()) return false;
	}
	return true;
}
bool CloudHistoryRecord::replace(const QJsonObject &candidate)
{
	CloudHistoryRecord next; next.value = candidate;
	next.value["sequence"] = value.value("sequence").toInt() + 1;
	if (!valid() || !next.valid()) return false;
	value = next.value; return true;
}
bool CloudHistoryRecord::registerUpload(const QString &id, const QString &expectedHead, const QString &sha, qint64 bytes, const QString &source, const QString &reservation)
{
	if (head() != expectedHead || !validEpoch(id)) return false;
	auto tickets = value.value("tickets").toObject();
	if (tickets.contains(id) || value.value("reservations").toObject().contains(id)) return false;
	tickets[id] = QJsonObject{{"name", payloadName(string(value, "epoch"), id)}, {"state", "registered"},
		{"parent", expectedHead}, {"sha256", sha}, {"bytes", bytes}, {"object", ""}, {"version", ""},
		{"restore_source", source}, {"reservation", reservation}, {"created_at", QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs)}};
	auto next = value; next["tickets"] = tickets; return replace(next);
}
bool CloudHistoryRecord::completeUpload(const QString &id, const QString &object, const QString &version)
{
	auto t = ticket(id);
	if (string(t, "state") != "registered") return false;
	t["state"] = "uploaded"; t["object"] = object; t["version"] = version;
	auto tickets = value.value("tickets").toObject(); tickets[id] = t;
	auto next = value; next["tickets"] = tickets; return replace(next);
}
bool CloudHistoryRecord::publish(const QString &id, const QString &expectedHead)
{
	auto t = ticket(id);
	if (head() != expectedHead || string(t, "parent") != expectedHead || string(t, "state") != "uploaded") return false;
	t["state"] = "committed";
	auto tickets = value.value("tickets").toObject(); tickets[id] = t;
	auto history = value.value("history").toArray(); history.append(id);
	auto next = value; next["tickets"] = tickets; next["history"] = history; next["head"] = id; return replace(next);
}
bool CloudHistoryRecord::reserve(const QString &id, const QString &commit)
{
	if (!validEpoch(id) || string(ticket(commit), "state") != "committed") return false;
	auto reservations = value.value("reservations").toObject();
	if (reservations.contains(id) || value.value("tickets").toObject().contains(id)) return false;
	reservations[id] = QJsonObject{{"commit", commit}, {"active", true}};
	auto next = value; next["reservations"] = reservations; return replace(next);
}
bool CloudHistoryRecord::release(const QString &id)
{
	auto reservations = value.value("reservations").toObject();
	auto r = reservations.value(id).toObject();
	if (!r.value("active").toBool()) return false;
	r["active"] = false; reservations[id] = r;
	auto next = value; next["reservations"] = reservations; return replace(next);
}
QVariantMap CloudHistoryRecord::report() const
{
	QVariantList versions;
	const auto history = value.value("history").toArray();
	for (auto it = history.constEnd(); it != history.constBegin();) {
		const QString id = (*--it).toString(); const auto t = ticket(id);
		versions.append(QVariantMap{{"id", id}, {"current", id == head()}, {"bytes", qint64(t.value("bytes").toDouble())},
			{"createdAt", string(t, "created_at")}, {"restored", !string(t, "restore_source").isEmpty()}});
	}
	return {{"initialized", !head().isEmpty()}, {"versions", versions}, {"legacyChanged", false}};
}

CloudSyncHistory::CloudSyncHistory(QNetworkAccessManager *network, QObject *parent) : QObject(parent), files(network, this)
{
	files.setStrictReads(true);
	files.setResponseLimit(PayloadLimit);
	deadline.setSingleShot(true);
	connect(&deadline, &QTimer::timeout, this, [this] { fail(tr("History operation timed out. Reload history before retrying; all files were kept.")); });
	connect(&files, &CloudSyncFileStore::versionedDownloadFinished, this,
		[this](CloudSyncProviderType, const QString &name, const QByteArray &data, const QString &version, const QString &id) { received(name, data, version, id); });
	connect(&files, &CloudSyncFileStore::uploadFinished, this, [this](CloudSyncProviderType, const QString &name) {
		if (step == Step::VerifyRoot) read(name, Step::VerifyRoot);
		else if (step == Step::Upload) read(name, Step::VerifyPayload);
	});
	connect(&files, &CloudSyncFileStore::fileNotFound, this, [this](CloudSyncProviderType, const QString &name) {
		if (step != Step::Root || name != CloudHistoryRecord::rootName(epoch)) { fail(); return; }
		if (action == "list") { complete(); return; }
		if (action != "migrate") { fail(tr("Initialize history from the cloud log first.")); return; }
		root = CloudHistoryRecord::create(cloudSyncProviderDescriptor(provider).id, account, epoch, legacyHash);
		writeRoot(root, [this] { processRoot(); });
	});
	connect(&files, &CloudSyncFileStore::writeConflict, this, [this](CloudSyncProviderType, const QString &) {
		fail(tr("Cloud history changed during this operation. Reload it before retrying. All versions were kept."));
	});
	connect(&files, &CloudSyncFileStore::operationError, this, [this](CloudSyncProviderType, const QString &, const QString &, bool rejected) {
		// A failed acknowledgement is not evidence that a root write failed.
		// Read it once; never repeat the write or remove the uploaded object.
		if (step == Step::VerifyRoot && !pendingRoot.value.isEmpty() && !retriedRootRead && !rejected) {
			retriedRootRead = true;
			files.cancel(provider);
			read(CloudHistoryRecord::rootName(epoch), Step::VerifyRoot);
		} else fail({}, rejected);
	});
}
CloudSyncHistory::~CloudSyncHistory() { cancel(); }
void CloudSyncHistory::cancel()
{
	step = Step::Idle; deadline.stop(); files.cancel(provider); continuation = {}; localUnchanged = {};
	token.clear(); local.clear(); payload.clear(); pendingRoot.value = {};
}
void CloudSyncHistory::fail(const QString &message, bool rejected)
{
	if (step == Step::Idle) return;
	cancel();
	emit failed(message.isEmpty() ? tr("Cloud history could not be confirmed. Reload history before retrying. Local data and cloud files were kept.") : message, rejected);
}
void CloudSyncHistory::start(CloudSyncProviderType type, const QString &accessToken, const QString &accountHash,
	const QString &namespaceEpoch, const QString &operation, const QString &commit, const QByteArray &localPayload, const std::function<bool()> &guard)
{
	cancel(); provider = type; token = accessToken; account = accountHash; epoch = namespaceEpoch; action = operation; revision = commit;
	local = localPayload; localUnchanged = guard; root.value = {}; pendingRoot.value = {}; rootId.clear(); rootVersion.clear(); reservationId.clear(); uploadId.clear();
	step = Step::Legacy;
	if ((type != CloudSyncProviderType::GoogleDrive && type != CloudSyncProviderType::Dropbox) || token.isEmpty() || !hash(account) ||
	    !CloudHistoryRecord::validEpoch(epoch) || !QStringList{"list", "migrate", "save", "restore"}.contains(action) ||
	    !CloudHistoryRecord::validXml(local) || (action == "restore" && !CloudHistoryRecord::validEpoch(revision))) { fail(); return; }
	deadline.start(180000); read(LegacyName, Step::Legacy);
}
void CloudSyncHistory::read(const QString &name, Step next)
{
	step = next;
	files.downloadVersioned(provider, token, name);
}
void CloudSyncHistory::received(const QString &, const QByteArray &data, const QString &version, const QString &id)
{
	if (step == Step::Idle) return;
	if (step == Step::Legacy || step == Step::CheckLegacy) {
		const auto m = CloudSyncManifest::fromJson(data);
		if (!m.isValid()) { fail(tr("The existing cloud manifest is invalid or unsupported. Nothing was migrated.")); return; }
		const QString fingerprint = CloudSyncManifest::sha256(data);
		if (step == Step::CheckLegacy) {
			if (fingerprint != legacyHash) { fail(tr("The existing sync log changed. Its branch was preserved; reload history before continuing.")); return; }
			publish(); return;
		}
		legacyHash = fingerprint; legacyPayload = m.payloadFileName(); legacyPayloadHash = m.payloadSha256;
		read(CloudHistoryRecord::rootName(epoch), Step::Root); return;
	}
	if (step == Step::Root || step == Step::VerifyRoot || step == Step::VerifyReservation) {
		const auto parsed = CloudHistoryRecord::parse(data);
		if (!parsed.valid() || string(parsed.value, "provider") != cloudSyncProviderDescriptor(provider).id ||
		    string(parsed.value, "account") != account || string(parsed.value, "epoch") != epoch || version.isEmpty() || id.isEmpty()) { fail(); return; }
		if (!rootId.isEmpty() && rootId != id) { fail(); return; }
		root = parsed; rootVersion = version; rootId = id;
		if (step == Step::VerifyRoot) {
			if (root.json() != pendingRoot.json()) { fail(); return; }
			pendingRoot.value = {}; auto next = std::move(continuation); continuation = {}; next(); return;
		}
		if (step == Step::VerifyReservation) {
			const auto r = root.value.value("reservations").toObject().value(reservationId).toObject();
			if (root.head() != expectedHead || !r.value("active").toBool() || string(r, "commit") != revision) { fail(); return; }
			registerUpload(); return;
		}
		if (string(root.value, "legacy_sha256") != legacyHash) {
			if (action != "list") { fail(tr("The existing sync log changed after history initialization. Both branches were kept. History writes are paused.")); return; }
			auto report = root.report(); report["legacyChanged"] = true; cancel(); emit finished(report, {}); return;
		}
		processRoot(); return;
	}
	if (step == Step::MigrationPayload) {
		if (!CloudHistoryRecord::validXml(data) || CloudSyncManifest::sha256(data) != legacyPayloadHash) { fail(); return; }
		payload = data; registerUpload(); return;
	}
	if (step == Step::RestorePayload) {
		const auto t = root.ticket(revision);
		if (id != string(t, "object") || version != string(t, "version") || data.size() != qint64(t.value("bytes").toDouble()) ||
		    !CloudHistoryRecord::validXml(data) || CloudSyncManifest::sha256(data) != string(t, "sha256")) { fail(); return; }
		payload = data; read(CloudHistoryRecord::rootName(epoch), Step::VerifyReservation); return;
	}
	if (step == Step::VerifyPayload) {
		if (data != payload || id.isEmpty() || version.isEmpty()) { fail(); return; }
		auto next = root;
		if (!next.completeUpload(uploadId, id, version)) { fail(); return; }
		writeRoot(next, [this] { read(LegacyName, Step::CheckLegacy); });
	}
}
void CloudSyncHistory::processRoot()
{
	if (action == "list" || (action == "migrate" && !root.head().isEmpty())) { complete(); return; }
	expectedHead = root.head();
	if (action == "migrate") { read(legacyPayload, Step::MigrationPayload); return; }
	if (expectedHead.isEmpty()) { fail(tr("Finish initializing history before saving or restoring a version.")); return; }
	if (action == "save") { payload = local; registerUpload(); return; }
	if (CloudSyncManifest::sha256(local) != string(root.ticket(expectedHead), "sha256")) {
		fail(tr("Save your current local log as a history version before restoring an older one.")); return;
	}
	reservationId = uuid(); auto next = root;
	if (!next.reserve(reservationId, revision)) { fail(); return; }
	writeRoot(next, [this] {
		const auto t = root.ticket(revision); step = Step::RestorePayload;
		files.downloadExact(provider, token, string(t, "name"), string(t, "object"), string(t, "version"));
	});
}
void CloudSyncHistory::writeRoot(const CloudHistoryRecord &next, const std::function<void()> &then)
{
	if (!next.valid() || !localUnchanged || !localUnchanged()) { fail(tr("Local data changed or history reached a safety limit. Your edits were kept. Reload history before retrying.")); return; }
	pendingRoot = next; continuation = then; step = Step::VerifyRoot; retriedRootRead = false;
	files.uploadConditional(provider, token, CloudHistoryRecord::rootName(epoch), next.json(), rootVersion, rootId);
}
void CloudSyncHistory::registerUpload()
{
	uploadId = uuid(); auto next = root;
	if (!CloudHistoryRecord::validXml(payload) || !next.registerUpload(uploadId, expectedHead, CloudSyncManifest::sha256(payload), payload.size(),
	    action == "restore" ? revision : QString(), reservationId)) { fail(); return; }
	writeRoot(next, [this] {
		if (!localUnchanged || !localUnchanged()) { fail(); return; }
		step = Step::Upload;
		files.uploadConditional(provider, token, CloudHistoryRecord::payloadName(epoch, uploadId), payload, {}, {});
	});
}
void CloudSyncHistory::publish()
{
	auto next = root;
	if (!next.publish(uploadId, expectedHead)) { fail(); return; }
	writeRoot(next, [this] {
		if (reservationId.isEmpty()) { complete(); return; }
		auto next = root;
		if (!next.release(reservationId)) { fail(); return; }
		writeRoot(next, [this] { complete(); });
	});
}
void CloudSyncHistory::complete()
{
	const auto report = root.valid() ? root.report() : QVariantMap{{"initialized", false}, {"versions", QVariantList()}, {"legacyChanged", false}};
	const QByteArray restored = action == "restore" ? payload : QByteArray();
	cancel(); emit finished(report, restored);
}
