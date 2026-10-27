// SPDX-License-Identifier: GPL-2.0
#ifndef CLOUD_SYNC_FILE_STORE_H
#define CLOUD_SYNC_FILE_STORE_H

#include "cloudsyncprovider.h"

#include <QByteArray>
#include <QObject>
#include <QMap>
#include <QSet>

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
	void cancel(CloudSyncProviderType provider);
	// An empty version means create only. Google additionally needs the file ID
	// returned with the version; names alone are not unique in Drive.
	void uploadConditional(CloudSyncProviderType provider, const QString &accessToken,
		const QString &fileName, const QByteArray &data, const QString &version, const QString &fileId = {});
	void downloadVersioned(CloudSyncProviderType provider, const QString &accessToken, const QString &fileName);
	// History uses strict reads: duplicate legacy names must never trigger a rename.
	void setStrictReads(bool enabled) { strictReads = enabled; }
	// Explicit opt-in for one history root. Only pristine duplicates may be
	// renamed; the caller validates the full format and account binding.
	enum class HistoryRootState { Invalid, Pristine, Active };
	void setHistoryRootRecovery(const QString &name,
		const std::function<HistoryRootState(const QByteArray &)> &validator);
	void setResponseLimit(qint64 bytes) { responseLimit = bytes; }
	void downloadExact(CloudSyncProviderType provider, const QString &accessToken,
		const QString &fileName, const QString &fileId, const QString &version);

signals:
	void uploadFinished(CloudSyncProviderType provider, const QString &fileName);
	void downloadFinished(CloudSyncProviderType provider, const QString &fileName, const QByteArray &data);
	void operationError(CloudSyncProviderType provider, const QString &fileName, const QString &message,
			    bool tokenRejected = false);
	void fileNotFound(CloudSyncProviderType provider, const QString &fileName);
	void versionedDownloadFinished(CloudSyncProviderType provider, const QString &fileName,
		const QByteArray &data, const QString &version, const QString &fileId);
	void writeConflict(CloudSyncProviderType provider, const QString &fileName);

private:
	struct GoogleRecoveryFile { QString id, version; };
	void trackReply(CloudSyncProviderType provider, QNetworkReply *reply);
	void googleFindFile(const QString &accessToken, const QString &fileName,
			    const std::function<void(const QString &)> &continuation, int recoveryAttempt = 0);
	void googleRecoverInitialManifests(const QString &accessToken, const QString &fileName,
		QList<GoogleRecoveryFile> files, int next, int attempt,
		const std::function<void(const QString &)> &continuation);
	void googleArchiveInitialManifest(const QString &accessToken, const QString &fileName,
		const QList<GoogleRecoveryFile> &files, int next, int attempt,
		const std::function<void(const QString &)> &continuation);
	void googleRecoverInitialHistoryRoots(const QString &accessToken, const QString &fileName,
		QList<GoogleRecoveryFile> files, int next, int active, int attempt,
		const std::function<void(const QString &)> &continuation);
	void googleUpload(const QString &accessToken, const QString &fileName, const QByteArray &data, bool createOnly = false);
	void googleDownload(const QString &accessToken, const QString &fileName);
	void dropboxUpload(const QString &accessToken, const QString &fileName, const QByteArray &data,
		bool conditional = false, const QString &version = {});
	void dropboxDownload(const QString &accessToken, const QString &fileName, bool versioned = false,
		const QString &expectedId = {}, const QString &expectedVersion = {});
	void googleDownloadVersioned(const QString &accessToken, const QString &fileName, int attempt = 0);
	void googleReadVersion(const QString &accessToken, const QString &fileName, const QString &fileId,
		const std::function<void(const QString &)> &continuation, const std::function<void()> &changed = {});
	void googleReadFileVersioned(const QString &accessToken, const QString &fileName, const QString &fileId,
		const std::function<void(const QByteArray &, const QString &)> &continuation,
		const std::function<void()> &changed = {});

	QNetworkRequest authorizedRequest(const QUrl &url, const QString &accessToken) const;
	QString replyError(QNetworkReply *reply, const QByteArray &payload) const;
	static QString safeRemoteName(const QString &fileName);

	QNetworkAccessManager *networkManager;
	QMap<CloudSyncProviderType, QSet<QNetworkReply *>> pendingReplies;
	bool strictReads = false;
	QString historyRootName;
	std::function<HistoryRootState(const QByteArray &)> historyRootValidator;
	qint64 responseLimit = 0;
};

Q_DECLARE_METATYPE(CloudSyncProviderType)

#endif // CLOUD_SYNC_FILE_STORE_H
