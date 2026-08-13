// SPDX-License-Identifier: GPL-2.0
#include "cloudsyncmanager.h"
#include "cloudcredentialstore.h"
#include "divelog.h"
#include "file.h"
#include "qthelper.h"
#include "save-xml.h"

#include <QDesktopServices>
#include <QFile>
#include <QHostAddress>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QSettings>
#include <QTemporaryFile>
#include <QTcpServer>
#include <QTcpSocket>
#include <QUrlQuery>

namespace {

constexpr auto GOOGLE_DESKTOP_CLIENT_ID = "1014878739336-vpgn495hlm5lnu0kf5ipp8sm4o91bdnt.apps.googleusercontent.com";
constexpr auto GOOGLE_WEB_CLIENT_ID = "1014878739336-pdnmro56alegmna158grah0tf4mrqjnt.apps.googleusercontent.com";
constexpr auto DROPBOX_CLIENT_ID = "ibporeggf7zjv34";
constexpr quint16 DROPBOX_DESKTOP_CALLBACK_PORT = 53682;
constexpr auto DROPBOX_MOBILE_REDIRECT = "https://threecats-lsp.com/subsurface-neo/oauth/dropbox/callback";
constexpr auto NEO_DIVELOG_FILENAME = "subsurface-neo.xml";
constexpr auto NEO_MANIFEST_FILENAME = "subsurface-neo-sync.json";

QString connectionState(bool configured, bool connected)
{
	if (connected)
		return QStringLiteral("connected");
	return configured ? QStringLiteral("ready") : QStringLiteral("not-configured");
}

QByteArray serializeTokens(const OAuth2TokenSet &tokens)
{
	QJsonObject object;
	object.insert(QStringLiteral("access_token"), tokens.accessToken);
	object.insert(QStringLiteral("refresh_token"), tokens.refreshToken);
	object.insert(QStringLiteral("token_type"), tokens.tokenType);
	object.insert(QStringLiteral("scope"), tokens.scope);
	if (tokens.expiresAt.isValid())
		object.insert(QStringLiteral("expires_at"), tokens.expiresAt.toUTC().toString(Qt::ISODateWithMs));
	return QJsonDocument(object).toJson(QJsonDocument::Compact);
}

OAuth2TokenSet deserializeTokens(const QByteArray &payload)
{
	OAuth2TokenSet tokens;
	const QJsonDocument document = QJsonDocument::fromJson(payload);
	if (!document.isObject())
		return tokens;
	const QJsonObject object = document.object();
	tokens.accessToken = object.value(QStringLiteral("access_token")).toString();
	tokens.refreshToken = object.value(QStringLiteral("refresh_token")).toString();
	tokens.tokenType = object.value(QStringLiteral("token_type")).toString(QStringLiteral("Bearer"));
	tokens.scope = object.value(QStringLiteral("scope")).toString();
	tokens.expiresAt = QDateTime::fromString(object.value(QStringLiteral("expires_at")).toString(), Qt::ISODateWithMs);
	return tokens;
}

} // namespace

