// SPDX-License-Identifier: GPL-2.0
#ifndef CLOUD_SYNC_MANAGER_H
#define CLOUD_SYNC_MANAGER_H

#include "cloudsyncfilestore.h"
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
	Q_PROPERTY(QString lastError READ lastError NOTIFY lastErrorChanged)
public:
	explicit CloudSyncManager(QNetworkAccessManager *networkManager, QObject *parent = nullptr);
	~CloudSyncManager() override;

	QVariantList providers() const;
	bool authorizationInProgress() const { return !activeProviderId.isEmpty(); }
	QString lastError() const { return errorText; }

	Q_INVOKABLE void beginAuthorization(const QString &providerId);
	Q_INVOKABLE void handleAuthorizationRedirect(const QUrl &url);
	Q_INVOKABLE void disconnectProvider(const QString &providerId);
	Q_INVOKABLE void uploadBytes(const QString &providerId, const QString &fileName, const QByteArray &data);
	Q_INVOKABLE void downloadBytes(const QString &providerId, const QString &fileName);
	Q_INVOKABLE void backupDiveLog(const QString &providerId);

signals:
	void providersChanged();
	void authorizationInProgressChanged();
	void lastErrorChanged();
	void authorizationUrlOpened(const QUrl &url);
	void providerConnected(const QString &providerId);
	void providerDisconnected(const QString &providerId);
	void uploadFinished(const QString &providerId, const QString &fileName);
	void downloadFinished(const QString &providerId, const QString &fileName, const QByteArray &data);
	void diveLogBackupFinished(const QString &providerId);

private:
	const CloudSyncProviderDescriptor *descriptorForId(const QString &providerId) const;
	QString configuredClientId(const CloudSyncProviderDescriptor &provider) const;
	void setError(const QString &message);
	void clearAuthorization();
	void exchangeCode(const QString &code);
	QUrl startLoopbackListener(const CloudSyncProviderDescriptor &provider);
	void handleLoopbackConnection();
	void finishLoopbackSocket(QTcpSocket *socket, bool success, const QString &message);
	void refreshThen(const CloudSyncProviderDescriptor &provider, const QString &providerId,
			 const std::function<void(const QString &)> &continuation);

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
};

#endif // CLOUD_SYNC_MANAGER_H
