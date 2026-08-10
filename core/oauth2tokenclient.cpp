// SPDX-License-Identifier: GPL-2.0
#include "oauth2tokenclient.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrlQuery>

bool OAuth2TokenSet::isExpired(int leewaySeconds) const
{
	return expiresAt.isValid() && QDateTime::currentDateTimeUtc().addSecs(leewaySeconds) >= expiresAt;
}

OAuth2TokenClient::OAuth2TokenClient(QNetworkAccessManager *networkManager, QObject *parent) :
	QObject(parent), networkManager(networkManager)
{
	Q_ASSERT(networkManager);
	qRegisterMetaType<OAuth2TokenSet>();
}

void OAuth2TokenClient::exchangeAuthorizationCode(const CloudSyncProviderDescriptor &provider,
							 const QString &clientId,
							 const QString &authorizationCode,
							 const QString &codeVerifier,
							 const QUrl &redirectUri)
{
	postTokenRequest(provider, {
		{QStringLiteral("client_id"), clientId},
		{QStringLiteral("grant_type"), QStringLiteral("authorization_code")},
		{QStringLiteral("code"), authorizationCode},
		{QStringLiteral("code_verifier"), codeVerifier},
		{QStringLiteral("redirect_uri"), redirectUri.toString()},
	});
}

void OAuth2TokenClient::refreshAccessToken(const CloudSyncProviderDescriptor &provider,
							const QString &clientId,
							const QString &refreshToken)
{
	postTokenRequest(provider, {
		{QStringLiteral("client_id"), clientId},
		{QStringLiteral("grant_type"), QStringLiteral("refresh_token")},
		{QStringLiteral("refresh_token"), refreshToken},
	}, refreshToken);
}

void OAuth2TokenClient::postTokenRequest(const CloudSyncProviderDescriptor &provider,
							 const QList<QPair<QString, QString>> &parameters,
							 const QString &preservedRefreshToken)
{
	if (!networkManager) {
		emit tokenError(tr("OAuth network manager is unavailable."));
		return;
	}
	if (!provider.tokenEndpoint.isValid() || provider.tokenEndpoint.isEmpty()) {
		emit tokenError(tr("This cloud provider does not use OAuth token exchange."));
		return;
	}

	QUrlQuery form;
	for (const auto &parameter : parameters)
		form.addQueryItem(parameter.first, parameter.second);

	QNetworkRequest request(provider.tokenEndpoint);
	request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/x-www-form-urlencoded"));
	request.setRawHeader("Accept", "application/json");

	QNetworkReply *reply = networkManager->post(request, form.query(QUrl::FullyEncoded).toUtf8());
	connect(reply, &QNetworkReply::finished, this, [this, reply, preservedRefreshToken]() {
		handleTokenReply(reply, preservedRefreshToken);
	});
}

void OAuth2TokenClient::handleTokenReply(QNetworkReply *reply, const QString &preservedRefreshToken)
{
	const QByteArray payload = reply->readAll();
	const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
	const QNetworkReply::NetworkError networkError = reply->error();
	reply->deleteLater();

	QJsonParseError parseError;
	const QJsonDocument document = QJsonDocument::fromJson(payload, &parseError);
	const QJsonObject object = document.isObject() ? document.object() : QJsonObject();

	if (networkError != QNetworkReply::NoError || status < 200 || status >= 300) {
		QString message = object.value(QStringLiteral("error_description")).toString();
		if (message.isEmpty())
			message = object.value(QStringLiteral("error_summary")).toString();
		if (message.isEmpty())
			message = object.value(QStringLiteral("error")).toString();
		if (message.isEmpty())
			message = tr("OAuth token request failed (HTTP %1).").arg(status);
		emit tokenError(message);
		return;
	}

	if (parseError.error != QJsonParseError::NoError || object.isEmpty()) {
		emit tokenError(tr("Cloud provider returned an invalid OAuth token response."));
		return;
	}

	OAuth2TokenSet tokens;
	tokens.accessToken = object.value(QStringLiteral("access_token")).toString();
	tokens.refreshToken = object.value(QStringLiteral("refresh_token")).toString();
	if (tokens.refreshToken.isEmpty())
		tokens.refreshToken = preservedRefreshToken;
	tokens.tokenType = object.value(QStringLiteral("token_type")).toString(QStringLiteral("Bearer"));
	tokens.scope = object.value(QStringLiteral("scope")).toString();

	const int expiresIn = object.value(QStringLiteral("expires_in")).toInt();
	if (expiresIn > 0)
		tokens.expiresAt = QDateTime::currentDateTimeUtc().addSecs(expiresIn);

	if (!tokens.hasAccessToken()) {
		emit tokenError(tr("Cloud provider did not return an access token."));
		return;
	}

	emit tokenReceived(tokens);
}
