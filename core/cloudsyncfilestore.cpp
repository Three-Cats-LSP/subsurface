// SPDX-License-Identifier: GPL-2.0
#include "cloudsyncfilestore.h"
#include "cloudsyncmanifest.h"

#include <algorithm>
#include <QRegularExpression>

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
	if (responseLimit > 0) {
		const auto checkSize = [this, provider, reply] {
			if (reply->bytesAvailable() <= responseLimit) return;
			pendingReplies[provider].remove(reply);
			reply->disconnect(this); reply->abort(); reply->deleteLater();
			emit operationError(provider, {}, tr("Cloud response exceeded the history safety limit."));
		};
		connect(reply, &QNetworkReply::readyRead, this, checkSize);
		connect(reply, &QNetworkReply::finished, this, checkSize);
	}
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

void CloudSyncFileStore::setHistoryRootRecovery(const QString &name,
	const std::function<HistoryRootState(const QString &, const QByteArray &)> &validator, const QString &requiredId)
{
	historyRootName = name;
	historyRootValidator = validator;
	historyRootRequiredId = requiredId;
}

void CloudSyncFileStore::googleFindFile(const QString &accessToken, const QString &fileName,
					const std::function<void(const QString &)> &continuation, int recoveryAttempt)
{
	QUrl url(QStringLiteral("https://www.googleapis.com/drive/v3/files"));
	QUrlQuery query;
	QString escapedName = fileName;
	escapedName.replace(QLatin1Char('\\'), QStringLiteral("\\\\"));
	escapedName.replace(QLatin1Char('\''), QStringLiteral("\\'"));
	query.addQueryItem(QStringLiteral("spaces"), QStringLiteral("appDataFolder"));
	query.addQueryItem(QStringLiteral("q"), QStringLiteral("name='%1' and trashed=false").arg(escapedName));
	query.addQueryItem(QStringLiteral("fields"), QStringLiteral("nextPageToken,incompleteSearch,files(id,name,createdTime)"));
	url.setQuery(query);

	QNetworkReply *reply = networkManager->get(authorizedRequest(url, accessToken));
	trackReply(CloudSyncProviderType::GoogleDrive, reply);
	connect(reply, &QNetworkReply::finished, this, [this, reply, accessToken, fileName, continuation, recoveryAttempt]() {
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
		if (object.value(QStringLiteral("incompleteSearch")).toBool() || !object.value(QStringLiteral("nextPageToken")).toString().isEmpty() ||
		    (!files.isEmpty() && files.first().toObject().value(QStringLiteral("id")).toString().isEmpty())) {
			emit operationError(CloudSyncProviderType::GoogleDrive, fileName,
				tr("Google Drive returned an ambiguous or invalid cloud file. No data was changed."));
			return;
		}
		if (files.size() > 1) {
			const bool historyRecovery = historyRootValidator && fileName == historyRootName;
			// Never elect a winner for ambiguous payloads or arbitrary names.
			if ((!historyRecovery && (strictReads || fileName != QStringLiteral("subsurface-neo-sync.json"))) || recoveryAttempt >= 8) {
				emit operationError(CloudSyncProviderType::GoogleDrive, fileName,
					tr("Google Drive has competing cloud files. Sync again to retry recovery. No logs were deleted."));
				return;
			}
			QList<QPair<QDateTime, QString>> order;
			QSet<QString> ids;
			static const QRegularExpression idPattern(QStringLiteral("^[A-Za-z0-9_-]+$"));
			for (const auto &value : files) {
				const auto file = value.toObject();
				const QString id = file.value(QStringLiteral("id")).toString();
				const auto created = QDateTime::fromString(file.value(QStringLiteral("createdTime")).toString(), Qt::ISODateWithMs);
				if (!idPattern.match(id).hasMatch() || ids.contains(id) || !created.isValid() ||
				    file.value(QStringLiteral("name")).toString() != fileName) {
					emit operationError(CloudSyncProviderType::GoogleDrive, fileName,
						tr("Google Drive returned incomplete recovery metadata. No data was changed."));
					return;
				}
				ids.insert(id);
				order.append({created, id});
			}
			if (historyRecovery && !historyRootRequiredId.isEmpty() && !ids.contains(historyRootRequiredId)) {
				emit operationError(CloudSyncProviderType::GoogleDrive, fileName,
					tr("The known history root is missing. Competing files were kept unchanged."));
				return;
			}
			// Provider creation time, not a device clock or last modification.
			// File ID breaks ties, so every recovering client uses the same order.
			std::sort(order.begin(), order.end());
			QList<GoogleRecoveryFile> candidates;
			for (const auto &entry : order) candidates.append({entry.second, {}});
			if (historyRecovery)
				googleRecoverInitialHistoryRoots(accessToken, fileName, candidates, 0, -1, recoveryAttempt, continuation);
			else
				googleRecoverInitialManifests(accessToken, fileName, candidates, 0, recoveryAttempt, continuation);
			return;
		}
		if (strictReads && !files.isEmpty()) {
			const auto file = files.first().toObject();
			if (file.value("name").toString() != fileName ||
			    !QRegularExpression(QStringLiteral("^[A-Za-z0-9_-]+$")).match(file.value("id").toString()).hasMatch()) {
				emit operationError(CloudSyncProviderType::GoogleDrive, fileName, tr("Cloud file identity is invalid.")); return;
			}
		}
		continuation(files.isEmpty() ? QString() : files.first().toObject().value(QStringLiteral("id")).toString());
	});
}

