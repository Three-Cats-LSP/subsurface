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
	query.addQueryItem(QStringLiteral("fields"), QStringLiteral("files(id,name)"));
	url.setQuery(query);

	QNetworkReply *reply = networkManager->get(authorizedRequest(url, accessToken));
	connect(reply, &QNetworkReply::finished, this, [this, reply, fileName, continuation]() {
		const QByteArray payload = reply->readAll();
		const bool ok = reply->error() == QNetworkReply::NoError;
		const QString error = ok ? QString() : replyError(reply, payload);
		reply->deleteLater();
		if (!ok) {
			emit operationError(CloudSyncProviderType::GoogleDrive, fileName, error);
			return;
		}
		const QJsonArray files = QJsonDocument::fromJson(payload).object().value(QStringLiteral("files")).toArray();
		continuation(files.isEmpty() ? QString() : files.first().toObject().value(QStringLiteral("id")).toString());
	});
}

void CloudSyncFileStore::googleUpload(const QString &accessToken, const QString &fileName, const QByteArray &data)
{
	googleFindFile(accessToken, fileName, [this, accessToken, fileName, data](const QString &fileId) {
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

		connect(reply, &QNetworkReply::finished, this, [this, reply, fileName]() {
			const QByteArray payload = reply->readAll();
			const bool ok = reply->error() == QNetworkReply::NoError;
			const QString error = ok ? QString() : replyError(reply, payload);
			reply->deleteLater();
			if (ok)
				emit uploadFinished(CloudSyncProviderType::GoogleDrive, fileName);
			else
				emit operationError(CloudSyncProviderType::GoogleDrive, fileName, error);
		});
	});
}

void CloudSyncFileStore::googleDownload(const QString &accessToken, const QString &fileName)
{
	googleFindFile(accessToken, fileName, [this, accessToken, fileName](const QString &fileId) {
		if (fileId.isEmpty()) {
			emit operationError(CloudSyncProviderType::GoogleDrive, fileName, tr("Cloud file was not found."));
			return;
		}
		QUrl url(QStringLiteral("https://www.googleapis.com/drive/v3/files/%1").arg(fileId));
		QUrlQuery query;
		query.addQueryItem(QStringLiteral("alt"), QStringLiteral("media"));
		url.setQuery(query);
		QNetworkReply *reply = networkManager->get(authorizedRequest(url, accessToken));
		connect(reply, &QNetworkReply::finished, this, [this, reply, fileName]() {
			const QByteArray payload = reply->readAll();
			const bool ok = reply->error() == QNetworkReply::NoError;
			const QString error = ok ? QString() : replyError(reply, payload);
			reply->deleteLater();
			if (ok)
				emit downloadFinished(CloudSyncProviderType::GoogleDrive, fileName, payload);
			else
				emit operationError(CloudSyncProviderType::GoogleDrive, fileName, error);
		});
	});
}

void CloudSyncFileStore::dropboxUpload(const QString &accessToken, const QString &fileName, const QByteArray &data)
{
	QNetworkRequest request = authorizedRequest(QUrl(QStringLiteral("https://content.dropboxapi.com/2/files/upload")), accessToken);
	request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/octet-stream"));
	request.setRawHeader("Dropbox-API-Arg", compactJson({
		{QStringLiteral("path"), QStringLiteral("/") + fileName},
		{QStringLiteral("mode"), QStringLiteral("overwrite")},
		{QStringLiteral("autorename"), false},
		{QStringLiteral("mute"), true},
	}));
	QNetworkReply *reply = networkManager->post(request, data);
	connect(reply, &QNetworkReply::finished, this, [this, reply, fileName]() {
		const QByteArray payload = reply->readAll();
		const bool ok = reply->error() == QNetworkReply::NoError;
		const QString error = ok ? QString() : replyError(reply, payload);
		reply->deleteLater();
		if (ok)
			emit uploadFinished(CloudSyncProviderType::Dropbox, fileName);
		else
			emit operationError(CloudSyncProviderType::Dropbox, fileName, error);
	});
}

void CloudSyncFileStore::dropboxDownload(const QString &accessToken, const QString &fileName)
{
	QNetworkRequest request = authorizedRequest(QUrl(QStringLiteral("https://content.dropboxapi.com/2/files/download")), accessToken);
	request.setRawHeader("Dropbox-API-Arg", compactJson({{QStringLiteral("path"), QStringLiteral("/") + fileName}}));
	QNetworkReply *reply = networkManager->post(request, QByteArray());
	connect(reply, &QNetworkReply::finished, this, [this, reply, fileName]() {
		const QByteArray payload = reply->readAll();
		const bool ok = reply->error() == QNetworkReply::NoError;
		const QString error = ok ? QString() : replyError(reply, payload);
		reply->deleteLater();
		if (ok)
			emit downloadFinished(CloudSyncProviderType::Dropbox, fileName, payload);
		else
			emit operationError(CloudSyncProviderType::Dropbox, fileName, error);
	});
}
