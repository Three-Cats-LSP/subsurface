// SPDX-License-Identifier: GPL-2.0
#include "cloudsyncfilestore.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrlQuery>

namespace {

QByteArray compactJson(const QJsonObject &object)
{
	return QJsonDocument(object).toJson(QJsonDocument::Compact);
}

} // namespace

CloudSyncFileStore::CloudSyncFileStore(QNetworkAccessManager *networkManager, QObject *parent) :
	QObject(parent), networkManager(networkManager)
{
	Q_ASSERT(networkManager);
	qRegisterMetaType<CloudSyncProviderType>();
}

void CloudSyncFileStore::trackReply(CloudSyncProviderType provider, QNetworkReply *reply)
{
	pendingReplies[provider].insert(reply);
	connect(reply, &QNetworkReply::finished, this, [this, provider, reply]() {
		pendingReplies[provider].remove(reply);
	});
}

void CloudSyncFileStore::cancel(CloudSyncProviderType provider)
{
	const auto replies = pendingReplies.take(provider);
	for (QNetworkReply *reply : replies) {
		reply->disconnect(this);
		reply->abort();
		reply->deleteLater();
	}
}

void CloudSyncFileStore::upload(CloudSyncProviderType provider, const QString &accessToken,
				const QString &fileName, const QByteArray &data)
{
	const QString remoteName = safeRemoteName(fileName);
	if (remoteName.isEmpty() || accessToken.isEmpty()) {
		emit operationError(provider, fileName, tr("Cloud upload is missing a file name or access token."));
		return;
	}

	switch (provider) {
	case CloudSyncProviderType::GoogleDrive:
		googleUpload(accessToken, remoteName, data);
		break;
	case CloudSyncProviderType::Dropbox:
		dropboxUpload(accessToken, remoteName, data);
		break;
	case CloudSyncProviderType::SubsurfaceCloud:
		emit operationError(provider, remoteName, tr("Legacy Subsurface Cloud uses its existing sync backend."));
		break;
	}
}

void CloudSyncFileStore::download(CloudSyncProviderType provider, const QString &accessToken,
				  const QString &fileName)
{
	const QString remoteName = safeRemoteName(fileName);
	if (remoteName.isEmpty() || accessToken.isEmpty()) {
		emit operationError(provider, fileName, tr("Cloud download is missing a file name or access token."));
		return;
	}

	switch (provider) {
	case CloudSyncProviderType::GoogleDrive:
		googleDownload(accessToken, remoteName);
		break;
	case CloudSyncProviderType::Dropbox:
		dropboxDownload(accessToken, remoteName);
		break;
	case CloudSyncProviderType::SubsurfaceCloud:
		emit operationError(provider, remoteName, tr("Legacy Subsurface Cloud uses its existing sync backend."));
		break;
	}
}

QNetworkRequest CloudSyncFileStore::authorizedRequest(const QUrl &url, const QString &accessToken) const
{
	QNetworkRequest request(url);
	request.setTransferTimeout(30000);
	request.setRawHeader("Authorization", QByteArray("Bearer ") + accessToken.toUtf8());
	request.setRawHeader("Accept", "application/json");
	return request;
}

QString CloudSyncFileStore::safeRemoteName(const QString &fileName)
{
	QString name = fileName.trimmed();
	name.replace(QLatin1Char('\\'), QLatin1Char('/'));
	if (name.contains(QLatin1Char('/')) || name == QStringLiteral(".") || name == QStringLiteral(".."))
		return QString();
	return name;
}

QString CloudSyncFileStore::replyError(QNetworkReply *reply, const QByteArray &payload) const
{
	QJsonParseError error;
	const QJsonDocument document = QJsonDocument::fromJson(payload, &error);
	if (error.error == QJsonParseError::NoError && document.isObject()) {
		const QJsonObject object = document.object();
		QString message = object.value(QStringLiteral("error_summary")).toString();
		if (message.isEmpty()) {
			const QJsonValue errorValue = object.value(QStringLiteral("error"));
			if (errorValue.isString())
				message = errorValue.toString();
			else if (errorValue.isObject())
				message = errorValue.toObject().value(QStringLiteral("message")).toString();
		}
		if (!message.isEmpty())
			return message;
	}
	return reply->errorString();
}

