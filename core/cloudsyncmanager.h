// SPDX-License-Identifier: GPL-2.0
#ifndef CLOUD_SYNC_MANAGER_H
#define CLOUD_SYNC_MANAGER_H

#include "cloudsyncfilestore.h"
#include "cloudsyncmanifest.h"
#include "oauth2pkce.h"
#include "oauth2tokenclient.h"

#include <QHash>
#include <QObject>
#include <QPointer>
#include <QUrl>
#include <QVariantList>

#include <functional>
#include <memory>

class QNetworkAccessManager;
class QTcpServer;
class QTcpSocket;

class CloudSyncManager : public QObject {
	Q_OBJECT
	Q_PROPERTY(QVariantList providers READ providers NOTIFY providersChanged)
	Q_PROPERTY(bool authorizationInProgress READ authorizationInProgress NOTIFY authorizationInProgressChanged)
	Q_PROPERTY(bool syncInProgress READ syncInProgress NOTIFY syncInProgressChanged)
	Q_PROPERTY(QString lastError READ lastError NOTIFY lastErrorChanged)
	Q_PROPERTY(QString primaryProviderId READ primaryProviderId NOTIFY providerAssignmentsChanged)
	Q_PROPERTY(QString backupProviderId READ backupProviderId NOTIFY providerAssignmentsChanged)
public:
	explicit CloudSyncManager(QNetworkAccessManager *networkManager, QObject *parent = nullptr);
	~CloudSyncManager() override;

	QVariantList providers() const;
	bool authorizationInProgress() const { return !activeProviderId.isEmpty(); }
	bool syncInProgress() const { return !syncProviderId.isEmpty(); }
	QString lastError() const { return errorText; }
	QString primaryProviderId() const;
	QString backupProviderId() const;

	Q_INVOKABLE void beginAuthorization(const QString &providerId);
	Q_INVOKABLE void handleAuthorizationRedirect(const QUrl &url);
	Q_INVOKABLE void disconnectProvider(const QString &providerId);
	Q_INVOKABLE void uploadBytes(const QString &providerId, const QString &fileName, const QByteArray &data);
	Q_INVOKABLE void downloadBytes(const QString &providerId, const QString &fileName);
	Q_INVOKABLE void backupDiveLog(const QString &providerId);
	Q_INVOKABLE void syncDiveLog(const QString &providerId);
	Q_INVOKABLE void useCloudDiveLog(const QString &providerId);
	Q_INVOKABLE void setPrimaryProvider(const QString &providerId);
	Q_INVOKABLE void setBackupProvider(const QString &providerId);
	void handleAndroidGoogleAccessToken(const QString &accessToken);
	void handleAndroidGoogleAuthorizationError(const QString &message);

signals:
	void providersChanged();
	void providerAssignmentsChanged();
	void authorizationInProgressChanged();
	void syncInProgressChanged();
	void lastErrorChanged();
	void authorizationUrlOpened(const QUrl &url);
	void providerConnected(const QString &providerId);
	void providerDisconnected(const QString &providerId);
	void uploadFinished(const QString &providerId, const QString &fileName);
	void downloadFinished(const QString &providerId, const QString &fileName, const QByteArray &data);
	void diveLogBackupFinished(const QString &providerId);
	void diveLogSyncFinished(const QString &providerId, const QString &result);
	void diveLogSyncConflict(const QString &providerId);
	void diveLogInitialChoiceRequired(const QString &providerId);

private:
	enum class SyncOperation {
		None,
		BackupUploadPayload,
		BackupUploadManifest,
		SyncDownloadManifest,
		SyncUploadPayload,
		SyncUploadManifest,
		SyncDownloadPayload,
	};

	const CloudSyncProviderDescriptor *descriptorForId(const QString &providerId) const;
	QString configuredClientId(const CloudSyncProviderDescriptor &provider) const;
	void setError(const QString &message);
	void acceptNativeAccessToken(const QString &providerId, const QString &accessToken);
	void requestAndroidGoogleAccessToken();
	void clearAuthorization();
	void exchangeCode(const QString &code);
	QUrl startLoopbackListener(const CloudSyncProviderDescriptor &provider);
	void handleLoopbackConnection();
	void finishLoopbackSocket(QTcpSocket *socket, bool success, const QString &message);
	void refreshThen(const CloudSyncProviderDescriptor &provider, const QString &providerId,
			 const std::function<void(const QString &)> &continuation);
	QByteArray serializeCurrentDiveLog();
	bool applyCloudDiveLog(const QByteArray &payload);
	CloudSyncManifest lastSyncManifest(const QString &providerId) const;
	void saveLastSyncManifest(const QString &providerId, const CloudSyncManifest &manifest);
	void startUploadSequence(const QString &providerId, const QByteArray &payload,
				 const QString &parentSha256, bool backupOnly);
	void handleDownloadedManifest(const QString &providerId, const QByteArray &data);
	void handleDownloadedDiveLog(const QString &providerId, const QByteArray &data);
	void clearSyncOperation();
	static QString syncStateCredentialKey(const QString &providerId);
	static bool isNotFoundError(const QString &message);

	QNetworkAccessManager *networkManager;
	OAuth2TokenClient tokenClient;
	CloudSyncFileStore fileStore;
	QHash<QString, OAuth2TokenSet> tokens;
	QString activeProviderId;
	QString activeClientId;
	QUrl activeRedirectUri;
	std::unique_ptr<OAuth2PkceSession> activePkce;
	QPointer<QTcpServer> loopbackServer;
	QString errorText;
	QString refreshProviderId;
	std::function<void(const QString &)> pendingTokenContinuation;

	SyncOperation syncOperation = SyncOperation::None;
	QString syncProviderId;
	QByteArray syncLocalPayload;
	QString syncLocalSha256;
	CloudSyncManifest syncRemoteManifest;
	CloudSyncManifest syncUploadManifest;
	bool forceCloudDownload = false;
};

#endif // CLOUD_SYNC_MANAGER_H
