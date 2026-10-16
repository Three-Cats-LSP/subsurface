// SPDX-License-Identifier: GPL-2.0
#include "cloudsyncreview.h"
#include "cloudsyncmanifest.h"
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRegularExpression>
#include <QUrlQuery>
#include <cmath>
#include <limits>

namespace {
const QString headName = QStringLiteral("subsurface-neo-sync.json");
const QString legacyName = QStringLiteral("subsurface-neo.xml");
const QRegularExpression recovered(QStringLiteral("^subsurface-neo-sync-recovered-[A-Za-z0-9_-]+\\.json$"));
const QRegularExpression snapshot(QStringLiteral("^subsurface-neo-[0-9a-f]{8}(?:-[0-9a-f]{4}){3}-[0-9a-f]{12}\\.xml$"));
const QRegularExpression hash(QStringLiteral("^[0-9a-f]{64}$"));
constexpr qint64 manifestLimit = 256 * 1024;

bool sizeValue(const QJsonValue &value, bool google, qint64 &bytes)
{
	if (google) {
		bool ok = false;
		bytes = value.toString().toLongLong(&ok);
		return ok && bytes >= 0;
	}
	const double number = value.toDouble(-1);
	if (!value.isDouble() || !std::isfinite(number) || number < 0 || number > 9007199254740991.0 || std::floor(number) != number)
		return false;
	bytes = static_cast<qint64>(number);
	return true;
}
}

bool CloudStorageEntry::operator==(const CloudStorageEntry &other) const
{
	return id == other.id && name == other.name && version == other.version && bytes == other.bytes && folder == other.folder;
}

bool CloudStorageReview::isManifest(const QString &name)
{
	return name == headName || recovered.match(name).hasMatch();
}

QVariantMap CloudStorageReview::classify(const QList<CloudStorageEntry> &entries,
		const QMap<QString, QByteArray> &manifests, QString *error)
{
	error->clear();
	auto invalid = [error](const QString &message) -> QVariantMap { *error = message; return {}; };
	QMap<QString, QList<CloudStorageEntry>> names;
	QSet<QString> ids;
	qint64 total = 0;
	int folders = 0;
	for (const auto &entry : entries) {
		if (entry.id.isEmpty() || entry.name.isEmpty() || entry.version.isEmpty() || entry.bytes < 0 ||
		    ids.contains(entry.id) || entry.bytes > std::numeric_limits<qint64>::max() - total)
			return invalid(tr("Cloud storage metadata is incomplete or ambiguous. Try reviewing again."));
		ids.insert(entry.id);
		names[entry.name].append(entry);
		total += entry.bytes;
		folders += entry.folder ? 1 : 0;
	}
	if (!entries.isEmpty() && names.value(headName).size() != 1)
		return invalid(tr("The current cloud manifest is missing or ambiguous. All files were kept."));
	QString current;
	QSet<QString> recoveryPayloads;
	for (const auto &entry : entries) {
		if (!isManifest(entry.name))
			continue;
		if (entry.folder || !manifests.contains(entry.id) || manifests.value(entry.id).size() > manifestLimit)
			return invalid(tr("A cloud manifest could not be inspected. All files were kept."));
		const auto manifest = CloudSyncManifest::fromJson(manifests.value(entry.id));
		if (!manifest.isValid() || !hash.match(manifest.payloadSha256).hasMatch())
			return invalid(tr("A cloud manifest is invalid or uses an unsupported format. All files were kept."));
		const auto targets = names.value(manifest.payloadFileName());
		if (targets.size() != 1 || targets.first().folder)
			return invalid(tr("A referenced cloud snapshot is missing or ambiguous. All files were kept."));
		if (entry.name == headName)
			current = manifest.payloadFileName();
		else
			recoveryPayloads.insert(manifest.payloadFileName());
	}
	const QStringList keys{QStringLiteral("current"), QStringLiteral("recovery"), QStringLiteral("legacy"),
		QStringLiteral("unknown"), QStringLiteral("other")};
	QMap<QString, int> counts;
	QMap<QString, qint64> sizes;
	for (const auto &entry : entries) {
		QString key;
		if (entry.name == headName || entry.name == current) key = keys[0];
		else if (isManifest(entry.name) || recoveryPayloads.contains(entry.name)) key = keys[1];
		else if (entry.name == legacyName) key = keys[2];
		else if (!entry.folder && snapshot.match(entry.name).hasMatch()) key = keys[3];
		else key = keys[4];
		++counts[key]; sizes[key] += entry.bytes;
	}
	QVariantList categories;
	for (const auto &key : keys)
		categories.append(QVariantMap{{QStringLiteral("id"), key}, {QStringLiteral("count"), counts.value(key)},
			{QStringLiteral("bytes"), sizes.value(key)}});
	return {{QStringLiteral("categories"), categories}, {QStringLiteral("entryCount"), entries.size()},
		{QStringLiteral("bytes"), total}, {QStringLiteral("folders"), folders}};
}