void CloudSyncFileStore::googleFindFile(const QString &accessToken, const QString &fileName,
					const std::function<void(const QString &)> &continuation)
{
	QUrl url(QStringLiteral("https://www.googleapis.com/drive/v3/files"));
	QUrlQuery query;
	QString escapedName = fileName;
	escapedName.replace(QLatin1Char('\\'), QStringLiteral("\\\\"));
	escapedName.replace(QLatin1Char('\''), QStringLiteral("\\'"));
	query.addQueryItem(QStringLiteral("spaces"), QStringLiteral("appDataFolder"));
	query.addQueryItem(QStringLiteral("q"), QStringLiteral("name='%1' and trashed=false").arg(escapedName));
	query.addQueryItem(QStringLiteral("fields"), QStringLiteral("nextPageToken,files(id,name)"));
	url.setQuery(query);

	QNetworkReply *reply = networkManager->get(authorizedRequest(url, accessToken));
	trackReply(CloudSyncProviderType::GoogleDrive, reply);
	connect(reply, &QNetworkReply::finished, this, [this, reply, fileName, continuation]() {
		const QByteArray payload = reply->readAll();
		const bool ok = reply->error() == QNetworkReply::NoError;
		const QString error = ok ? QString() : replyError(reply, payload);
		reply->deleteLater();
		if (!ok) {
			emit operationError(CloudSyncProviderType::GoogleDrive, fileName, error,
				reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() == 401);
			return;
		}
		QJsonParseError parseError;
		const QJsonDocument document = QJsonDocument::fromJson(payload, &parseError);
		const QJsonObject object = document.object();
		if (parseError.error != QJsonParseError::NoError || !document.isObject() ||
		    !object.value(QStringLiteral("files")).isArray()) {
			emit operationError(CloudSyncProviderType::GoogleDrive, fileName,
				tr("Google Drive returned an invalid file-list response."));
			return;
		}
		const QJsonArray files = object.value(QStringLiteral("files")).toArray();
		if (files.size() > 1 || !object.value(QStringLiteral("nextPageToken")).toString().isEmpty() ||
		    (!files.isEmpty() && files.first().toObject().value(QStringLiteral("id")).toString().isEmpty())) {
			emit operationError(CloudSyncProviderType::GoogleDrive, fileName,
				tr("Google Drive returned an ambiguous or invalid cloud file. No data was changed."));
			return;
		}
		continuation(files.isEmpty() ? QString() : files.first().toObject().value(QStringLiteral("id")).toString());
	});
}

