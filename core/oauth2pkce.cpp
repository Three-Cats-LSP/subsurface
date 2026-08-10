// SPDX-License-Identifier: GPL-2.0
#include "oauth2pkce.h"

#include <QCryptographicHash>
#include <QRandomGenerator>
#include <QUrlQuery>

OAuth2PkceSession::OAuth2PkceSession()
{
	params.verifier = base64Url(randomBytes(32));
	params.challenge = base64Url(QCryptographicHash::hash(params.verifier.toUtf8(), QCryptographicHash::Sha256));
	params.state = base64Url(randomBytes(24));
}

const OAuth2PkceParameters &OAuth2PkceSession::parameters() const
{
	return params;
}

QUrl OAuth2PkceSession::authorizationUrl(const QUrl &authorizationEndpoint,
					 const QString &clientId,
					 const QUrl &redirectUri,
					 const QStringList &scopes,
					 const QMap<QString, QString> &extraParameters) const
{
	QUrl url(authorizationEndpoint);
	QUrlQuery query(url);
	query.addQueryItem(QStringLiteral("client_id"), clientId);
	query.addQueryItem(QStringLiteral("redirect_uri"), redirectUri.toString());
	query.addQueryItem(QStringLiteral("response_type"), QStringLiteral("code"));
	query.addQueryItem(QStringLiteral("scope"), scopes.join(QLatin1Char(' ')));
	query.addQueryItem(QStringLiteral("state"), params.state);
	query.addQueryItem(QStringLiteral("code_challenge"), params.challenge);
	query.addQueryItem(QStringLiteral("code_challenge_method"), QStringLiteral("S256"));
	for (auto it = extraParameters.cbegin(); it != extraParameters.cend(); ++it)
		query.addQueryItem(it.key(), it.value());
	url.setQuery(query);
	return url;
}

bool OAuth2PkceSession::matchesState(const QString &returnedState) const
{
	return !returnedState.isEmpty() && returnedState == params.state;
}

QByteArray OAuth2PkceSession::randomBytes(qsizetype length)
{
	QByteArray result(length, Qt::Uninitialized);
	for (qsizetype i = 0; i < length; ++i)
		result[i] = static_cast<char>(QRandomGenerator::system()->generate() & 0xff);
	return result;
}

QString OAuth2PkceSession::base64Url(const QByteArray &data)
{
	return QString::fromLatin1(data.toBase64(QByteArray::Base64UrlEncoding | QByteArray::OmitTrailingEquals));
}