CloudSyncManager::CloudSyncManager(QNetworkAccessManager *networkManager, QObject *parent) :
	QObject(parent),
	networkManager(networkManager),
	tokenClient(networkManager, this),
	fileStore(networkManager, this)
{
	Q_ASSERT(networkManager);

	for (const auto &provider : cloudSyncProviderDescriptors()) {
		if (provider.type == CloudSyncProviderType::SubsurfaceCloud)
			continue;
		const OAuth2TokenSet restored = deserializeTokens(CloudCredentialStore::load(provider.id));
		if (restored.hasAccessToken() || restored.canRefresh())
			tokens.insert(provider.id, restored);
	}

	connect(&tokenClient, &OAuth2TokenClient::tokenReceived, this, [this](const OAuth2TokenSet &newTokens) {
		if (pendingTokenContinuation) {
			const QString providerId = refreshProviderId;
			const auto continuation = std::move(pendingTokenContinuation);
			pendingTokenContinuation = {};
			refreshProviderId.clear();
			if (!providerId.isEmpty()) {
				tokens.insert(providerId, newTokens);
				CloudCredentialStore::save(providerId, serializeTokens(newTokens));
				emit providersChanged();
			}
			continuation(newTokens.accessToken);
			return;
		}

		if (activeProviderId.isEmpty())
			return;
		const QString connectedProvider = activeProviderId;
		tokens.insert(connectedProvider, newTokens);
		CloudCredentialStore::save(connectedProvider, serializeTokens(newTokens));
		clearAuthorization();
		emit providersChanged();
		emit providerConnected(connectedProvider);
	});
	connect(&tokenClient, &OAuth2TokenClient::tokenError, this, [this](const QString &message) {
		pendingTokenContinuation = {};
		refreshProviderId.clear();
		setError(message);
		clearAuthorization();
		if (syncInProgress())
			clearSyncOperation();
	});

	connect(&fileStore, &CloudSyncFileStore::uploadFinished, this,
		[this](CloudSyncProviderType type, const QString &fileName) {
			const QString providerId = cloudSyncProviderDescriptor(type).id;
			emit uploadFinished(providerId, fileName);

			if (providerId != syncProviderId)
				return;

			if (fileName == QString::fromLatin1(NEO_DIVELOG_FILENAME) &&
			    (syncOperation == SyncOperation::BackupUploadPayload || syncOperation == SyncOperation::SyncUploadPayload)) {
				syncOperation = syncOperation == SyncOperation::BackupUploadPayload
					? SyncOperation::BackupUploadManifest : SyncOperation::SyncUploadManifest;
				uploadBytes(providerId, QString::fromLatin1(NEO_MANIFEST_FILENAME), syncUploadManifest.toJson());
				return;
			}

			if (fileName == QString::fromLatin1(NEO_MANIFEST_FILENAME) &&
			    (syncOperation == SyncOperation::BackupUploadManifest || syncOperation == SyncOperation::SyncUploadManifest)) {
				const bool backupOnly = syncOperation == SyncOperation::BackupUploadManifest;
				saveLastSyncManifest(providerId, syncUploadManifest);
				clearSyncOperation();
				if (backupOnly)
					emit diveLogBackupFinished(providerId);
				else
					emit diveLogSyncFinished(providerId, QStringLiteral("uploaded"));
			}
		});

	connect(&fileStore, &CloudSyncFileStore::downloadFinished, this,
		[this](CloudSyncProviderType type, const QString &fileName, const QByteArray &data) {
			const QString providerId = cloudSyncProviderDescriptor(type).id;
			emit downloadFinished(providerId, fileName, data);
			if (providerId != syncProviderId)
				return;
			if (fileName == QString::fromLatin1(NEO_MANIFEST_FILENAME) && syncOperation == SyncOperation::SyncDownloadManifest)
				handleDownloadedManifest(providerId, data);
			else if (fileName == QString::fromLatin1(NEO_DIVELOG_FILENAME) && syncOperation == SyncOperation::SyncDownloadPayload)
				handleDownloadedDiveLog(providerId, data);
		});

	connect(&fileStore, &CloudSyncFileStore::operationError, this,
		[this](CloudSyncProviderType type, const QString &fileName, const QString &message) {
			const QString providerId = cloudSyncProviderDescriptor(type).id;
			if (!forceCloudDownload && providerId == syncProviderId && syncOperation == SyncOperation::SyncDownloadManifest &&
			    fileName == QString::fromLatin1(NEO_MANIFEST_FILENAME) && isNotFoundError(message)) {
				startUploadSequence(providerId, syncLocalPayload, QString(), false);
				return;
			}
			setError(message);
			if (providerId == syncProviderId)
				clearSyncOperation();
		});
}

CloudSyncManager::~CloudSyncManager() = default;

QVariantList CloudSyncManager::providers() const
{
	const QString primary = primaryProviderId();
	const QString backup = backupProviderId();
	QVariantList result;
	for (const auto &provider : cloudSyncProviderDescriptors()) {
		if (provider.type == CloudSyncProviderType::SubsurfaceCloud)
			continue;
		const QString clientId = configuredClientId(provider);
		const OAuth2TokenSet tokenSet = tokens.value(provider.id);
		QVariantMap row;
		row.insert(QStringLiteral("id"), provider.id);
		row.insert(QStringLiteral("name"), provider.displayName);
		row.insert(QStringLiteral("configured"), !clientId.isEmpty());
		row.insert(QStringLiteral("connected"), tokenSet.hasAccessToken() || tokenSet.canRefresh());
		row.insert(QStringLiteral("state"), connectionState(!clientId.isEmpty(), tokenSet.hasAccessToken() || tokenSet.canRefresh()));
		row.insert(QStringLiteral("scope"), provider.scopes.join(QLatin1Char(' ')));
		row.insert(QStringLiteral("primary"), provider.id == primary);
		row.insert(QStringLiteral("backup"), provider.id == backup);
		result.append(row);
	}
	return result;
}