CloudStorageReview::CloudStorageReview(QNetworkAccessManager *network, QObject *parent) : QObject(parent), network(network)
{
	deadline.setSingleShot(true);
	connect(&deadline, &QTimer::timeout, this, [this] { fail(tr("Storage review timed out. Check your connection and try again.")); });
}

CloudStorageReview::~CloudStorageReview() { cancel(); }

void CloudStorageReview::cancel()
{
	deadline.stop();
	if (pending) {
		pending->disconnect(this);
		pending->abort();
		pending->deleteLater();
		pending = nullptr;
	}
	token.clear(); entries.clear(); original.clear(); manifests.clear(); manifestIds.clear(); cursors.clear();
}

void CloudStorageReview::fail(const QString &message, bool rejected)
{
	cancel();
	emit failed(message, rejected);
}

void CloudStorageReview::start(CloudSyncProviderType selected, const QString &accessToken)
{
	cancel();
	provider = selected; token = accessToken; pages = 0; verifying = false;
	if ((provider != CloudSyncProviderType::GoogleDrive && provider != CloudSyncProviderType::Dropbox) || token.isEmpty()) {
		fail(tr("Connect a cloud provider before reviewing its storage."));
		return;
	}
	deadline.start(180000);
	listPage();
}

void CloudStorageReview::request(const QUrl &url, const QByteArray &data, const QByteArray &argument,
		qint64 limit, const std::function<void(const QByteArray &, QNetworkReply *)> &done)
{
	QNetworkRequest request(url);
	request.setRawHeader("Authorization", "Bearer " + token.toUtf8());
	request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::ManualRedirectPolicy);
	request.setTransferTimeout(30000);
	if (!argument.isEmpty()) request.setRawHeader("Dropbox-API-Arg", argument);
	else request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
	auto *reply = provider == CloudSyncProviderType::GoogleDrive ? network->get(request) : network->post(request, data);
	pending = reply;
	QTimer::singleShot(35000, reply, [reply] { if (!reply->isFinished()) reply->abort(); });
	connect(reply, &QNetworkReply::readyRead, this, [reply, limit] { if (reply->bytesAvailable() > limit) reply->abort(); });
	connect(reply, &QNetworkReply::finished, this, [this, reply, limit, done] {
		if (pending != reply) return;
		pending = nullptr;
		reply->deleteLater();
		const auto body = reply->read(limit + 1);
		const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
		if (reply->error() != QNetworkReply::NoError || status != 200 || body.size() > limit) {
			fail(tr("Storage review is incomplete. Check your connection and try again. All cloud files were kept."), status == 401);
			return;
		}
		done(body, reply);
	});
}

