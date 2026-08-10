// SPDX-License-Identifier: GPL-2.0
#include "cloudsyncmanager.h"

#include <QDesktopServices>
#include <QHostAddress>
#include <QNetworkAccessManager>
#include <QTcpServer>
#include <QTcpSocket>
#include <QUrlQuery>

namespace {

QString connectionState(bool configured, bool connected)
{
	if (connected)
		return QStringLiteral("connected");
	return configured ? QStringLiteral("ready") : QStringLiteral("not-configured");
}

} // namespace

CloudSyncManager::CloudSyncManager(QNetworkAccessManager *networkManager, QObject *parent) :
	QObject(parent),
	networkManager(networkManager),
	tokenClient(networkManager, this),
	fileStore(networkManager, this)
{
	Q_ASSERT(networkManager);

	connect(&tokenClient, &OAuth2TokenClient::tokenReceived, this, [this](const OAuth2TokenSet &newTokens) {
		if (!pendingTokenContinuation) {
			if (activeProviderId.isEmpty())
				return;
			tokens.insert(activeProviderId, newTokens);
			const QString connectedProvider = activeProviderId;
			clearAuthorization();
			emit providersChanged();
			emit providerConnected(connectedProvider);
			return;
		}

		const auto continuation = std::move(pendingTokenContinuation);
		pendingTokenContinuation = {};
		OAuth2TokenSet merged = newTokens;
		if (!activeProviderId.isEmpty())
			tokens.insert(activeProviderId, merged);
		continuation(merged.accessToken);
	});
	connect(&tokenClient, &OAuth2TokenClient::tokenError, this, [this](const QString &message) {
		pendingTokenContinuation = {};
		setError(message);
		clearAuthorization();
	});

	connect(&fileStore, &CloudSyncFileStore::uploadFinished, this,
		[this](CloudSyncProviderType type, const QString &fileName) {
			emit uploadFinished(cloudSyncProviderDescriptor(type).id, fileName);
		});
	connect(&fileStore, &CloudSyncFileStore::downloadFinished, this,
		[this](CloudSyncProviderType type, const QString &fileName, const QByteArray &data) {
			emit downloadFinished(cloudSyncProviderDescriptor(type).id, fileName, data);
		});
	connect(&fileStore, &CloudSyncFileStore::operationError, this,
		[this](CloudSyncProviderType, const QString &, const QString &message) { setError(message); });
}

CloudSyncManager::~CloudSyncManager() = default;

QVariantList CloudSyncManager::providers() const
{
	QVariantList result;
	for (const auto &provider : cloudSyncProviderDescriptors()) {
		if (provider.type == CloudSyncProviderType::SubsurfaceCloud)
			continue;
		const QString clientId = configuredClientId(provider);
		QVariantMap row;
		row.insert(QStringLiteral("id"), provider.id);
		row.insert(QStringLiteral("name"), provider.displayName);
		row.insert(QStringLiteral("configured"), !clientId.isEmpty());
		row.insert(QStringLiteral("connected"), tokens.value(provider.id).hasAccessToken());
		row.insert(QStringLiteral("state"), connectionState(!clientId.isEmpty(), tokens.value(provider.id).hasAccessToken()));
		row.insert(QStringLiteral("scope"), provider.scopes.join(QLatin1Char(' ')));
		result.append(row);
	}
	return result;
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
	if (provider.clientIdEnvironmentVariable.isEmpty())
		return QString();
	return qEnvironmentVariable(provider.clientIdEnvironmentVariable.toUtf8().constData()).trimmed();
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

QUrl CloudSyncManager::startLoopbackListener()
{
#if defined(Q_OS_ANDROID) || defined(Q_OS_IOS)
	return QUrl(QStringLiteral("subsurface-neo://oauth/callback"));
#else
	QTcpServer *server = new QTcpServer(this);
	if (!server->listen(QHostAddress::LocalHost, 0)) {
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
		setError(tr("%1 OAuth client ID is not configured (%2).")
			 .arg(provider->displayName, provider->clientIdEnvironmentVariable));
		return;
	}

	clearAuthorization();
	setError(QString());
	activeProviderId = providerId;
	activeClientId = clientId;
	activePkce = std::make_unique<OAuth2PkceSession>();
	activeRedirectUri = startLoopbackListener();
	if (!activeRedirectUri.isValid() || activeRedirectUri.isEmpty()) {
		setError(tr("Could not start the OAuth callback listener."));
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
	if (tokens.remove(providerId) > 0) {
		emit providersChanged();
		emit providerDisconnected(providerId);
	}
}

void CloudSyncManager::refreshThen(const CloudSyncProviderDescriptor &provider, const QString &providerId,
					   const std::function<void(const QString &)> &continuation)
{
	const OAuth2TokenSet current = tokens.value(providerId);
	if (!current.hasAccessToken()) {
		setError(tr("%1 is not connected.").arg(provider.displayName));
		return;
	}
	if (!current.isExpired()) {
		continuation(current.accessToken);
		return;
	}
	if (!current.canRefresh()) {
		setError(tr("%1 authorization expired. Please reconnect it.").arg(provider.displayName));
		return;
	}
	const QString clientId = configuredClientId(provider);
	if (clientId.isEmpty()) {
		setError(tr("%1 OAuth client ID is not configured.").arg(provider.displayName));
		return;
	}
	activeProviderId = providerId;
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