QString CloudSyncManager::primaryProviderId() const
{
	QSettings settings;
	return settings.value(QStringLiteral("subsurface-neo/cloud/primary-provider")).toString();
}

QString CloudSyncManager::backupProviderId() const
{
	QSettings settings;
	return settings.value(QStringLiteral("subsurface-neo/cloud/backup-provider")).toString();
}

void CloudSyncManager::setPrimaryProvider(const QString &providerId)
{
	if (!providerId.isEmpty() && !descriptorForId(providerId)) {
		setError(tr("Unknown cloud provider."));
		return;
	}
	QSettings settings;
	settings.setValue(QStringLiteral("subsurface-neo/cloud/primary-provider"), providerId);
	if (!providerId.isEmpty() && providerId == backupProviderId())
		settings.remove(QStringLiteral("subsurface-neo/cloud/backup-provider"));
	emit providerAssignmentsChanged();
	emit providersChanged();
}

void CloudSyncManager::setBackupProvider(const QString &providerId)
{
	if (!providerId.isEmpty() && !descriptorForId(providerId)) {
		setError(tr("Unknown cloud provider."));
		return;
	}
	QSettings settings;
	if (providerId.isEmpty() || providerId == primaryProviderId())
		settings.remove(QStringLiteral("subsurface-neo/cloud/backup-provider"));
	else
		settings.setValue(QStringLiteral("subsurface-neo/cloud/backup-provider"), providerId);
	emit providerAssignmentsChanged();
	emit providersChanged();
}

const CloudSyncProviderDescriptor *CloudSyncManager::descriptorForId(const QString &providerId) const
{
	for (const auto &provider : cloudSyncProviderDescriptors()) {
		if (provider.id == providerId)
			return &cloudSyncProviderDescriptor(provider.type);
	}
	return nullptr;
}

QString CloudSyncManager::configuredClientId(const CloudSyncProviderDescriptor &provider) const
{
	if (!provider.clientIdEnvironmentVariable.isEmpty()) {
		const QString overrideId = qEnvironmentVariable(provider.clientIdEnvironmentVariable.toUtf8().constData()).trimmed();
		if (!overrideId.isEmpty())
			return overrideId;
	}

	switch (provider.type) {
	case CloudSyncProviderType::GoogleDrive:
#if defined(Q_OS_ANDROID) || defined(Q_OS_IOS)
		return QString();
#elif defined(__EMSCRIPTEN__)
		return QString::fromLatin1(GOOGLE_WEB_CLIENT_ID);
#else
		return QString::fromLatin1(GOOGLE_DESKTOP_CLIENT_ID);
#endif
	case CloudSyncProviderType::Dropbox:
		return QString::fromLatin1(DROPBOX_CLIENT_ID);
	case CloudSyncProviderType::SubsurfaceCloud:
		return QString();
	}
	return QString();
}

void CloudSyncManager::setError(const QString &message)
{
	if (errorText == message)
		return;
	errorText = message;
	emit lastErrorChanged();
}

void CloudSyncManager::clearAuthorization()
{
	const bool wasActive = authorizationInProgress();
	activeProviderId.clear();
	activeClientId.clear();
	activeRedirectUri.clear();
	activePkce.reset();
	if (loopbackServer) {
		loopbackServer->close();
		loopbackServer->deleteLater();
		loopbackServer = nullptr;
	}
	if (wasActive)
		emit authorizationInProgressChanged();
}

