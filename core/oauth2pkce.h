// SPDX-License-Identifier: GPL-2.0
#ifndef OAUTH2_PKCE_H
#define OAUTH2_PKCE_H

#include <QByteArray>
#include <QMap>
#include <QString>
#include <QStringList>
#include <QUrl>

struct OAuth2PkceParameters {
	QString verifier;
	QString challenge;
	QString state;
};

class OAuth2PkceSession {
public:
	OAuth2PkceSession();

	const OAuth2PkceParameters &parameters() const;
	QUrl authorizationUrl(const QUrl &authorizationEndpoint,
				 const QString &clientId,
				 const QUrl &redirectUri,
				 const QStringList &scopes,
				 const QMap<QString, QString> &extraParameters = {}) const;
	bool matchesState(const QString &returnedState) const;

private:
	static QByteArray randomBytes(qsizetype length);
	static QString base64Url(const QByteArray &data);

	OAuth2PkceParameters params;
};

#endif // OAUTH2_PKCE_H
