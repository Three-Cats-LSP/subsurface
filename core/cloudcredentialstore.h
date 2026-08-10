// SPDX-License-Identifier: GPL-2.0
#ifndef CLOUD_CREDENTIAL_STORE_H
#define CLOUD_CREDENTIAL_STORE_H

#include <QByteArray>
#include <QString>

class CloudCredentialStore {
public:
	static QByteArray load(const QString &providerId);
	static bool save(const QString &providerId, const QByteArray &payload);
	static bool remove(const QString &providerId);
};

#endif // CLOUD_CREDENTIAL_STORE_H
