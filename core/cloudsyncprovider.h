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

// Desktop Google OAuth credentials are loaded locally, never from user settings.
struct GoogleDesktopOAuthClient {
	QString clientId;
	QString clientSecret;
	QString error;
};
GoogleDesktopOAuthClient googleDesktopOAuthClient();

#endif // CLOUD_SYNC_PROVIDER_H