void CloudStorageReview::listPage(const QString &cursor)
{
	if (++pages > 100 || (!cursor.isEmpty() && cursors.contains(cursor))) {
		fail(tr("Cloud storage listing is incomplete or too large to review. All files were kept.")); return;
	}
	if (!cursor.isEmpty()) cursors.insert(cursor);
	const bool google = provider == CloudSyncProviderType::GoogleDrive;
	QUrl url;
	QByteArray data;
	if (google) {
		url = QUrl(QStringLiteral("https://www.googleapis.com/drive/v3/files"));
		QUrlQuery query;
		query.addQueryItem(QStringLiteral("spaces"), QStringLiteral("appDataFolder"));
		query.addQueryItem(QStringLiteral("q"), QStringLiteral("trashed=false"));
		query.addQueryItem(QStringLiteral("pageSize"), QStringLiteral("1000"));
		query.addQueryItem(QStringLiteral("fields"), QStringLiteral("nextPageToken,incompleteSearch,files(id,name,mimeType,size,version)"));
		if (!cursor.isEmpty()) query.addQueryItem(QStringLiteral("pageToken"), cursor);
		url.setQuery(query);
	} else {
		url = QUrl(cursor.isEmpty() ? QStringLiteral("https://api.dropboxapi.com/2/files/list_folder")
			: QStringLiteral("https://api.dropboxapi.com/2/files/list_folder/continue"));
		data = QJsonDocument(cursor.isEmpty() ? QJsonObject{{"path", ""}, {"recursive", false}, {"include_deleted", false}, {"limit", 1000}}
			: QJsonObject{{"cursor", cursor}}).toJson(QJsonDocument::Compact);
	}
	request(url, data, {}, 2 * 1024 * 1024, [this, google](const QByteArray &body, QNetworkReply *) {
		const auto document = QJsonDocument::fromJson(body);
		const auto object = document.object();
		const auto files = object.value(google ? QStringLiteral("files") : QStringLiteral("entries"));
		const auto next = object.value(google ? QStringLiteral("nextPageToken") : QStringLiteral("cursor"));
		if (!document.isObject() || !files.isArray() ||
		    (google && ((object.contains("incompleteSearch") && !object.value("incompleteSearch").isBool()) || object.value("incompleteSearch").toBool() || (!next.isUndefined() && !next.isString()))) ||
		    (!google && (!object.value("has_more").isBool() || !next.isString() || next.toString().isEmpty()))) {
			fail(tr("Cloud storage listing is incomplete or invalid. All files were kept.")); return;
		}
		for (const auto &value : files.toArray()) {
			const auto item = value.toObject();
			CloudStorageEntry entry;
			entry.id = item.value("id").toString(); entry.name = item.value("name").toString();
			entry.folder = google ? item.value("mimeType").toString() == QStringLiteral("application/vnd.google-apps.folder")
				: item.value(".tag").toString() == QStringLiteral("folder");
			entry.version = google ? item.value("version").toString() : entry.folder ? entry.id : item.value("rev").toString();
			const bool validId = google ? QRegularExpression(QStringLiteral("^[A-Za-z0-9_-]+$")).match(entry.id).hasMatch()
				: entry.id.startsWith(QStringLiteral("id:")) && entry.id.size() > 3;
			if (!validId || entry.name.isEmpty() || entry.name.contains('/') || entry.name.contains('\\') || entry.version.isEmpty() ||
			    (google && item.value("mimeType").toString().isEmpty()) ||
			    (!google && !entry.folder && item.value(".tag").toString() != QStringLiteral("file")) ||
			    (!entry.folder && !sizeValue(item.value("size"), google, entry.bytes)) || entries.contains(entry.id) || entries.size() >= 20000) {
				fail(tr("Cloud storage metadata is incomplete or ambiguous. All files were kept.")); return;
			}
			entries.insert(entry.id, entry);
		}
		const bool more = google ? !next.toString().isEmpty() : object.value("has_more").toBool();
		if (more) { listPage(next.toString()); return; }
		if (verifying) {
			if (entries != original) { fail(tr("Cloud storage changed during review. Review again for current totals.")); return; }
			QString error;
			const auto report = classify(entries.values(), manifests, &error);
			if (!error.isEmpty()) { fail(error); return; }
			cancel();
			emit finished(report);
			return;
		}
		original = entries;
		for (const auto &entry : entries) {
			if (isManifest(entry.name)) {
				if (entry.folder || entry.bytes > manifestLimit || manifestIds.size() >= 128) {
					fail(tr("Cloud manifests exceed the review limit. All files were kept.")); return;
				}
				manifestIds.append(entry.id);
			}
		}
		readNextManifest();
	});
}

void CloudStorageReview::readNextManifest()
{
	if (manifestIds.isEmpty()) {
		verifying = true; entries.clear(); pages = 0; cursors.clear(); listPage(); return;
	}
	const auto entry = original.value(manifestIds.takeFirst());
	const bool google = provider == CloudSyncProviderType::GoogleDrive;
	const QUrl url(google ? QStringLiteral("https://www.googleapis.com/drive/v3/files/%1?alt=media").arg(entry.id)
		: QStringLiteral("https://content.dropboxapi.com/2/files/download"));
	const auto argument = google ? QByteArray() : QJsonDocument(QJsonObject{{"path", entry.id}}).toJson(QJsonDocument::Compact);
	request(url, {}, argument, manifestLimit, [this, entry, google](const QByteArray &body, QNetworkReply *reply) {
		if (!google) {
			const auto metadata = QJsonDocument::fromJson(reply->rawHeader("Dropbox-API-Result")).object();
			if (metadata.value("id").toString() != entry.id || metadata.value("rev").toString() != entry.version) {
				fail(tr("A cloud manifest changed during review. Review again for current totals.")); return;
			}
		}
		if (body.size() != entry.bytes) { fail(tr("A cloud manifest changed during review. Review again for current totals.")); return; }
		manifests.insert(entry.id, body);
		readNextManifest();
	});
}
