// SPDX-License-Identifier: GPL-2.0
#include "cloudsynchistory.h"
#include "cloudsyncmanifest.h"
#include <QJsonArray>
#include <QJsonDocument>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QLockFile>
#include <QRegularExpression>
#include <QRandomGenerator>
#include <QSet>
#include <QSaveFile>
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
QString archivedRootName(const QString &epoch, const QString &id) {
	return CloudHistoryRecord::rootName(epoch).chopped(5) + QStringLiteral("-recovered-%1.json").arg(id);
}
bool objectId(const QString &provider, const QString &id) {
	return QRegularExpression(provider == "google-drive" ? QStringLiteral("^[A-Za-z0-9_-]+$") :
		QStringLiteral("^id:[A-Za-z0-9_-]+$")).match(id).hasMatch() && id.size() <= 512;
}
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
bool CloudHistoryRecord::pristine() const
{
	return valid() && value.value("sequence").toInt() == 0 && head().isEmpty() &&
		value.value("tickets").toObject().isEmpty() && value.value("reservations").toObject().isEmpty();
}
bool CloudHistoryRecord::follows(const CloudHistoryRecord &previous) const
{
	if (!valid() || !previous.valid()) return false;
	for (const auto &key : {"schema", "provider", "account", "epoch"})
		if (value.value(key) != previous.value.value(key)) return false;
	const int sequence = value.value("sequence").toInt(), oldSequence = previous.value.value("sequence").toInt();
	if (sequence < oldSequence) return false;
	if (sequence == oldSequence) return json() == previous.json();
	const bool upgrade = string(previous.value, "format") == "neo-history-development-1" && string(value, "format") == "neo-history-development-2";
	if (value.value("format") != previous.value.value("format") && !upgrade) return false;
	if (string(value, "format") == "neo-history-development-1" && value.value("legacy_sha256") != previous.value.value("legacy_sha256")) return false;
	const auto history = value.value("history").toArray(), oldHistory = previous.value.value("history").toArray();
	if (history.size() < oldHistory.size()) return false;
	for (int i = 0; i < oldHistory.size(); ++i) if (history[i] != oldHistory[i]) return false;
	const auto oldTickets = previous.value.value("tickets").toObject();
	const QString initial = oldHistory.isEmpty() ? QString() : oldHistory.first().toString();
	const QStringList states = {"registered", "uploaded", "committed"};
	for (auto it = oldTickets.begin(); it != oldTickets.end(); ++it) {
		const auto before = it.value().toObject(), after = ticket(it.key());
		if (after.isEmpty()) return false;
		for (const auto &key : {"name", "parent", "sha256", "bytes", "restore_source", "reservation", "created_at"})
			if (before.value(key) != after.value(key)) return false;
		if (states.indexOf(string(after, "state")) < states.indexOf(string(before, "state"))) return false;
		if (string(before, "state") != "registered" &&
		    (before.value("object") != after.value("object") || before.value("version") != after.value("version"))) return false;
		if (upgrade) {
			if (string(after, "legacy_source") != (it.key() == initial ? string(previous.value, "legacy_sha256") : QString())) return false;
		} else if (before.value("legacy_source") != after.value("legacy_source")) return false;
	}
	const auto oldReservations = previous.value.value("reservations").toObject(), reservations = value.value("reservations").toObject();
	for (auto it = oldReservations.begin(); it != oldReservations.end(); ++it) {
		const auto before = it.value().toObject(), after = reservations.value(it.key()).toObject();
		if (after.isEmpty() || before.value("commit") != after.value("commit") ||
		    (!before.value("active").toBool() && after.value("active").toBool())) return false;
	}
	return true;
}
bool CloudHistoryRecord::valid() const
{
	const bool provenance = string(value, "format") == "neo-history-development-2";
	if (!fields(value, {"schema", "format", "provider", "account", "epoch", "legacy_sha256", "sequence", "head", "history", "tickets", "reservations"}) ||
	    value.value("schema") != QJsonValue(3) || (!provenance && string(value, "format") != "neo-history-development-1") ||
	    (string(value, "provider") != "google-drive" && string(value, "provider") != "dropbox") ||
	    !hash(string(value, "account")) || !validEpoch(string(value, "epoch")) || !hash(string(value, "legacy_sha256")) ||
	    !integer(value.value("sequence"), 0, 1000000) || !value.value("head").isString() ||
	    !value.value("history").isArray() || !value.value("tickets").isObject() || !value.value("reservations").isObject()) return false;
	const auto history = value.value("history").toArray();
	const auto tickets = value.value("tickets").toObject(), reservations = value.value("reservations").toObject();
	if (tickets.size() > EntryLimit || reservations.size() > EntryLimit || history.size() > tickets.size() || json().size() > RecordLimit) return false;
	QSet<QString> committed, objects;
	QString previous, acceptedLegacy;
	for (const auto &entry : history) {
		const QString id = entry.toString();
		const auto t = ticket(id);
		if (!validEpoch(id) || committed.contains(id) || string(t, "state") != "committed" || string(t, "parent") != previous) return false;
		const QString source = string(t, "restore_source");
		if (!source.isEmpty() && !committed.contains(source)) return false;
		const auto legacy = string(t, "legacy_source");
		if (provenance && previous.isEmpty() && legacy.isEmpty()) return false;
		if (!legacy.isEmpty()) acceptedLegacy = legacy;
		committed.insert(id); previous = id;
	}
	if (head() != previous) return false;
	if (provenance && acceptedLegacy != string(value, "legacy_sha256")) return false;
	for (auto it = tickets.begin(); it != tickets.end(); ++it) {
		const auto t = it.value().toObject();
		const QString state = string(t, "state"), parent = string(t, "parent"), source = string(t, "restore_source"), reservation = string(t, "reservation");
		QStringList ticketFields = {"name", "state", "parent", "sha256", "bytes", "object", "version", "restore_source", "reservation", "created_at"};
		if (provenance) ticketFields.append("legacy_source");
		if (!validEpoch(it.key()) || !fields(t, ticketFields) ||
		    string(t, "name") != payloadName(string(value, "epoch"), it.key()) || !hash(string(t, "sha256")) ||
		    !integer(t.value("bytes"), 1, PayloadLimit) || !QDateTime::fromString(string(t, "created_at"), Qt::ISODateWithMs).isValid() ||
		    (state != "registered" && state != "uploaded" && state != "committed") ||
		    (!parent.isEmpty() && (!committed.contains(parent) || parent == it.key())) ||
		    (state == "committed") != committed.contains(it.key())) return false;
		for (const auto &key : {"parent", "object", "version", "restore_source", "reservation"}) if (!t.value(QLatin1String(key)).isString()) return false;
		if (provenance && (!t.value("legacy_source").isString() || (!string(t, "legacy_source").isEmpty() &&
		    (!hash(string(t, "legacy_source")) || !source.isEmpty() || !reservation.isEmpty())))) return false;
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
bool CloudHistoryRecord::upgradeLegacyProvenance()
{
	if (!valid() || head().isEmpty()) return false;
	if (string(value, "format") == "neo-history-development-2") return true;
	auto next = value; next["format"] = "neo-history-development-2";
	auto tickets = value.value("tickets").toObject();
	const QString initial = value.value("history").toArray().first().toString();
	for (auto it = tickets.begin(); it != tickets.end(); ++it) {
		auto t = it.value().toObject();
		t["legacy_source"] = it.key() == initial ? string(value, "legacy_sha256") : QString();
		it.value() = t;
	}
	next["tickets"] = tickets; return replace(next);
}
bool CloudHistoryRecord::registerUpload(const QString &id, const QString &expectedHead, const QString &sha, qint64 bytes, const QString &source, const QString &reservation, const QString &legacySource)
{
	if (head() != expectedHead || !validEpoch(id)) return false;
	const bool provenance = string(value, "format") == "neo-history-development-2";
	if (!legacySource.isEmpty() && !provenance) return false;
	auto tickets = value.value("tickets").toObject();
	if (tickets.contains(id) || value.value("reservations").toObject().contains(id)) return false;
	tickets[id] = QJsonObject{{"name", payloadName(string(value, "epoch"), id)}, {"state", "registered"},
		{"parent", expectedHead}, {"sha256", sha}, {"bytes", bytes}, {"object", ""}, {"version", ""},
		{"restore_source", source}, {"reservation", reservation}, {"created_at", QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs)}};
	if (provenance) { auto t = tickets[id].toObject(); t["legacy_source"] = legacySource; tickets[id] = t; }
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
	auto next = value; next["tickets"] = tickets; next["history"] = history; next["head"] = id;
	if (!string(t, "legacy_source").isEmpty()) next["legacy_sha256"] = string(t, "legacy_source");
	return replace(next);
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
CloudHistoryAuthority::CloudHistoryAuthority(const QString &path) : fileName(path) {}
bool CloudHistoryAuthority::readUnlocked()
{
	rootId.clear(); checkpoint.value = {};
	const QFileInfo info(fileName);
	if (!info.exists()) return !info.isSymLink();
	if (!info.isFile() || info.isSymLink() || info.size() > RecordLimit + 1024) return false;
	QFile file(fileName);
	if (!file.open(QIODevice::ReadOnly)) return false;
	QJsonParseError error;
	const auto document = QJsonDocument::fromJson(file.read(RecordLimit + 1025), &error);
	const auto value = document.object();
	if (error.error != QJsonParseError::NoError || !document.isObject() || !fields(value, {"schema", "root_id", "root"}) ||
	    value.value("schema") != QJsonValue(1) || !text(string(value, "root_id")) || !value.value("root").isObject()) return false;
	checkpoint.value = value.value("root").toObject();
	if (!checkpoint.valid() || !objectId(string(checkpoint.value, "provider"), string(value, "root_id"))) { checkpoint.value = {}; return false; }
	rootId = string(value, "root_id"); return true;
}
bool CloudHistoryAuthority::load()
{
	if (fileName.isEmpty()) return false;
	if (!QFileInfo(QFileInfo(fileName).absolutePath()).exists()) { rootId.clear(); checkpoint.value = {}; return true; }
	QLockFile lock(fileName + ".lock");
	return lock.tryLock(0) && readUnlocked();
}
bool CloudHistoryAuthority::writeUnlocked(const QString &id, const CloudHistoryRecord &root)
{
	const auto bytes = QJsonDocument(QJsonObject{{"schema", 1}, {"root_id", id}, {"root", root.value}}).toJson(QJsonDocument::Compact);
	QSaveFile file(fileName);
	if (!file.open(QIODevice::WriteOnly) || file.write(bytes) != bytes.size() || !file.commit()) return false;
	return readUnlocked() && rootId == id && checkpoint.json() == root.json();
}
bool CloudHistoryAuthority::observe(const QString &id, const CloudHistoryRecord &root)
{
	if (!root.valid() || !objectId(string(root.value, "provider"), id) || fileName.isEmpty() || !QDir().mkpath(QFileInfo(fileName).absolutePath())) return false;
	QLockFile lock(fileName + ".lock");
	if (!lock.tryLock(0) || !readUnlocked()) return false;
	if (!rootId.isEmpty() && (rootId != id || !root.follows(checkpoint))) return false;
	if (rootId == id && checkpoint.json() == root.json()) return true;
	return writeUnlocked(id, root);
}
bool CloudHistoryAuthority::rebindPristine(const QString &oldId, const CloudHistoryRecord &archived,
	const QString &newId, const CloudHistoryRecord &root)
{
	if (!root.valid() || !objectId(string(root.value, "provider"), newId) || newId == oldId || fileName.isEmpty()) return false;
	QLockFile lock(fileName + ".lock");
	if (!lock.tryLock(0) || !readUnlocked() || rootId != oldId || !checkpoint.pristine() ||
	    archived.json() != checkpoint.json() || !root.follows(checkpoint)) return false;
	return writeUnlocked(newId, root);
}
QVariantMap CloudHistoryRecord::report() const
{
	QVariantList versions;
	const auto history = value.value("history").toArray();
	for (auto it = history.constEnd(); it != history.constBegin();) {
		const QString id = (*--it).toString(); const auto t = ticket(id);
		versions.append(QVariantMap{{"id", id}, {"current", id == head()}, {"bytes", qint64(t.value("bytes").toDouble())},
			{"createdAt", string(t, "created_at")}, {"restored", !string(t, "restore_source").isEmpty()},
			{"legacyImported", !string(t, "legacy_source").isEmpty()}});
	}
	return {{"initialized", !head().isEmpty()}, {"versions", versions}, {"legacyChanged", false}};
}

CloudSyncHistory::CloudSyncHistory(QNetworkAccessManager *network, QObject *parent) : QObject(parent), files(network, this)
{
	files.setStrictReads(true);
	files.setResponseLimit(PayloadLimit);
	deadline.setSingleShot(true);
	initializationRetry.setSingleShot(true);
	connect(&initializationRetry, &QTimer::timeout, this, [this] {
		if (step != Step::RetryInitialization) return;
		if (!localUnchanged || !localUnchanged()) { fail(); return; }
		read(CloudHistoryRecord::rootName(epoch), Step::Root);
	});
	connect(&deadline, &QTimer::timeout, this, [this] { fail(tr("History operation timed out. Reload history before retrying; all files were kept.")); });
	connect(&files, &CloudSyncFileStore::versionedDownloadFinished, this,
		[this](CloudSyncProviderType, const QString &name, const QByteArray &data, const QString &version, const QString &id) { received(name, data, version, id); });
	connect(&files, &CloudSyncFileStore::historyRootsFound, this, [this](const QStringList &ids) {
		if (step == Step::VerifyReviewList) {
			if (ids != reviewIds) { fail(tr("History branches changed during review. All files were kept; review again.")); return; }
			completeRootReview();
		} else if (step == Step::ReviewList) {
			reviewIds = ids; reviewIndex = 0; step = Step::ReviewRoot; reviewNextRoot();
		}
	});
	connect(&files, &CloudSyncFileStore::uploadFinished, this, [this](CloudSyncProviderType, const QString &name) {
		if (step == Step::VerifyRoot) read(name, Step::VerifyRoot);
		else if (step == Step::Upload) read(name, Step::VerifyPayload);
	});
	connect(&files, &CloudSyncFileStore::fileNotFound, this, [this](CloudSyncProviderType, const QString &name) {
		if (step == Step::ReviewRoot && provider == CloudSyncProviderType::Dropbox && reviewedRoots.isEmpty()) {
			reviewIds.clear(); completeRootReview(); return;
		}
		if (step != Step::Root || name != CloudHistoryRecord::rootName(epoch)) { fail(); return; }
		if (authority && (!authority->load() || !authority->id().isEmpty())) {
			fail(tr("The known history root is missing or its local checkpoint cannot be read. No replacement was created. Restore the known root or use a new history namespace.")); return;
		}
		if (action == "list") { complete(); return; }
		if (action != "migrate") { fail(tr("Initialize history from the cloud log first.")); return; }
		root = CloudHistoryRecord::create(cloudSyncProviderDescriptor(provider).id, account, epoch, legacyHash);
		writeRoot(root, [this] { processRoot(); });
	});
	connect(&files, &CloudSyncFileStore::writeConflict, this, [this](CloudSyncProviderType, const QString &name) {
		// Initialization is idempotent. Join the winning root, or allocate a
		// fresh ticket after a conflict; never replay a stale publication.
		if (name == CloudHistoryRecord::rootName(epoch) && retryInitialization()) return;
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
	step = Step::Idle; deadline.stop(); initializationRetry.stop(); files.cancel(provider); continuation = {}; localUnchanged = {};
	token.clear(); local.clear(); payload.clear(); pendingRoot.value = {};
	authority.reset(); aliasStep = Step::Idle;
	reviewedRoots.clear(); reviewIds.clear(); reviewIndex = 0;
	files.setHistoryRootRecovery({}, {});
}
void CloudSyncHistory::fail(const QString &message, bool rejected)
{
	if (step == Step::Idle) return;
	cancel();
	emit failed(message.isEmpty() ? tr("Cloud history could not be confirmed. Reload history before retrying. Local data and cloud files were kept.") : message, rejected);
}
void CloudSyncHistory::start(CloudSyncProviderType type, const QString &accessToken, const QString &accountHash,
	const QString &namespaceEpoch, const QString &operation, const QString &commit, const QByteArray &localPayload, const std::function<bool()> &guard,
	const QString &authorityFile)
{
	cancel(); provider = type; token = accessToken; account = accountHash; epoch = namespaceEpoch; action = operation; revision = commit;
	local = localPayload; localUnchanged = guard; root.value = {}; pendingRoot.value = {}; rootId.clear(); rootVersion.clear(); reservationId.clear(); uploadId.clear();
	initializationRetries = 0;
	step = Step::Legacy;
	if ((type != CloudSyncProviderType::GoogleDrive && type != CloudSyncProviderType::Dropbox) || token.isEmpty() || !hash(account) ||
	    !CloudHistoryRecord::validEpoch(epoch) || !QStringList{"list", "migrate", "save", "restore", "import-legacy", "review-roots"}.contains(action) ||
	    !CloudHistoryRecord::validXml(local) || (action == "restore" && !CloudHistoryRecord::validEpoch(revision))) { fail(); return; }
	if (!authorityFile.isEmpty()) {
		authority = std::make_unique<CloudHistoryAuthority>(authorityFile);
		if (!authority->load() || (!authority->id().isEmpty() &&
		    (string(authority->record().value, "provider") != cloudSyncProviderDescriptor(provider).id ||
		     string(authority->record().value, "account") != account || string(authority->record().value, "epoch") != epoch))) {
			fail(tr("The saved history authority cannot be confirmed. Its checkpoint was kept; history access is paused.")); return;
		}
	}
	if (action == "review-roots") {
		deadline.start(180000);
		if (provider == CloudSyncProviderType::GoogleDrive) {
			step = Step::ReviewList; files.findHistoryRoots(token, CloudHistoryRecord::rootName(epoch));
		} else {
			reviewIds = {QString()}; read(CloudHistoryRecord::rootName(epoch), Step::ReviewRoot);
		}
		return;
	}
	if (action != "list") {
		files.setHistoryRootRecovery(CloudHistoryRecord::rootName(epoch), [this](const QString &id, const QByteArray &data) {
			using State = CloudSyncFileStore::HistoryRootState;
			const auto candidate = CloudHistoryRecord::parse(data);
			if (!candidate.valid() || string(candidate.value, "provider") != cloudSyncProviderDescriptor(provider).id ||
			    string(candidate.value, "account") != account || string(candidate.value, "epoch") != epoch) return State::Invalid;
			if (authority) {
				if (!authority->load()) return State::Invalid;
				const auto known = authority->record();
				if (!authority->id().isEmpty() && (string(known.value, "provider") != cloudSyncProviderDescriptor(provider).id ||
				    string(known.value, "account") != account || string(known.value, "epoch") != epoch ||
				    (id == authority->id() && !candidate.follows(known)))) return State::Invalid;
			}
			return candidate.pristine() ? State::Pristine : State::Active;
		}, authority && !authority->record().pristine() ? authority->id() : QString());
	}
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
	if (step == Step::ReviewRoot || step == Step::VerifyReviewRoot) {
		const auto record = CloudHistoryRecord::parse(data);
		if (!record.valid() || string(record.value, "provider") != cloudSyncProviderDescriptor(provider).id ||
		    string(record.value, "account") != account || string(record.value, "epoch") != epoch || version.isEmpty() ||
		    !objectId(cloudSyncProviderDescriptor(provider).id, id) || reviewIndex >= reviewIds.size()) { fail(); return; }
		if (provider == CloudSyncProviderType::Dropbox && step == Step::ReviewRoot) reviewIds[reviewIndex] = id;
		if (id != reviewIds[reviewIndex]) { fail(); return; }
		if (step == Step::ReviewRoot) reviewedRoots.append({id, version, record});
		else if (id != reviewedRoots[reviewIndex].id || version != reviewedRoots[reviewIndex].version ||
		         record.json() != reviewedRoots[reviewIndex].record.json()) { fail(); return; }
		++reviewIndex; reviewNextRoot(); return;
	}
	if (step == Step::VerifyPristineAlias) {
		const auto archived = CloudHistoryRecord::parse(data);
		if (!authority || id != authority->id() || !authority->rebindPristine(id, archived, rootId, root)) {
			fail(tr("The history root identity changed and its pristine recovery alias could not be confirmed. All files were kept.")); return;
		}
		step = aliasStep; aliasStep = Step::Idle;
		received(CloudHistoryRecord::rootName(epoch), root.json(), rootVersion, rootId); return;
	}
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
		if (authority && !authority->id().isEmpty() && authority->id() != id) {
			if (provider != CloudSyncProviderType::GoogleDrive || !authority->record().pristine()) {
				fail(tr("The history root was replaced. The known authority and every cloud version were kept; history access is paused.")); return;
			}
			const auto oldId = authority->id();
			root = parsed; rootVersion = version; rootId = id; aliasStep = step; step = Step::VerifyPristineAlias;
			files.downloadIdentity(provider, token, archivedRootName(epoch, oldId), oldId); return;
		}
		if (authority && !authority->observe(id, parsed)) {
			fail(tr("History differs from its saved authority or its checkpoint could not be saved. History access is paused; all versions were kept.")); return;
		}
		if (!rootId.isEmpty() && rootId != id) { fail(); return; }
		root = parsed; rootVersion = version; rootId = id;
		if (step == Step::VerifyRoot) {
			// Another initializer may already have advanced the selected root.
			// Only the initial empty create can be adopted this way.
			if (action == "migrate" && pendingRoot.valid() && pendingRoot.value.value("sequence").toInt() == 0 &&
			    pendingRoot.head().isEmpty() && pendingRoot.value.value("tickets").toObject().isEmpty() &&
			    string(root.value, "legacy_sha256") == legacyHash) {
				pendingRoot.value = {}; continuation = {}; processRoot(); return;
			}
			if (root.json() != pendingRoot.json()) { if (!retryInitialization()) fail(); return; }
			pendingRoot.value = {}; auto next = std::move(continuation); continuation = {}; next(); return;
		}
		if (step == Step::VerifyReservation) {
			const auto r = root.value.value("reservations").toObject().value(reservationId).toObject();
			if (root.head() != expectedHead || !r.value("active").toBool() || string(r, "commit") != revision) { fail(); return; }
			registerUpload(); return;
		}
		if (string(root.value, "legacy_sha256") != legacyHash) {
			if (action == "import-legacy" && !root.head().isEmpty()) { processRoot(); return; }
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
void CloudSyncHistory::reviewNextRoot()
{
	if (reviewIndex >= reviewIds.size()) {
		if (step == Step::ReviewRoot && !reviewIds.isEmpty()) { reviewIndex = 0; step = Step::VerifyReviewRoot; }
		else {
			if (provider == CloudSyncProviderType::GoogleDrive) {
				step = Step::VerifyReviewList; files.findHistoryRoots(token, CloudHistoryRecord::rootName(epoch));
			} else completeRootReview();
			return;
		}
	}
	if (step == Step::ReviewRoot)
		files.downloadIdentity(provider, token, CloudHistoryRecord::rootName(epoch), reviewIds[reviewIndex]);
	else {
		const auto &entry = reviewedRoots[reviewIndex];
		files.downloadExact(provider, token, CloudHistoryRecord::rootName(epoch), entry.id, entry.version);
	}
}
void CloudSyncHistory::completeRootReview()
{
	if (!localUnchanged || !localUnchanged() || (authority && !authority->load())) { fail(); return; }
	const auto knownId = authority ? authority->id() : QString();
	QVariantList branches; bool conflict = false; int active = 0;
	for (const auto &entry : reviewedRoots) {
		const auto &record = entry.record; int pending = 0, reservations = 0;
		const auto tickets = record.value.value("tickets").toObject();
		for (auto it = tickets.begin(); it != tickets.end(); ++it) if (string(it.value().toObject(), "state") != "committed") ++pending;
		const auto readers = record.value.value("reservations").toObject();
		for (auto it = readers.begin(); it != readers.end(); ++it) if (it.value().toObject().value("active").toBool()) ++reservations;
		const bool known = !knownId.isEmpty() && knownId == entry.id;
		const bool matches = known && record.follows(authority->record());
		conflict |= known && !matches; if (!record.pristine()) ++active;
		branches.append(QVariantMap{{"versions", record.value.value("history").toArray().size()}, {"pendingUploads", pending},
			{"activeReservations", reservations}, {"pristine", record.pristine()}, {"known", known}, {"matchesCheckpoint", matches}});
	}
	const QVariantMap report{{"initialized", false}, {"versions", QVariantList()}, {"legacyChanged", false},
		{"branchesReviewed", true}, {"reviewedAt", QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs)},
		{"branches", branches}, {"activeBranches", active}, {"hasCheckpoint", !knownId.isEmpty()},
		{"knownRootMissing", !knownId.isEmpty() && !reviewIds.contains(knownId)}, {"checkpointConflict", conflict}};
	cancel(); emit finished(report, {});
}
bool CloudSyncHistory::retryInitialization()
{
	if (step == Step::Idle || action != "migrate" || initializationRetries++ >= 8 || !localUnchanged || !localUnchanged()) return false;
	files.cancel(provider); pendingRoot.value = {}; continuation = {}; uploadId.clear(); payload.clear();
	// Give the winning initializer time to finish. Immediate competing
	// registrations can repeatedly invalidate each other's upload completion.
	step = Step::RetryInitialization;
	initializationRetry.start(qMin(8000, 250 << (initializationRetries - 1)) + QRandomGenerator::global()->bounded(250));
	return true;
}
void CloudSyncHistory::processRoot()
{
	if (action == "list" || (action == "migrate" && !root.head().isEmpty())) { complete(); return; }
	if (action == "import-legacy" && string(root.value, "legacy_sha256") == legacyHash) { complete(); return; }
	expectedHead = root.head();
	if (action == "migrate") { read(legacyPayload, Step::MigrationPayload); return; }
	if (expectedHead.isEmpty()) { fail(tr("Finish initializing history before saving or restoring a version.")); return; }
	if (action == "import-legacy") { read(legacyPayload, Step::MigrationPayload); return; }
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
	if (authority && (rootId.isEmpty() ? (!authority->load() || !authority->id().isEmpty()) : !authority->observe(rootId, root))) {
		fail(tr("The saved history authority changed or could not be confirmed. Reload history before retrying; no new version was published.")); return;
	}
	pendingRoot = next; continuation = then; step = Step::VerifyRoot; retriedRootRead = false;
	files.uploadConditional(provider, token, CloudHistoryRecord::rootName(epoch), next.json(), rootVersion, rootId);
}
void CloudSyncHistory::registerUpload()
{
	uploadId = uuid(); auto next = root;
	if (action == "import-legacy" && !next.upgradeLegacyProvenance()) { fail(); return; }
	if (!CloudHistoryRecord::validXml(payload) || !next.registerUpload(uploadId, expectedHead, CloudSyncManifest::sha256(payload), payload.size(),
	    action == "restore" ? revision : QString(), reservationId, action == "import-legacy" ? legacyHash : QString())) { fail(); return; }
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