void CloudSyncFileStore::googleRecoverInitialManifests(const QString &accessToken, const QString &fileName,
		QList<GoogleRecoveryFile> files, int next, int attempt,
		const std::function<void(const QString &)> &continuation)
{
	if (next == files.size()) {
		googleArchiveInitialManifest(accessToken, fileName, files, 1, attempt, continuation);
		return;
	}
	const auto retry = [this, accessToken, fileName, continuation, attempt]() {
		googleFindFile(accessToken, fileName, continuation, attempt + 1);
	};
	googleReadFileVersioned(accessToken, fileName, files[next].id,
		[this, accessToken, fileName, files, next, attempt, continuation](const QByteArray &data, const QString &version) mutable {
			const auto manifest = CloudSyncManifest::fromJson(data);
			// An established branch is not an abandoned initial upload. Keep it
			// visible for manual recovery instead of silently demoting its edits.
			if (!manifest.isValid() || (next > 0 && (manifest.schemaVersion != 2 || !manifest.parentSha256.isEmpty()))) {
				emit operationError(CloudSyncProviderType::GoogleDrive, fileName,
					tr("Competing Google Drive logs include an invalid or previously synchronized log. All copies were kept for recovery."));
				return;
			}
			files[next].version = version;
			googleRecoverInitialManifests(accessToken, fileName, files, next + 1, attempt, continuation);
		}, retry);
}

void CloudSyncFileStore::googleRecoverInitialHistoryRoots(const QString &accessToken, const QString &fileName,
	QList<GoogleRecoveryFile> files, int next, int active, int attempt,
	const std::function<void(const QString &)> &continuation)
{
	if (next == files.size()) {
		// A root with any durable activity wins over every pristine root,
		// even when an older, delayed create becomes visible later.
		if (active > 0) files.swapItemsAt(0, active);
		googleArchiveInitialManifest(accessToken, fileName, files, 1, attempt, continuation);
		return;
	}
	googleReadFileVersioned(accessToken, fileName, files[next].id,
		[this, accessToken, fileName, files, next, active, attempt, continuation](const QByteArray &data, const QString &version) mutable {
			const auto state = historyRootValidator && fileName == historyRootName ? historyRootValidator(files[next].id, data) : HistoryRootState::Invalid;
			if (state == HistoryRootState::Invalid || (state == HistoryRootState::Active && active >= 0)) {
				emit operationError(CloudSyncProviderType::GoogleDrive, fileName,
					tr("Competing history roots contain activity or unsupported data. All roots and versions were kept for recovery."));
				return;
			}
			files[next].version = version;
			googleRecoverInitialHistoryRoots(accessToken, fileName, files, next + 1,
				state == HistoryRootState::Active ? next : active, attempt, continuation);
		}, [this, accessToken, fileName, continuation, attempt] {
			googleFindFile(accessToken, fileName, continuation, attempt + 1);
		});
}