QUrl CloudSyncManager::startLoopbackListener(const CloudSyncProviderDescriptor &provider)
{
#if defined(Q_OS_ANDROID) || defined(Q_OS_IOS)
	if (provider.type == CloudSyncProviderType::Dropbox)
		return QUrl(QString::fromLatin1(DROPBOX_MOBILE_REDIRECT));
	return QUrl(QStringLiteral("subsurface-neo://oauth/callback"));
#else
	QTcpServer *server = new QTcpServer(this);
	const quint16 requestedPort = provider.type == CloudSyncProviderType::Dropbox ? DROPBOX_DESKTOP_CALLBACK_PORT : 0;
	if (!server->listen(QHostAddress::LocalHost, requestedPort)) {
		server->deleteLater();
		return QUrl();
	}
	loopbackServer = server;
	connect(server, &QTcpServer::newConnection, this, &CloudSyncManager::handleLoopbackConnection);
	return QUrl(QStringLiteral("http://127.0.0.1:%1/oauth/callback").arg(server->serverPort()));
#endif
}

void CloudSyncManager::beginAuthorization(const QString &providerId)
{
	const CloudSyncProviderDescriptor *provider = descriptorForId(providerId);
	if (!provider || provider->type == CloudSyncProviderType::SubsurfaceCloud) {
		setError(tr("Unknown OAuth cloud provider."));
		return;
	}

	const QString clientId = configuredClientId(*provider);
	if (clientId.isEmpty()) {
		setError(tr("%1 OAuth client is not available on this platform yet.").arg(provider->displayName));
		return;
	}

	clearAuthorization();
	setError(QString());
	activeProviderId = providerId;
	activeClientId = clientId;
	activePkce = std::make_unique<OAuth2PkceSession>();
	activeRedirectUri = startLoopbackListener(*provider);
	if (!activeRedirectUri.isValid() || activeRedirectUri.isEmpty()) {
		setError(provider->type == CloudSyncProviderType::Dropbox
			? tr("Dropbox OAuth callback port %1 is already in use.").arg(DROPBOX_DESKTOP_CALLBACK_PORT)
			: tr("Could not start the OAuth callback listener."));
		clearAuthorization();
		return;
	}

	QMap<QString, QString> extra;
	if (provider->type == CloudSyncProviderType::GoogleDrive) {
		extra.insert(QStringLiteral("access_type"), QStringLiteral("offline"));
		extra.insert(QStringLiteral("prompt"), QStringLiteral("consent"));
	} else if (provider->type == CloudSyncProviderType::Dropbox) {
		extra.insert(QStringLiteral("token_access_type"), QStringLiteral("offline"));
	}

	const QUrl url = activePkce->authorizationUrl(provider->authorizationEndpoint, clientId,
		activeRedirectUri, provider->scopes, extra);
	emit authorizationInProgressChanged();
	emit authorizationUrlOpened(url);
	if (!QDesktopServices::openUrl(url)) {
		setError(tr("Could not open the system browser for OAuth authorization."));
		clearAuthorization();
	}
}

void CloudSyncManager::handleAuthorizationRedirect(const QUrl &url)
{
	if (!authorizationInProgress() || !activePkce) {
		setError(tr("Received an OAuth callback without an active authorization request."));
		return;
	}

	const QUrlQuery query(url);
	const QString returnedState = query.queryItemValue(QStringLiteral("state"));
	if (!activePkce->matchesState(returnedState)) {
		setError(tr("OAuth callback state did not match the authorization request."));
		clearAuthorization();
		return;
	}

	const QString oauthError = query.queryItemValue(QStringLiteral("error"));
	if (!oauthError.isEmpty()) {
		QString description = query.queryItemValue(QStringLiteral("error_description"));
		if (description.isEmpty())
			description = oauthError;
		setError(description);
		clearAuthorization();
		return;
	}

	const QString code = query.queryItemValue(QStringLiteral("code"));
	if (code.isEmpty()) {
		setError(tr("OAuth callback did not contain an authorization code."));
		clearAuthorization();
		return;
	}
	exchangeCode(code);
}

void CloudSyncManager::exchangeCode(const QString &code)
{
	const CloudSyncProviderDescriptor *provider = descriptorForId(activeProviderId);
	if (!provider || !activePkce)
		return;
	tokenClient.exchangeAuthorizationCode(*provider, activeClientId, code,
		activePkce->parameters().verifier, activeRedirectUri);
}