void CloudSyncFileStore::googleUpload(const QString &accessToken, const QString &fileName, const QByteArray &data, bool createOnly)
{
	googleFindFile(accessToken, fileName, [this, accessToken, fileName, data, createOnly](const QString &fileId) {
		if (createOnly && !fileId.isEmpty()) {
			emit writeConflict(CloudSyncProviderType::GoogleDrive, fileName);
			return;
		}
		QNetworkRequest request;
		QNetworkReply *reply = nullptr;
		if (!fileId.isEmpty()) {
			QUrl url(QStringLiteral("https://www.googleapis.com/upload/drive/v3/files/%1").arg(fileId));
			QUrlQuery query;
			query.addQueryItem(QStringLiteral("uploadType"), QStringLiteral("media"));
			url.setQuery(query);
			request = authorizedRequest(url, accessToken);
			request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/octet-stream"));
			reply = networkManager->sendCustomRequest(request, "PATCH", data);
		} else {
			const QByteArray boundary("subsurface_neo_boundary");
			const QByteArray metadata = compactJson({
				{QStringLiteral("name"), fileName},
				{QStringLiteral("parents"), QJsonArray{QStringLiteral("appDataFolder")}},
			});
			QByteArray body;
			body += "--" + boundary + "\r\nContent-Type: application/json; charset=UTF-8\r\n\r\n";
			body += metadata + "\r\n--" + boundary + "\r\nContent-Type: application/octet-stream\r\n\r\n";
			body += data + "\r\n--" + boundary + "--\r\n";
			QUrl url(QStringLiteral("https://www.googleapis.com/upload/drive/v3/files"));
			QUrlQuery query;
			query.addQueryItem(QStringLiteral("uploadType"), QStringLiteral("multipart"));
			url.setQuery(query);
			request = authorizedRequest(url, accessToken);
			request.setHeader(QNetworkRequest::ContentTypeHeader,
					  QStringLiteral("multipart/related; boundary=%1").arg(QString::fromLatin1(boundary)));
			reply = networkManager->post(request, body);
		}

		trackReply(CloudSyncProviderType::GoogleDrive, reply);
		connect(reply, &QNetworkReply::finished, this, [this, reply, fileName]() {
			const QByteArray payload = reply->readAll();
			const bool ok = reply->error() == QNetworkReply::NoError;
			const QString error = ok ? QString() : replyError(reply, payload);
			reply->deleteLater();
			if (ok)
				emit uploadFinished(CloudSyncProviderType::GoogleDrive, fileName);
			else
				emit operationError(CloudSyncProviderType::GoogleDrive, fileName, error,
					reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() == 401);
		});
	});
}

void CloudSyncFileStore::googleDownload(const QString &accessToken, const QString &fileName)
{
	googleFindFile(accessToken, fileName, [this, accessToken, fileName](const QString &fileId) {
		if (fileId.isEmpty()) {
			emit fileNotFound(CloudSyncProviderType::GoogleDrive, fileName);
			return;
		}
		QUrl url(QStringLiteral("https://www.googleapis.com/drive/v3/files/%1").arg(fileId));
		QUrlQuery query;
		query.addQueryItem(QStringLiteral("alt"), QStringLiteral("media"));
		url.setQuery(query);
		QNetworkReply *reply = networkManager->get(authorizedRequest(url, accessToken));
		trackReply(CloudSyncProviderType::GoogleDrive, reply);
		connect(reply, &QNetworkReply::finished, this, [this, reply, fileName]() {
			const QByteArray payload = reply->readAll();
			const bool ok = reply->error() == QNetworkReply::NoError;
			const QString error = ok ? QString() : replyError(reply, payload);
			reply->deleteLater();
			if (ok)
				emit downloadFinished(CloudSyncProviderType::GoogleDrive, fileName, payload);
			else
				emit operationError(CloudSyncProviderType::GoogleDrive, fileName, error,
					reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() == 401);
		});
	});
}

void CloudSyncFileStore::dropboxUpload(const QString &accessToken, const QString &fileName, const QByteArray &data,
				     bool conditional, const QString &version)
{
	QNetworkRequest request = authorizedRequest(QUrl(QStringLiteral("https://content.dropboxapi.com/2/files/upload")), accessToken);
	request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/octet-stream"));
	request.setRawHeader("Dropbox-API-Arg", compactJson({
		{QStringLiteral("path"), QStringLiteral("/") + fileName},
		{QStringLiteral("mode"), !conditional ? QJsonValue(QStringLiteral("overwrite")) : version.isEmpty()
			? QJsonValue(QStringLiteral("add")) : QJsonValue(QJsonObject{{QStringLiteral(".tag"), QStringLiteral("update")},
				{QStringLiteral("update"), version}})},
		{QStringLiteral("strict_conflict"), conditional},
		{QStringLiteral("autorename"), false},
		{QStringLiteral("mute"), true},
	}));
	QNetworkReply *reply = networkManager->post(request, data);
	trackReply(CloudSyncProviderType::Dropbox, reply);
	connect(reply, &QNetworkReply::finished, this, [this, reply, fileName, conditional]() {
		const QByteArray payload = reply->readAll();
		const bool ok = reply->error() == QNetworkReply::NoError;
		const QString error = ok ? QString() : replyError(reply, payload);
		reply->deleteLater();
		if (ok)
			emit uploadFinished(CloudSyncProviderType::Dropbox, fileName);
		else if (conditional && reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() == 409 &&
			 QJsonDocument::fromJson(payload).object().value(QStringLiteral("error_summary")).toString().startsWith(QStringLiteral("path/conflict/")))
			emit writeConflict(CloudSyncProviderType::Dropbox, fileName);
		else
			emit operationError(CloudSyncProviderType::Dropbox, fileName, error,
				reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() == 401);
	});
}

