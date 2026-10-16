// SPDX-License-Identifier: GPL-2.0
#ifndef OAUTH2_TOKEN_CLIENT_H
#define OAUTH2_TOKEN_CLIENT_H

#include "cloudsyncprovider.h"

#include <QDateTime>
#include <QObject>
#include <QPointer>
#include <QUrl>

class QNetworkAccessManager;
class QNetworkReply;

struct OAuth2TokenSet {
	QString accessToken;
	QString refreshToken;
	QString tokenType;
	QString scope;
	// Provider identity is stored with the credential, never in sync manifests.
	QString accountId;
	QString accountName;
	QString accountEmail;
	QDateTime expiresAt;

	bool hasAccessToken() const { return !accessToken.isEmpty(); }
	bool canRefresh() const { return !refreshToken.isEmpty(); }
	bool isExpired(int leewaySeconds = 60) const;
};

class OAuth2TokenClient : public QObject {
	Q_OBJECT
public:
	explicit OAuth2TokenClient(QNetworkAccessManager *networkManager, QObject *parent = nullptr);

	void exchangeAuthorizationCode(const CloudSyncProviderDescriptor &provider,
					   const QString &clientId,
					   const QString &authorizationCode,
					   const QString &codeVerifier,
					   const QUrl &redirectUri);
	void refreshAccessToken(const CloudSyncProviderDescriptor &provider,
					 const QString &clientId,
					 const QString &refreshToken);
	void cancel();

signals:
	void tokenReceived(const OAuth2TokenSet &tokens);
	void tokenError(const QString &message, bool reconnectRequired = false);

private:
	void postTokenRequest(const CloudSyncProviderDescriptor &provider,
				      const QList<QPair<QString, QString>> &parameters,
				      const QString &preservedRefreshToken = QString());
	void handleTokenReply(QNetworkReply *reply, const QString &preservedRefreshToken);

	QNetworkAccessManager *networkManager;
	QPointer<QNetworkReply> pendingReply;
};

Q_DECLARE_METATYPE(OAuth2TokenSet)

#endif // OAUTH2_TOKEN_CLIENT_H