void CloudSyncManager::handleLoopbackConnection()
{
	if (!loopbackServer)
		return;
	while (QTcpSocket *socket = loopbackServer->nextPendingConnection()) {
		connect(socket, &QTcpSocket::readyRead, this, [this, socket]() {
			const QByteArray request = socket->readAll();
			const int lineEnd = request.indexOf("\r\n");
			const QList<QByteArray> parts = request.left(lineEnd).split(' ');
			if (parts.size() < 2) {
				finishLoopbackSocket(socket, false, tr("Invalid OAuth callback request."));
				return;
			}
			QUrl callback(QStringLiteral("http://127.0.0.1") + QString::fromUtf8(parts.at(1)));
			const QUrlQuery query(callback);
			const bool stateMatches = activePkce && activePkce->matchesState(query.queryItemValue(QStringLiteral("state")));
			const bool hasCode = !query.queryItemValue(QStringLiteral("code")).isEmpty();
			finishLoopbackSocket(socket, stateMatches && hasCode,
				stateMatches && hasCode ? tr("Authorization complete. You can return to Subsurface Neo.")
							 : tr("Authorization failed. Return to Subsurface Neo for details."));
			handleAuthorizationRedirect(callback);
		});
	}
}

void CloudSyncManager::finishLoopbackSocket(QTcpSocket *socket, bool success, const QString &message)
{
	const QByteArray body = QStringLiteral("<!doctype html><html><body><h2>%1</h2><p>%2</p></body></html>")
		.arg(success ? tr("Subsurface Neo connected") : tr("Subsurface Neo authorization error"), message)
		.toUtf8();
	QByteArray response = "HTTP/1.1 200 OK\r\nContent-Type: text/html; charset=utf-8\r\nConnection: close\r\nContent-Length: ";
	response += QByteArray::number(body.size()) + "\r\n\r\n" + body;
	socket->write(response);
	socket->disconnectFromHost();
}

void CloudSyncManager::disconnectProvider(const QString &providerId)
{
	const bool hadToken = tokens.remove(providerId) > 0;
	const bool wasAssigned = providerId == primaryProviderId() || providerId == backupProviderId();
	if (wasAssigned) {
		QSettings settings;
		if (providerId == primaryProviderId())
			settings.remove(QStringLiteral("subsurface-neo/cloud/primary-provider"));
		if (providerId == backupProviderId())
			settings.remove(QStringLiteral("subsurface-neo/cloud/backup-provider"));
	}
	CloudCredentialStore::remove(providerId);
	CloudCredentialStore::remove(syncStateCredentialKey(providerId));
	if (providerId == syncProviderId)
		clearSyncOperation();
	if (hadToken) {
		emit providersChanged();
		emit providerDisconnected(providerId);
	}
	if (wasAssigned) {
		if (!hadToken)
			emit providersChanged();
		emit providerAssignmentsChanged();
	}
}

void CloudSyncManager::refreshThen(const CloudSyncProviderDescriptor &provider, const QString &providerId,
					   const std::function<void(const QString &)> &continuation)
{
	const OAuth2TokenSet current = tokens.value(providerId);
	if (!current.hasAccessToken() && !current.canRefresh()) {
		setError(tr("%1 is not connected.").arg(provider.displayName));
		return;
	}
	if (current.hasAccessToken() && !current.isExpired()) {
		continuation(current.accessToken);
		return;
	}
	if (!current.canRefresh()) {
		setError(tr("%1 authorization expired. Please reconnect it.").arg(provider.displayName));
		return;
	}
	const QString clientId = configuredClientId(provider);
	if (clientId.isEmpty()) {
		setError(tr("%1 OAuth client is not available on this platform.").arg(provider.displayName));
		return;
	}
	refreshProviderId = providerId;
	pendingTokenContinuation = continuation;
	tokenClient.refreshAccessToken(provider, clientId, current.refreshToken);
}

void CloudSyncManager::uploadBytes(const QString &providerId, const QString &fileName, const QByteArray &data)
{
	const CloudSyncProviderDescriptor *provider = descriptorForId(providerId);
	if (!provider)
		return;
	refreshThen(*provider, providerId, [this, type = provider->type, fileName, data](const QString &accessToken) {
		fileStore.upload(type, accessToken, fileName, data);
	});
}

void CloudSyncManager::downloadBytes(const QString &providerId, const QString &fileName)
{
	const CloudSyncProviderDescriptor *provider = descriptorForId(providerId);
	if (!provider)
		return;
	refreshThen(*provider, providerId, [this, type = provider->type, fileName](const QString &accessToken) {
		fileStore.download(type, accessToken, fileName);
	});
}