void CloudSyncFileStore::dropboxDownload(const QString &accessToken, const QString &fileName, bool versioned)
{
	QNetworkRequest request = authorizedRequest(QUrl(QStringLiteral("https://content.dropboxapi.com/2/files/download")), accessToken);
	request.setRawHeader("Dropbox-API-Arg", compactJson({{QStringLiteral("path"), QStringLiteral("/") + fileName}}));
	QNetworkReply *reply = networkManager->post(request, QByteArray());
	trackReply(CloudSyncProviderType::Dropbox, reply);
	connect(reply, &QNetworkReply::finished, this, [this, reply, fileName, versioned]() {
		const QByteArray payload = reply->readAll();
		const bool ok = reply->error() == QNetworkReply::NoError;
		const QString error = ok ? QString() : replyError(reply, payload);
		const QJsonObject providerError = QJsonDocument::fromJson(payload).object().value(QStringLiteral("error")).toObject();
		const bool missing = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() == 409 &&
			providerError.value(QStringLiteral(".tag")).toString() == QStringLiteral("path") &&
			providerError.value(QStringLiteral("path")).toObject().value(QStringLiteral(".tag")).toString() == QStringLiteral("not_found");
		reply->deleteLater();
		if (ok && versioned) {
			const QString version = QJsonDocument::fromJson(reply->rawHeader("Dropbox-API-Result")).object().value(QStringLiteral("rev")).toString();
			if (version.isEmpty())
				emit operationError(CloudSyncProviderType::Dropbox, fileName, tr("Dropbox did not return a file revision. No data was changed."));
			else
				emit versionedDownloadFinished(CloudSyncProviderType::Dropbox, fileName, payload, version, QString());
		} else if (ok)
			emit downloadFinished(CloudSyncProviderType::Dropbox, fileName, payload);
		else if (missing)
			emit fileNotFound(CloudSyncProviderType::Dropbox, fileName);
		else
			emit operationError(CloudSyncProviderType::Dropbox, fileName, error,
				reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() == 401);
	});
}


void CloudSyncFileStore::uploadConditional(CloudSyncProviderType provider, const QString &accessToken,
		const QString &fileName, const QByteArray &data, const QString &version, const QString &fileId)
{
	if (safeRemoteName(fileName).isEmpty() || accessToken.isEmpty()) {
		emit operationError(provider, fileName, tr("Cloud upload is missing a file name or access token."));
		return;
	}
	if (provider == CloudSyncProviderType::Dropbox) {
		dropboxUpload(accessToken, fileName, data, true, version);
	} else if (provider == CloudSyncProviderType::GoogleDrive && version.isEmpty() && fileId.isEmpty()) {
		// Drive permits duplicate names. A first-create race is detected by the
		// manager's post-commit lookup; both immutable payloads are retained.
		googleUpload(accessToken, fileName, data, true);
	} else if (provider == CloudSyncProviderType::GoogleDrive && !version.isEmpty() && !fileId.isEmpty()) {
		// Drive v3 ignores If-Match for media updates. v2 enforces the File etag.
		QUrl url(QStringLiteral("https://www.googleapis.com/upload/drive/v2/files/%1?uploadType=media").arg(fileId));
		QNetworkRequest request = authorizedRequest(url, accessToken);
		request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/octet-stream"));
		request.setRawHeader("If-Match", version.toUtf8());
		QNetworkReply *reply = networkManager->put(request, data);
		trackReply(provider, reply);
		connect(reply, &QNetworkReply::finished, this, [this, reply, provider, fileName]() {
			const QByteArray payload = reply->readAll();
			const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
			reply->deleteLater();
			if (reply->error() == QNetworkReply::NoError)
				emit uploadFinished(provider, fileName);
			else if (status == 412 || status == 404)
				emit writeConflict(provider, fileName);
			else
				emit operationError(provider, fileName, replyError(reply, payload), status == 401);
		});
	} else {
		emit operationError(provider, fileName, tr("Cloud update is missing its expected file revision."));
	}
}

