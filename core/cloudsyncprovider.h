// SPDX-License-Identifier: GPL-2.0
#ifndef CLOUD_SYNC_PROVIDER_H
#define CLOUD_SYNC_PROVIDER_H

#include <QString>
#include <QStringList>
#include <QUrl>

enum class CloudSyncProviderType {
	SubsurfaceCloud,
	GoogleDrive,
	Dropbox,
	OneDrive,
};

enum class CloudSyncStorageMode {
	LegacySubsurface,
	PrivateAppFolder,
};

struct CloudSyncProviderDescriptor {
	CloudSyncProviderType type;
	QString id;
	QString displayName;
	CloudSyncStorageMode storageMode;
	QUrl authorizationEndpoint;
	QUrl tokenEndpoint;
	QStringList scopes;
	QString clientIdEnvironmentVariable;
};

const CloudSyncProviderDescriptor &cloudSyncProviderDescriptor(CloudSyncProviderType type);
QList<CloudSyncProviderDescriptor> cloudSyncProviderDescriptors();

#endif // CLOUD_SYNC_PROVIDER_H
