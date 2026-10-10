// SPDX-License-Identifier: GPL-2.0
#include "cloudsyncprovider.h"

#include <array>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QObject>

namespace {

const std::array<CloudSyncProviderDescriptor, 3> providers = {{
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
		{
			QStringLiteral("account_info.read"),
			QStringLiteral("files.metadata.read"),
			QStringLiteral("files.content.read"),
			QStringLiteral("files.content.write"),
		},
		QStringLiteral("SUBSURFACE_NEO_DROPBOX_CLIENT_ID"),
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

GoogleDesktopOAuthClient googleDesktopOAuthClient()
{
	GoogleDesktopOAuthClient result;
	const QString path = qEnvironmentVariable("SUBSURFACE_NEO_GOOGLE_CLIENT_FILE");
	if (path.isEmpty()) {
		result.error = QObject::tr("Google Drive desktop OAuth needs a client configuration. Set SUBSURFACE_NEO_GOOGLE_CLIENT_FILE to the downloaded Desktop OAuth client JSON file, then restart Neo.");
		return result;
	}
	QFile file(path);
	if (!file.open(QIODevice::ReadOnly) || file.size() > 65536) {
		result.error = QObject::tr("Could not read the Google Desktop OAuth client configuration.");
		return result;
	}
	QJsonParseError error;
	const QJsonDocument document = QJsonDocument::fromJson(file.read(65537), &error);
	const QJsonObject installed = document.object().value(QStringLiteral("installed")).toObject();
	result.clientId = installed.value(QStringLiteral("client_id")).toString().trimmed();
	result.clientSecret = installed.value(QStringLiteral("client_secret")).toString().trimmed();
	if (error.error != QJsonParseError::NoError || result.clientId.isEmpty() || result.clientSecret.isEmpty()) {
		result = {};
		result.error = QObject::tr("Google OAuth configuration must be a Desktop client JSON with client_id and client_secret.");
	}
	const QString overrideId = qEnvironmentVariable("SUBSURFACE_NEO_GOOGLE_CLIENT_ID").trimmed();
	if (result.error.isEmpty() && !overrideId.isEmpty() && overrideId != result.clientId) {
		result = {};
		result.error = QObject::tr("Google OAuth client ID override does not match the Desktop client JSON.");
	}
	return result;
}
