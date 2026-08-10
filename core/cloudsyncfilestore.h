// SPDX-License-Identifier: GPL-2.0
#ifndef CLOUD_SYNC_FILE_STORE_H
#define CLOUD_SYNC_FILE_STORE_H

#include "cloudsyncprovider.h"

#include <QByteArray>
#include <QObject>

#include <functional>

class QNetworkAccessManager;
class QNetworkReply;
class QNetworkRequest;

class CloudSyncFileStore : public QObject {
	Q_OBJECT
public:
	explicit CloudSyncFileStore(QNetworkAccessManager *networkManager, QObject *parent = nullptr);

	void upload(CloudSyncProviderType provider, const QString &accessToken,
		    const QString &fileName, const QByteArray &data);
	void download(CloudSyncProviderType provider, const QString &accessToken,
		      const QString &fileName);

signals:
	void uploadFinished(CloudSyncProviderType provider, const QString &fileName);
	void downloadFinished(CloudSyncProviderType provider, const QString &fileName, const QByteArray &data);
	void operationError(CloudSyncProviderType provider, const QString &fileName, const QString &message);

private:
	void googleFindFile(const QString &accessToken, const QString &fileName,
			    const std::function<void(const QString &)> &continuation);
	void googleUpload(const QString &accessToken, const QString &fileName, const QByteArray &data);
	void googleDownload(const QString &accessToken, const QString &fileName);
	void dropboxUpload(const QString &accessToken, const QString &fileName, const QByteArray &data);
	void dropboxDownload(const QString &accessToken, const QString &fileName);
	void oneDriveUpload(const QString &accessToken, const QString &fileName, const QByteArray &data);
	void oneDriveDownload(const QString &accessToken, const QString &fileName);

	QNetworkRequest authorizedRequest(const QUrl &url, const QString &accessToken) const;
	QString replyError(QNetworkReply *reply, const QByteArray &payload) const;
	static QString safeRemoteName(const QString &fileName);

	QNetworkAccessManager *networkManager;
};

Q_DECLARE_METATYPE(CloudSyncProviderType)

#endif // CLOUD_SYNC_FILE_STORE_H