QByteArray CloudSyncManager::serializeCurrentDiveLog()
{
	QTemporaryFile temporaryFile;
	if (!temporaryFile.open()) {
		setError(tr("Could not create a temporary dive-log file."));
		return {};
	}
	const QString fileName = temporaryFile.fileName();
	temporaryFile.close();

	if (save_dives(fileName.toUtf8().constData()) != 0) {
		setError(tr("Could not serialize the current dive log."));
		return {};
	}

	QFile file(fileName);
	if (!file.open(QIODevice::ReadOnly)) {
		setError(tr("Could not read the serialized dive log."));
		return {};
	}
	const QByteArray payload = file.readAll();
	if (payload.isEmpty()) {
		setError(tr("The serialized dive log is empty."));
		return {};
	}
	return payload;
}

bool CloudSyncManager::applyCloudDiveLog(const QByteArray &payload)
{
	if (payload.isEmpty()) {
		setError(tr("The downloaded dive log is empty."));
		return false;
	}

	QTemporaryFile temporaryFile;
	if (!temporaryFile.open()) {
		setError(tr("Could not create a temporary file for the cloud dive log."));
		return false;
	}
	if (temporaryFile.write(payload) != payload.size() || !temporaryFile.flush()) {
		setError(tr("Could not stage the downloaded dive log."));
		return false;
	}
	const QString fileName = temporaryFile.fileName();
	temporaryFile.close();

	struct divelog replacement;
	if (parse_file(fileName.toUtf8().constData(), &replacement) != 0) {
		setError(tr("The downloaded cloud dive log could not be parsed."));
		return false;
	}
	replacement.process_loaded_dives();
	::divelog = std::move(replacement);
	emit_reset_signal();
	return true;
}

QString CloudSyncManager::syncStateCredentialKey(const QString &providerId)
{
	return QStringLiteral("sync-state-%1").arg(providerId);
}

CloudSyncManifest CloudSyncManager::lastSyncManifest(const QString &providerId) const
{
	QString error;
	const CloudSyncManifest manifest = CloudSyncManifest::fromJson(
		CloudCredentialStore::load(syncStateCredentialKey(providerId)), &error);
	return manifest.isValid() ? manifest : CloudSyncManifest();
}

void CloudSyncManager::saveLastSyncManifest(const QString &providerId, const CloudSyncManifest &manifest)
{
	if (manifest.isValid())
		CloudCredentialStore::save(syncStateCredentialKey(providerId), manifest.toJson());
}

bool CloudSyncManager::isNotFoundError(const QString &message)
{
	const QString lower = message.toLower();
	return lower.contains(QStringLiteral("not found")) || lower.contains(QStringLiteral("not_found")) ||
	       lower.contains(QStringLiteral("path/not_found"));
}

void CloudSyncManager::clearSyncOperation()
{
	const bool wasActive = syncInProgress();
	syncOperation = SyncOperation::None;
	syncProviderId.clear();
	syncLocalPayload.clear();
	syncLocalSha256.clear();
	syncRemoteManifest = CloudSyncManifest();
	syncUploadManifest = CloudSyncManifest();
	forceCloudDownload = false;
	if (wasActive)
		emit syncInProgressChanged();
}

void CloudSyncManager::startUploadSequence(const QString &providerId, const QByteArray &payload,
					   const QString &parentSha256, bool backupOnly)
{
	if (payload.isEmpty()) {
		setError(tr("Cannot upload an empty dive log."));
		clearSyncOperation();
		return;
	}

	syncProviderId = providerId;
	syncLocalPayload = payload;
	syncLocalSha256 = CloudSyncManifest::sha256(payload);
	syncUploadManifest = CloudSyncManifest::forPayload(payload, parentSha256);
	syncOperation = backupOnly ? SyncOperation::BackupUploadPayload : SyncOperation::SyncUploadPayload;
	uploadBytes(providerId, QString::fromLatin1(NEO_DIVELOG_FILENAME), payload);
}

void CloudSyncManager::backupDiveLog(const QString &providerId)
{
	if (!descriptorForId(providerId)) {
		setError(tr("Unknown cloud provider."));
		return;
	}
	if (syncInProgress()) {
		setError(tr("Another cloud operation is already in progress."));
		return;
	}

	const QByteArray payload = serializeCurrentDiveLog();
	if (payload.isEmpty())
		return;

	setError(QString());
	const CloudSyncManifest previous = lastSyncManifest(providerId);
	syncProviderId = providerId;
	emit syncInProgressChanged();
	startUploadSequence(providerId, payload, previous.payloadSha256, true);
}

