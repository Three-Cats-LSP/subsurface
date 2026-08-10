// SPDX-License-Identifier: GPL-2.0
#include "cloudsyncmanifest.h"

#include <QCryptographicHash>
#include <QJsonDocument>
#include <QJsonObject>
#include <QUuid>

bool CloudSyncManifest::isValid() const
{
	return schemaVersion == 1 && !revisionId.isEmpty() && !payloadSha256.isEmpty() && createdAtUtc.isValid();
}

QByteArray CloudSyncManifest::toJson() const
{
	QJsonObject object;
	object.insert(QStringLiteral("schema"), schemaVersion);
	object.insert(QStringLiteral("revision"), revisionId);
	object.insert(QStringLiteral("sha256"), payloadSha256);
	object.insert(QStringLiteral("parent_sha256"), parentSha256);
	object.insert(QStringLiteral("created_at"), createdAtUtc.toUTC().toString(Qt::ISODateWithMs));
	return QJsonDocument(object).toJson(QJsonDocument::Compact);
}

CloudSyncManifest CloudSyncManifest::fromJson(const QByteArray &json, QString *error)
{
	CloudSyncManifest manifest;
	QJsonParseError parseError;
	const QJsonDocument document = QJsonDocument::fromJson(json, &parseError);
	if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
		if (error)
			*error = QStringLiteral("Invalid sync manifest JSON.");
		return manifest;
	}
	const QJsonObject object = document.object();
	manifest.schemaVersion = object.value(QStringLiteral("schema")).toInt();
	manifest.revisionId = object.value(QStringLiteral("revision")).toString();
	manifest.payloadSha256 = object.value(QStringLiteral("sha256")).toString();
	manifest.parentSha256 = object.value(QStringLiteral("parent_sha256")).toString();
	manifest.createdAtUtc = QDateTime::fromString(object.value(QStringLiteral("created_at")).toString(), Qt::ISODateWithMs);
	if (!manifest.isValid() && error)
		*error = QStringLiteral("Sync manifest is incomplete or uses an unsupported schema.");
	return manifest;
}

CloudSyncManifest CloudSyncManifest::forPayload(const QByteArray &payload, const QString &parentSha256)
{
	CloudSyncManifest manifest;
	manifest.schemaVersion = 1;
	manifest.revisionId = QUuid::createUuid().toString(QUuid::WithoutBraces);
	manifest.payloadSha256 = sha256(payload);
	manifest.parentSha256 = parentSha256;
	manifest.createdAtUtc = QDateTime::currentDateTimeUtc();
	return manifest;
}

QString CloudSyncManifest::sha256(const QByteArray &payload)
{
	return QString::fromLatin1(QCryptographicHash::hash(payload, QCryptographicHash::Sha256).toHex());
}

CloudSyncRelation compareCloudSyncState(const QString &localSha256,
				       const QString &cloudSha256,
				       const QString &lastSyncedSha256)
{
	if (localSha256.isEmpty() || cloudSha256.isEmpty())
		return CloudSyncRelation::Unknown;
	if (localSha256 == cloudSha256)
		return CloudSyncRelation::Identical;
	if (lastSyncedSha256.isEmpty())
		return CloudSyncRelation::Unknown;

	const bool localChanged = localSha256 != lastSyncedSha256;
	const bool cloudChanged = cloudSha256 != lastSyncedSha256;
	if (localChanged && cloudChanged)
		return CloudSyncRelation::Conflict;
	if (localChanged)
		return CloudSyncRelation::LocalOnlyChanged;
	if (cloudChanged)
		return CloudSyncRelation::CloudOnlyChanged;
	return CloudSyncRelation::Unknown;
}
