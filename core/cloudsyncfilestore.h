// SPDX-License-Identifier: GPL-2.0
#ifndef CLOUD_SYNC_FILE_STORE_H
#define CLOUD_SYNC_FILE_STORE_H

#include "cloudsyncprovider.h"

#include <QByteArray>
#include <QObject>
#include <QMap>
#include <QSet>
#include <QStringList>

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
	// Update only this exact identity/revision, with no name-based fallback or
	// create mode. Confirm the current identity/body again before publication.
	void uploadIdentity(CloudSyncProviderType provider, const QString &accessToken,
		const QString &fileName, const QByteArray &data, const QString &version, const QString &fileId);
	// Transport primitive for a verified sealed history source. Caller must
	// verify its fence/body and durable coordinator intent before invoking it.
	// Google v2 ETag CAS only; never looks up a name, moves a folder or deletes.
	void archiveGoogleHistory(const QString &accessToken, const QString &rootName, const QString &fileId, const QString &version);
	void downloadVersioned(CloudSyncProviderType provider, const QString &accessToken, const QString &fileName);
	// History uses strict reads: duplicate legacy names must never trigger a rename.
	void setStrictReads(bool enabled) { strictReads = enabled; }
	// Explicit opt-in for one history root. Only pristine duplicates may be
	// renamed; the caller validates the full format and account binding.
	enum class HistoryRootState { Invalid, Pristine, Active };
	void setHistoryRootRecovery(const QString &name,
		const std::function<HistoryRootState(const QString &, const QByteArray &)> &validator, const QString &requiredId = {});
	void setResponseLimit(qint64 bytes) { responseLimit = bytes; }
	void downloadExact(CloudSyncProviderType provider, const QString &accessToken,
		const QString &fileName, const QString &fileId, const QString &version);
	// Read the current revision of one exact identity after an uncertain write.
	// Returned provider ID and expected name must still match.
	void downloadIdentity(CloudSyncProviderType provider, const QString &accessToken,
		const QString &fileName, const QString &fileId);
	// Read-only discovery for branch review. Never chooses or renames a root.
	void findHistoryRoots(const QString &accessToken, const QString &fileName);

signals:
	void uploadFinished(CloudSyncProviderType provider, const QString &fileName);
	void downloadFinished(CloudSyncProviderType provider, const QString &fileName, const QByteArray &data);
	void operationError(CloudSyncProviderType provider, const QString &fileName, const QString &message,
			    bool tokenRejected = false);
	void fileNotFound(CloudSyncProviderType provider, const QString &fileName);
	void versionedDownloadFinished(CloudSyncProviderType provider, const QString &fileName,
		const QByteArray &data, const QString &version, const QString &fileId);
	void writeConflict(CloudSyncProviderType provider, const QString &fileName);
	void historyRootsFound(const QStringList &ids);
	void historyRootArchived(const QString &archiveName, const QString &fileId);

private:
	struct GoogleRecoveryFile { QString id, version; };
	void trackReply(CloudSyncProviderType provider, QNetworkReply *reply, qint64 uploadBytes = 0, const QString &fileName = {});
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
		bool conditional = false, const QString &version = {}, const QString &fileId = {});
	void dropboxDownload(const QString &accessToken, const QString &fileName, bool versioned = false,
		const QString &expectedId = {}, const QString &expectedVersion = {});
	void googleDownloadVersioned(const QString &accessToken, const QString &fileName, int attempt = 0);
	void googleFindHistoryRoots(const QString &accessToken, const QString &fileName,
		const QString &cursor, const QSet<QString> &cursors, const QStringList &ids);
	void googleReadVersion(const QString &accessToken, const QString &fileName, const QString &fileId,
		const std::function<void(const QString &)> &continuation, const std::function<void()> &changed = {});
	void googleReadFileVersioned(const QString &accessToken, const QString &fileName, const QString &fileId,
		const std::function<void(const QByteArray &, const QString &)> &continuation,
		const std::function<void()> &changed = {});

	QNetworkRequest authorizedRequest(const QUrl &url, const QString &accessToken, qint64 uploadBytes = 0) const;
	QString replyError(QNetworkReply *reply, const QByteArray &payload) const;
	static QString safeRemoteName(const QString &fileName);

	QNetworkAccessManager *networkManager;
	QMap<CloudSyncProviderType, QSet<QNetworkReply *>> pendingReplies;
	bool strictReads = false;
	QString historyRootName, historyRootRequiredId;
	std::function<HistoryRootState(const QString &, const QByteArray &)> historyRootValidator;
	qint64 responseLimit = 0;
};

Q_DECLARE_METATYPE(CloudSyncProviderType)

#endif // CLOUD_SYNC_FILE_STORE_H
