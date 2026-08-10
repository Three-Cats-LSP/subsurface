// SPDX-License-Identifier: GPL-2.0
#include "cloudsyncprovider.h"

#include <array>

namespace {

const std::array<CloudSyncProviderDescriptor, 4> providers = {{
	{
		CloudSyncProviderType::SubsurfaceCloud,
		QStringLiteral("subsurface"),
		QStringLiteral("Subsurface Cloud"),
		CloudSyncStorageMode::LegacySubsurface,
		QUrl(),
		QUrl(),
		{},
		QString(),
	},
	{
		CloudSyncProviderType::GoogleDrive,
		QStringLiteral("google-drive"),
		QStringLiteral("Google Drive"),
		CloudSyncStorageMode::PrivateAppFolder,
		QUrl(QStringLiteral("https://accounts.google.com/o/oauth2/v2/auth")),
		QUrl(QStringLiteral("https://oauth2.googleapis.com/token")),
		{QStringLiteral("https://www.googleapis.com/auth/drive.appdata")},
		QStringLiteral("SUBSURFACE_NEO_GOOGLE_CLIENT_ID"),
	},
	{
		CloudSyncProviderType::Dropbox,
		QStringLiteral("dropbox"),
		QStringLiteral("Dropbox"),
		CloudSyncStorageMode::PrivateAppFolder,
		QUrl(QStringLiteral("https://www.dropbox.com/oauth2/authorize")),
		QUrl(QStringLiteral("https://api.dropboxapi.com/oauth2/token")),
		{QStringLiteral("files.content.read"), QStringLiteral("files.content.write")},
		QStringLiteral("SUBSURFACE_NEO_DROPBOX_CLIENT_ID"),
	},
	{
		CloudSyncProviderType::OneDrive,
		QStringLiteral("onedrive"),
		QStringLiteral("OneDrive"),
		CloudSyncStorageMode::PrivateAppFolder,
		QUrl(QStringLiteral("https://login.microsoftonline.com/common/oauth2/v2.0/authorize")),
		QUrl(QStringLiteral("https://login.microsoftonline.com/common/oauth2/v2.0/token")),
		{QStringLiteral("offline_access"), QStringLiteral("Files.ReadWrite.AppFolder")},
		QStringLiteral("SUBSURFACE_NEO_ONEDRIVE_CLIENT_ID"),
	},
}};

} // namespace

const CloudSyncProviderDescriptor &cloudSyncProviderDescriptor(CloudSyncProviderType type)
{
	for (const auto &provider : providers) {
		if (provider.type == type)
			return provider;
	}
	return providers.front();
}

QList<CloudSyncProviderDescriptor> cloudSyncProviderDescriptors()
{
	QList<CloudSyncProviderDescriptor> result;
	result.reserve(static_cast<qsizetype>(providers.size()));
	for (const auto &provider : providers)
		result.append(provider);
	return result;
}
