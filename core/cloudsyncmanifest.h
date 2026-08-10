// SPDX-License-Identifier: GPL-2.0
#ifndef CLOUD_SYNC_MANIFEST_H
#define CLOUD_SYNC_MANIFEST_H

#include <QByteArray>
#include <QDateTime>
#include <QString>

struct CloudSyncManifest {
	int schemaVersion = 1;
	QString revisionId;
	QString payloadSha256;
	QString parentSha256;
	QDateTime createdAtUtc;

	bool isValid() const;
	QByteArray toJson() const;
	static CloudSyncManifest fromJson(const QByteArray &json, QString *error = nullptr);
	static CloudSyncManifest forPayload(const QByteArray &payload, const QString &parentSha256 = QString());
	static QString sha256(const QByteArray &payload);
};

enum class CloudSyncRelation {
	Identical,
	LocalOnlyChanged,
	CloudOnlyChanged,
	Conflict,
	Unknown,
};

CloudSyncRelation compareCloudSyncState(const QString &localSha256,
				       const QString &cloudSha256,
				       const QString &lastSyncedSha256);

#endif // CLOUD_SYNC_MANIFEST_H