void CloudSyncFileStore::downloadVersioned(CloudSyncProviderType provider, const QString &accessToken, const QString &fileName)
{
	if (safeRemoteName(fileName).isEmpty() || accessToken.isEmpty()) {
		emit operationError(provider, fileName, tr("Cloud download is missing a file name or access token."));
		return;
	}
	if (provider == CloudSyncProviderType::GoogleDrive)
		googleDownloadVersioned(accessToken, fileName);
	else if (provider == CloudSyncProviderType::Dropbox)
		dropboxDownload(accessToken, fileName, true);
	else
		emit operationError(provider, fileName, tr("Legacy Subsurface Cloud uses its existing sync backend."));
}

void CloudSyncFileStore::googleReadVersion(const QString &accessToken, const QString &fileName,
		const QString &fileId, const std::function<void(const QString &)> &continuation)
{
	QUrl url(QStringLiteral("https://www.googleapis.com/drive/v2/files/%1?fields=id,etag").arg(fileId));
	QNetworkReply *reply = networkManager->get(authorizedRequest(url, accessToken));
	trackReply(CloudSyncProviderType::GoogleDrive, reply);
	connect(reply, &QNetworkReply::finished, this, [this, reply, fileName, continuation]() {
		const QByteArray payload = reply->readAll();
		reply->deleteLater();
		if (reply->error() != QNetworkReply::NoError) {
			emit operationError(CloudSyncProviderType::GoogleDrive, fileName, replyError(reply, payload),
				reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() == 401);
			return;
		}
		const QString version = QJsonDocument::fromJson(payload).object().value(QStringLiteral("etag")).toString();
		if (version.isEmpty()) {
			emit operationError(CloudSyncProviderType::GoogleDrive, fileName, tr("Google Drive did not return a file revision. No data was changed."));
			return;
		}
		continuation(version);
	});
}

void CloudSyncFileStore::googleDownloadVersioned(const QString &accessToken, const QString &fileName)
{
	googleFindFile(accessToken, fileName, [this, accessToken, fileName](const QString &fileId) {
		if (fileId.isEmpty()) {
			emit fileNotFound(CloudSyncProviderType::GoogleDrive, fileName);
			return;
		}
		googleReadVersion(accessToken, fileName, fileId, [this, accessToken, fileName, fileId](const QString &version) {
			QUrl url(QStringLiteral("https://www.googleapis.com/drive/v3/files/%1?alt=media").arg(fileId));
			QNetworkReply *reply = networkManager->get(authorizedRequest(url, accessToken));
			trackReply(CloudSyncProviderType::GoogleDrive, reply);
			connect(reply, &QNetworkReply::finished, this, [this, reply, accessToken, fileName, fileId, version]() {
				const QByteArray payload = reply->readAll();
				reply->deleteLater();
				if (reply->error() != QNetworkReply::NoError) {
					emit operationError(CloudSyncProviderType::GoogleDrive, fileName, replyError(reply, payload),
						reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() == 401);
					return;
				}
				// Metadata and media have different ETags. Bracket the media
				// read with metadata reads so its CAS version cannot be stale.
				googleReadVersion(accessToken, fileName, fileId, [this, fileName, fileId, version, payload](const QString &after) {
					if (after != version)
						emit writeConflict(CloudSyncProviderType::GoogleDrive, fileName);
					else
						emit versionedDownloadFinished(CloudSyncProviderType::GoogleDrive, fileName, payload, version, fileId);
				});
			});
		});
	});
}