void CloudSyncManager::syncDiveLog(const QString &providerId)
{
	if (!descriptorForId(providerId)) {
		setError(tr("Unknown cloud provider."));
		return;
	}
	if (syncInProgress()) {
		setError(tr("Another cloud operation is already in progress."));
		return;
	}

	const QByteArray payload = serializeCurrentDiveLog();
	if (payload.isEmpty())
		return;

	setError(QString());
	forceCloudDownload = false;
	syncProviderId = providerId;
	syncLocalPayload = payload;
	syncLocalSha256 = CloudSyncManifest::sha256(payload);
	syncOperation = SyncOperation::SyncDownloadManifest;
	emit syncInProgressChanged();
	downloadBytes(providerId, QString::fromLatin1(NEO_MANIFEST_FILENAME));
}

void CloudSyncManager::useCloudDiveLog(const QString &providerId)
{
	if (!descriptorForId(providerId)) {
		setError(tr("Unknown cloud provider."));
		return;
	}
	if (syncInProgress()) {
		setError(tr("Another cloud operation is already in progress."));
		return;
	}

	setError(QString());
	forceCloudDownload = true;
	syncProviderId = providerId;
	syncOperation = SyncOperation::SyncDownloadManifest;
	emit syncInProgressChanged();
	downloadBytes(providerId, QString::fromLatin1(NEO_MANIFEST_FILENAME));
}

void CloudSyncManager::handleDownloadedManifest(const QString &providerId, const QByteArray &data)
{
	QString error;
	const CloudSyncManifest remote = CloudSyncManifest::fromJson(data, &error);
	if (!remote.isValid()) {
		setError(error.isEmpty() ? tr("The cloud sync manifest is invalid.") : error);
		clearSyncOperation();
		return;
	}

	syncRemoteManifest = remote;
	if (forceCloudDownload) {
		syncOperation = SyncOperation::SyncDownloadPayload;
		downloadBytes(providerId, QString::fromLatin1(NEO_DIVELOG_FILENAME));
		return;
	}

	const CloudSyncManifest previous = lastSyncManifest(providerId);
	const CloudSyncRelation relation = compareCloudSyncState(syncLocalSha256, remote.payloadSha256,
		previous.isValid() ? previous.payloadSha256 : QString());

	switch (relation) {
	case CloudSyncRelation::Identical:
		saveLastSyncManifest(providerId, remote);
		clearSyncOperation();
		emit diveLogSyncFinished(providerId, QStringLiteral("up-to-date"));
		break;
	case CloudSyncRelation::LocalOnlyChanged:
		startUploadSequence(providerId, syncLocalPayload, remote.payloadSha256, false);
		break;
	case CloudSyncRelation::CloudOnlyChanged:
		syncOperation = SyncOperation::SyncDownloadPayload;
		downloadBytes(providerId, QString::fromLatin1(NEO_DIVELOG_FILENAME));
		break;
	case CloudSyncRelation::Conflict:
		clearSyncOperation();
		emit diveLogSyncConflict(providerId);
		break;
	case CloudSyncRelation::Unknown:
		clearSyncOperation();
		emit diveLogInitialChoiceRequired(providerId);
		break;
	}
}

void CloudSyncManager::handleDownloadedDiveLog(const QString &providerId, const QByteArray &data)
{
	if (!syncRemoteManifest.isValid()) {
		setError(tr("Cloud dive-log payload arrived without a valid sync manifest."));
		clearSyncOperation();
		return;
	}

	const QString downloadedSha = CloudSyncManifest::sha256(data);
	if (downloadedSha != syncRemoteManifest.payloadSha256) {
		setError(tr("Cloud dive-log checksum does not match its sync manifest."));
		clearSyncOperation();
		return;
	}

	if (!applyCloudDiveLog(data)) {
		clearSyncOperation();
		return;
	}

	const CloudSyncManifest appliedManifest = syncRemoteManifest;
	saveLastSyncManifest(providerId, appliedManifest);
	clearSyncOperation();
	emit diveLogSyncFinished(providerId, QStringLiteral("downloaded"));
}