void CloudSyncFileStore::googleArchiveInitialManifest(const QString &accessToken, const QString &fileName,
		const QList<GoogleRecoveryFile> &files, int next, int attempt,
		const std::function<void(const QString &)> &continuation)
{
	if (next == files.size()) {
		// Re-list after recovery: another client may have recovered these files,
		// or a delayed first upload may have created another initial candidate.
		googleFindFile(accessToken, fileName, continuation, attempt + 1);
		return;
	}
	const auto &file = files[next];
	QNetworkRequest request = authorizedRequest(QUrl(QStringLiteral("https://www.googleapis.com/drive/v2/files/%1").arg(file.id)), accessToken);
	request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
	request.setRawHeader("If-Match", file.version.toUtf8());
	const QByteArray metadata = compactJson({{QStringLiteral("title"),
		fileName.left(fileName.size() - 5) + QStringLiteral("-recovered-%1.json").arg(file.id)}});
	QNetworkReply *reply = networkManager->sendCustomRequest(request, "PATCH", metadata);
	trackReply(CloudSyncProviderType::GoogleDrive, reply);
	connect(reply, &QNetworkReply::finished, this, [this, reply, accessToken, fileName, files, next, attempt, continuation]() {
		const QByteArray payload = reply->readAll();
		const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
		reply->deleteLater();
		if (reply->error() == QNetworkReply::NoError)
			googleArchiveInitialManifest(accessToken, fileName, files, next + 1, attempt, continuation);
		else if (status == 412 || status == 404)
			googleFindFile(accessToken, fileName, continuation, attempt + 1);
		else
			emit operationError(CloudSyncProviderType::GoogleDrive, fileName, replyError(reply, payload), status == 401);
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

void CloudSyncFileStore::dropboxDownload(const QString &accessToken, const QString &fileName, bool versioned,
	const QString &expectedId, const QString &expectedVersion)
{
	QNetworkRequest request = authorizedRequest(QUrl(QStringLiteral("https://content.dropboxapi.com/2/files/download")), accessToken);
	request.setRawHeader("Dropbox-API-Arg", compactJson({{QStringLiteral("path"), expectedId.isEmpty() ? QStringLiteral("/") + fileName : expectedId}}));
	QNetworkReply *reply = networkManager->post(request, QByteArray());
	trackReply(CloudSyncProviderType::Dropbox, reply);
	connect(reply, &QNetworkReply::finished, this, [this, reply, fileName, versioned, expectedId, expectedVersion]() {
		const QByteArray payload = reply->readAll();
		const bool ok = reply->error() == QNetworkReply::NoError;
		const QString error = ok ? QString() : replyError(reply, payload);
		const QJsonObject providerError = QJsonDocument::fromJson(payload).object().value(QStringLiteral("error")).toObject();
		const bool missing = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() == 409 &&
			providerError.value(QStringLiteral(".tag")).toString() == QStringLiteral("path") &&
			providerError.value(QStringLiteral("path")).toObject().value(QStringLiteral(".tag")).toString() == QStringLiteral("not_found");
		reply->deleteLater();
		if (ok && versioned) {
			const auto metadata = QJsonDocument::fromJson(reply->rawHeader("Dropbox-API-Result")).object();
			const QString version = metadata.value(QStringLiteral("rev")).toString();
			const QString id = metadata.value(QStringLiteral("id")).toString();
			if (version.isEmpty() || (strictReads && (id.isEmpty() || metadata.value("name").toString() != fileName)) ||
			    (!expectedId.isEmpty() && (id != expectedId || version != expectedVersion)))
				emit operationError(CloudSyncProviderType::Dropbox, fileName, tr("Dropbox did not return a file revision. No data was changed."));
			else
				emit versionedDownloadFinished(CloudSyncProviderType::Dropbox, fileName, payload, version, id);
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
		// The post-commit lookup recovers duplicate initial manifests. Every
		// competing immutable payload and manifest remains available.
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
		const QString &fileId, const std::function<void(const QString &)> &continuation, const std::function<void()> &changed)
{
	QUrl url(QStringLiteral("https://www.googleapis.com/drive/v2/files/%1?fields=id,etag,title").arg(fileId));
	QNetworkReply *reply = networkManager->get(authorizedRequest(url, accessToken));
	trackReply(CloudSyncProviderType::GoogleDrive, reply);
	connect(reply, &QNetworkReply::finished, this, [this, reply, fileName, fileId, continuation, changed]() {
		const QByteArray payload = reply->readAll();
		reply->deleteLater();
		if (reply->error() != QNetworkReply::NoError) {
			if (changed && reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() == 404) {
				changed();
				return;
			}
			emit operationError(CloudSyncProviderType::GoogleDrive, fileName, replyError(reply, payload),
				reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() == 401);
			return;
		}
		const auto metadata = QJsonDocument::fromJson(payload).object();
		const QString version = metadata.value(QStringLiteral("etag")).toString();
		if (version.isEmpty() || (strictReads && metadata.value("id").toString() != fileId)) {
			emit operationError(CloudSyncProviderType::GoogleDrive, fileName, tr("Google Drive did not return a file revision. No data was changed."));
			return;
		}
		if (metadata.value(QStringLiteral("title")).toString() != fileName) {
			if (changed) changed();
			else emit writeConflict(CloudSyncProviderType::GoogleDrive, fileName);
			return;
		}
		continuation(version);
	});
}

void CloudSyncFileStore::downloadExact(CloudSyncProviderType provider, const QString &accessToken,
	const QString &fileName, const QString &fileId, const QString &version)
{
	if (accessToken.isEmpty() || safeRemoteName(fileName) != fileName || fileId.isEmpty() || version.isEmpty()) {
		emit operationError(provider, fileName, tr("Cloud snapshot identity is incomplete.")); return;
	}
	if (provider == CloudSyncProviderType::Dropbox && fileId.startsWith("id:")) {
		dropboxDownload(accessToken, fileName, true, fileId, version);
	} else if (provider == CloudSyncProviderType::GoogleDrive && QRegularExpression(QStringLiteral("^[A-Za-z0-9_-]+$")).match(fileId).hasMatch()) {
		googleReadFileVersioned(accessToken, fileName, fileId, [this, fileName, fileId, version](const QByteArray &data, const QString &actual) {
			if (actual != version) emit writeConflict(CloudSyncProviderType::GoogleDrive, fileName);
			else emit versionedDownloadFinished(CloudSyncProviderType::GoogleDrive, fileName, data, actual, fileId);
		});
	} else emit operationError(provider, fileName, tr("Cloud snapshot identity is invalid."));
}

void CloudSyncFileStore::downloadIdentity(CloudSyncProviderType provider, const QString &accessToken,
	const QString &fileName, const QString &fileId)
{
	// A pristine-root alias has a new revision after its conditional rename.
	// Read that exact ID under its expected archived name, bracketing its body
	// with metadata reads; never elect another file by name.
	if (provider != CloudSyncProviderType::GoogleDrive || accessToken.isEmpty() || safeRemoteName(fileName) != fileName ||
	    !QRegularExpression(QStringLiteral("^[A-Za-z0-9_-]+$")).match(fileId).hasMatch()) {
		emit operationError(provider, fileName, tr("Cloud object identity is invalid.")); return;
	}
	googleReadFileVersioned(accessToken, fileName, fileId, [this, fileName, fileId](const QByteArray &data, const QString &version) {
		emit versionedDownloadFinished(CloudSyncProviderType::GoogleDrive, fileName, data, version, fileId);
	});
}

void CloudSyncFileStore::googleReadFileVersioned(const QString &accessToken, const QString &fileName, const QString &fileId,
		const std::function<void(const QByteArray &, const QString &)> &continuation, const std::function<void()> &changed)
{
	googleReadVersion(accessToken, fileName, fileId, [this, accessToken, fileName, fileId, continuation, changed](const QString &version) {
		QUrl url(QStringLiteral("https://www.googleapis.com/drive/v3/files/%1?alt=media").arg(fileId));
		QNetworkReply *reply = networkManager->get(authorizedRequest(url, accessToken));
		trackReply(CloudSyncProviderType::GoogleDrive, reply);
		connect(reply, &QNetworkReply::finished, this, [this, reply, accessToken, fileName, fileId, version, continuation, changed]() {
			const QByteArray payload = reply->readAll();
			reply->deleteLater();
			if (reply->error() != QNetworkReply::NoError) {
				const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
				if (changed && status == 404) changed();
				else emit operationError(CloudSyncProviderType::GoogleDrive, fileName, replyError(reply, payload), status == 401);
				return;
			}
			// Metadata and media have different ETags. Bracket the media read
			// with metadata reads so its CAS version cannot be stale.
			googleReadVersion(accessToken, fileName, fileId, [this, fileName, version, payload, continuation, changed](const QString &after) {
				if (after != version) {
					if (changed) changed();
					else emit writeConflict(CloudSyncProviderType::GoogleDrive, fileName);
				} else continuation(payload, version);
			}, changed);
		});
	}, changed);
}

void CloudSyncFileStore::googleDownloadVersioned(const QString &accessToken, const QString &fileName, int attempt)
{
	if (attempt >= 8) {
		emit writeConflict(CloudSyncProviderType::GoogleDrive, fileName);
		return;
	}
	googleFindFile(accessToken, fileName, [this, accessToken, fileName, attempt](const QString &fileId) {
		if (fileId.isEmpty()) {
			emit fileNotFound(CloudSyncProviderType::GoogleDrive, fileName);
			return;
		}
		googleReadFileVersioned(accessToken, fileName, fileId, [this, fileName, fileId](const QByteArray &payload, const QString &version) {
			emit versionedDownloadFinished(CloudSyncProviderType::GoogleDrive, fileName, payload, version, fileId);
		}, [this, accessToken, fileName, attempt]() {
			googleDownloadVersioned(accessToken, fileName, attempt + 1);
		});
	}, attempt);
}
