// SPDX-License-Identifier: GPL-2.0
#include "cloudsyncmanager.h"
#include "cloudcredentialstore.h"
#include "divelog.h"
#include "file.h"
#include "fulltext.h"
#include "qthelper.h"
#include "save-xml.h"
#include "selection.h"

#include <QDesktopServices>
#include <QFile>
#include <QHostAddress>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QSettings>
#include <QStandardPaths>
#include <QTemporaryFile>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTimer>
#include <QUrlQuery>
#include <QXmlStreamReader>

#if defined(Q_OS_ANDROID)
#include <QJniObject>
#endif

namespace {

constexpr auto GOOGLE_DESKTOP_CLIENT_ID = "1014878739336-vpgn495hlm5lnu0kf5ipp8sm4o91bdnt.apps.googleusercontent.com";
constexpr auto GOOGLE_WEB_CLIENT_ID = "1014878739336-pdnmro56alegmna158grah0tf4mrqjnt.apps.googleusercontent.com";
constexpr auto GOOGLE_ANDROID_CLIENT_ID = "1014878739336-5gm47s7uv32fiqpsdt97rdh17h5lsfn6.apps.googleusercontent.com";
constexpr auto DROPBOX_CLIENT_ID = "ibporeggf7zjv34";
constexpr quint16 DROPBOX_DESKTOP_CALLBACK_PORT = 53682;
constexpr auto DROPBOX_MOBILE_REDIRECT = "subsurface-neo://oauth/callback";
constexpr auto NEO_DIVELOG_FILENAME = "subsurface-neo.xml";
constexpr auto NEO_MANIFEST_FILENAME = "subsurface-neo-sync.json";
constexpr auto AUTO_SYNC_SETTING = "subsurface-neo/cloud/auto-sync";

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
	object.insert(QStringLiteral("account_id"), tokens.accountId);
	object.insert(QStringLiteral("account_name"), tokens.accountName);
	object.insert(QStringLiteral("account_email"), tokens.accountEmail);
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
	tokens.accountId = object.value(QStringLiteral("account_id")).toString();
	tokens.accountName = object.value(QStringLiteral("account_name")).toString();
	tokens.accountEmail = object.value(QStringLiteral("account_email")).toString();
	tokens.expiresAt = QDateTime::fromString(object.value(QStringLiteral("expires_at")).toString(), Qt::ISODateWithMs);
	return tokens;
}

} // namespace

CloudSyncManager::CloudSyncManager(QNetworkAccessManager *networkManager, QObject *parent) :
	QObject(parent),
	networkManager(networkManager),
	tokenClient(networkManager, this),
	fileStore(networkManager, this),
	storageReviewClient(networkManager, this),
	historyClient(networkManager, this),
	workspaceReader(networkManager, this),
	workspaceCreator(networkManager, this),
	workspaceSource(networkManager, this),
	workspaceCopy(networkManager, this),
	workspaceAcceptance(networkManager, this),
	workspaceUpload(networkManager, this),
	workspaceRestoreReader(networkManager, this),
	workspaceRestoreApply(networkManager, this)
{
	Q_ASSERT(networkManager);
	connect(&workspaceReader, &CloudWorkspaceOpen::finished, this, &CloudSyncManager::finishWorkspace);
	connect(&workspaceCreator, &CloudWorkspaceCreate::finished, this, [this](const QJsonObject &root, const QString &version, const QString &) { finishWorkspace(root, version); });
	connect(&workspaceReader, &CloudWorkspaceOpen::failed, this, &CloudSyncManager::failWorkspace);
	connect(&workspaceCreator, &CloudWorkspaceCreate::failed, this, &CloudSyncManager::failWorkspace);
	connect(&workspaceSource, &CloudWorkspaceSourceReview::failed, this, &CloudSyncManager::failWorkspace);
	connect(&workspaceCopy, &CloudWorkspaceCopy::failed, this, &CloudSyncManager::failWorkspace);
	connect(&workspaceAcceptance, &CloudWorkspaceAcceptance::failed, this, &CloudSyncManager::failWorkspace);
	connect(&workspaceAcceptance, &CloudWorkspaceAcceptance::finished, this, [this](const QJsonObject &, const QString &, const QString &event, bool latest, bool changed) {
		if (!workspaceInProgress() || (workspaceAction != "accept-copy" && workspaceAction != "resume-accept")) return;
		const auto provider = syncProviderId;
		workspaceReports[provider] = {{"verified", true}, {"choiceVersion", workspaceChoiceVersion}, {"acceptedLegacy", event}, {"acceptanceLatest", latest}, {"legacyChanged", changed}};
		clearSyncOperation(); emit providersChanged(); emit workspaceFinished(provider);
	});
	connect(&workspaceUpload, &CloudWorkspaceUpload::failed, this, &CloudSyncManager::failWorkspace);
	connect(&workspaceUpload, &CloudWorkspaceUpload::finished, this, [this](const QJsonObject &root, const QString &version, const QString &ticket, bool headMatches) {
		if (workspaceRestoreStep == WorkspaceRestoreStep::Publishing) workspaceRestorePublished(root, version, ticket, headMatches);
		else finishWorkspaceVersions(root, version, ticket, headMatches);
	});
	connect(&workspaceRestoreReader, &CloudWorkspaceRead::finished, this, &CloudSyncManager::workspaceRestoreReadFinished);
	connect(&workspaceRestoreReader, &CloudWorkspaceRead::released, this, &CloudSyncManager::workspaceRestoreReleased);
	connect(&workspaceRestoreReader, &CloudWorkspaceRead::failed, this, &CloudSyncManager::failWorkspace);
	connect(&workspaceRestoreApply, &CloudWorkspaceApply::applied, this, &CloudSyncManager::workspaceRestoreApplied);
	connect(&workspaceRestoreApply, &CloudWorkspaceApply::finished, this, &CloudSyncManager::workspaceRestoreCompleted);
	connect(&workspaceRestoreApply, &CloudWorkspaceApply::failed, this, &CloudSyncManager::workspaceRestoreApplyFailed);
	connect(&workspaceSource, &CloudWorkspaceSourceReview::finished, this, [this](const QJsonObject &source, const QByteArray &) { finishWorkspaceSource(source); });
	connect(&workspaceCopy, &CloudWorkspaceCopy::finished, this, [this](const QJsonObject &root, const QString &version, const QString &, bool changed) {
		workspaceLegacyChanged = changed; finishWorkspace(root, version);
	});
	connect(&historyClient, &CloudSyncHistory::finished, this, [this](const QVariantMap &report, const QByteArray &restored) {
		if (!historyInProgress()) return;
		const QString providerId = syncProviderId;
		if (!restored.isEmpty() && (autoSyncEnabled() || !primaryProviderId().isEmpty() || !backupProviderId().isEmpty())) {
			setError(tr("Provider assignments changed during restore. The new history version was kept; the local log was not replaced."));
			clearSyncOperation(); return;
		}
		if (!restored.isEmpty() && (!localDiveLogUnchanged() || !applyCloudDiveLog(restored))) {
			clearSyncOperation(); return;
		}
		historyReports.insert(providerId, report);
		clearSyncOperation(); emit providersChanged(); emit historyFinished(providerId);
		// Refresh/persist the canonical model through the established download path,
		// but do not advance a schema 1/2 baseline or schedule legacy sync.
		if (!restored.isEmpty()) emit diveLogSyncFinished(providerId, QStringLiteral("downloaded"));
	});
	connect(&historyClient, &CloudSyncHistory::failed, this, [this](const QString &message, bool rejected) {
		if (!historyInProgress()) return;
		if (rejected) fileStore.operationError(descriptorForId(syncProviderId)->type, {}, message, true);
		else { setError(message); clearSyncOperation(); }
	});
	connect(&storageReviewClient, &CloudStorageReview::finished, this, [this](QVariantMap report) {
		if (!storageReviewInProgress()) return;
		const QString providerId = syncProviderId;
		report.insert(QStringLiteral("reviewedAt"), QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs));
		storageReviews.insert(providerId, report);
		clearSyncOperation();
		emit providersChanged();
		emit storageReviewFinished(providerId);
	});
	connect(&storageReviewClient, &CloudStorageReview::failed, this, [this](const QString &message, bool rejected) {
		if (!storageReviewInProgress()) return;
		if (rejected)
			fileStore.operationError(descriptorForId(syncProviderId)->type, QString(), message, true);
		else {
			setError(message);
			clearSyncOperation();
		}
	});

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
				storeUpdatedTokens(providerId, newTokens);
				emit providersChanged();
			}
			verifyAccountThen(providerId, newTokens.accessToken, continuation);
			return;
		}

		if (activeProviderId.isEmpty())
			return;
		const QString connectedProvider = activeProviderId;
		// An older credential has no account ID to compare after a new consent.
		if (tokens.value(connectedProvider).accountId.isEmpty() && !resetAccountSyncState(connectedProvider)) {
			clearAuthorization();
			return;
		}
		storeUpdatedTokens(connectedProvider, newTokens);
		clearAuthorization();
		emit providersChanged();
		verifyAccountThen(connectedProvider, newTokens.accessToken, [this, connectedProvider](const QString &) {
			emit providerConnected(connectedProvider);
		});
	});
	connect(&tokenClient, &OAuth2TokenClient::tokenError, this, [this](const QString &message, bool reconnectRequired) {
		const QString providerId = refreshProviderId;
		pendingTokenContinuation = {};
		refreshProviderId.clear();
		if (reconnectRequired && !providerId.isEmpty()) {
			// A new authorization may select a different account. Discard the old
			// baseline and assignments so it cannot silently resume automatic sync.
			disconnectProvider(providerId);
			setError(tr("%1 authorization expired or was revoked. Please connect it again.")
				.arg(descriptorForId(providerId)->displayName));
		} else {
			setError(message);
		}
		clearAuthorization();
		if (syncInProgress())
			clearSyncOperation();
	});

	connect(&fileStore, &CloudSyncFileStore::uploadFinished, this,
		[this](CloudSyncProviderType type, const QString &fileName) {
			const QString providerId = cloudSyncProviderDescriptor(type).id;
			if (providerId == syncProviderId && syncOperation == SyncOperation::RawProtectedUpload && fileName == rawProtectedName) {
				clearSyncOperation(); emit uploadFinished(providerId, fileName); return;
			}
			emit uploadFinished(providerId, fileName);

			if (providerId != syncProviderId)
				return;

			if (fileName == syncUploadManifest.payloadFileName() &&
			    (syncOperation == SyncOperation::BackupUploadPayload || syncOperation == SyncOperation::SyncUploadPayload)) {
				if (!localDiveLogUnchanged()) {
					clearSyncOperation();
					return;
				}
				syncOperation = syncOperation == SyncOperation::BackupUploadPayload
					? SyncOperation::BackupUploadManifest : SyncOperation::SyncUploadManifest;
				uploadSyncFile(providerId, QString::fromLatin1(NEO_MANIFEST_FILENAME), syncUploadManifest.toJson(), syncRemoteVersion, syncRemoteFileId);
				return;
			}

			if (fileName == QString::fromLatin1(NEO_MANIFEST_FILENAME) &&
			    (syncOperation == SyncOperation::BackupUploadManifest || syncOperation == SyncOperation::SyncUploadManifest)) {
				// Re-read by name: Drive permits simultaneous first creates with
				// duplicate names. Never pick one arbitrarily or report success.
				syncOperation = SyncOperation::SyncVerifyCommit;
				downloadSyncManifest(providerId);
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
			else if (fileName == syncRemoteManifest.payloadFileName() && syncOperation == SyncOperation::SyncDownloadPayload)
				handleDownloadedDiveLog(providerId, data);
			else if (fileName == QString::fromLatin1(NEO_DIVELOG_FILENAME) && syncOperation == SyncOperation::SyncCheckOrphanPayload) {
				setError(tr("A cloud dive log exists without its sync manifest. No data was overwritten. Recover the cloud files before syncing again."));
				clearSyncOperation();
			}
		});

	connect(&fileStore, &CloudSyncFileStore::versionedDownloadFinished, this,
		[this](CloudSyncProviderType type, const QString &fileName, const QByteArray &data,
		       const QString &version, const QString &fileId) {
			const QString providerId = cloudSyncProviderDescriptor(type).id;
			if (providerId != syncProviderId || fileName != QString::fromLatin1(NEO_MANIFEST_FILENAME))
				return;
			syncRemoteVersion = version;
			syncRemoteFileId = fileId;
			if (syncOperation == SyncOperation::SyncVerifyCommit) {
				const auto committed = CloudSyncManifest::fromJson(data);
				const bool sameInitialGoogleLog = type == CloudSyncProviderType::GoogleDrive &&
					syncUploadManifest.parentSha256.isEmpty() && committed.payloadSha256 == syncLocalSha256;
				if (!committed.isValid() || (committed.toJson() != syncUploadManifest.toJson() && !sameInitialGoogleLog)) {
					fileStore.writeConflict(type, fileName);
					return;
				}
				// Recovery may retain another device's identical initial snapshot.
				// Persist the canonical revision, not our archived candidate.
				syncUploadManifest = committed;
				finishUpload(providerId);
			} else if (syncOperation == SyncOperation::SyncDownloadManifest) {
				handleDownloadedManifest(providerId, data);
			}
		});
	connect(&fileStore, &CloudSyncFileStore::writeConflict, this,
		[this](CloudSyncProviderType type, const QString &fileName) {
			const QString providerId = cloudSyncProviderDescriptor(type).id;
			if (type == CloudSyncProviderType::GoogleDrive && providerId == syncProviderId &&
			    fileName == QString::fromLatin1(NEO_MANIFEST_FILENAME) && syncRemoteVersion.isEmpty() &&
			    (syncOperation == SyncOperation::SyncUploadManifest || syncOperation == SyncOperation::BackupUploadManifest)) {
				// A competing first upload may become visible before our create.
				// Compare it through the same recovery/read path as a committed race.
				syncOperation = SyncOperation::SyncVerifyCommit;
				downloadSyncManifest(providerId);
				return;
			}
			setError(tr("The cloud log changed during synchronization. Your local log was kept. Sync again to compare the versions."));
			if (providerId == syncProviderId) {
				clearSyncOperation();
				emit diveLogSyncConflict(providerId);
			}
		});

	connect(&fileStore, &CloudSyncFileStore::fileNotFound, this,
		[this](CloudSyncProviderType type, const QString &fileName) {
			const QString providerId = cloudSyncProviderDescriptor(type).id;
			if (!forceCloudDownload && providerId == syncProviderId && syncOperation == SyncOperation::SyncDownloadManifest &&
			    fileName == QString::fromLatin1(NEO_MANIFEST_FILENAME)) {
				if (localDiveLogUnchanged()) {
					// An interrupted upload may have left a log but no manifest.
					// Confirm both files are absent before creating a baseline.
					syncOperation = SyncOperation::SyncCheckOrphanPayload;
					downloadBytes(providerId, QString::fromLatin1(NEO_DIVELOG_FILENAME));
				} else {
					clearSyncOperation();
				}
				return;
			}
			if (providerId == syncProviderId && syncOperation == SyncOperation::SyncCheckOrphanPayload &&
			    fileName == QString::fromLatin1(NEO_DIVELOG_FILENAME)) {
				if (localDiveLogUnchanged())
					startUploadSequence(providerId, syncLocalPayload, QString(), syncBackupOnly);
				else
					clearSyncOperation();
				return;
			}
			setError(tr("Cloud file was not found. No local data was changed."));
			if (providerId == syncProviderId)
				clearSyncOperation();
		});
	connect(&fileStore, &CloudSyncFileStore::operationError, this,
		[this](CloudSyncProviderType type, const QString &, const QString &message, bool tokenRejected) {
			const QString providerId = cloudSyncProviderDescriptor(type).id;
			if (tokenRejected) {
				OAuth2TokenSet current = tokens.value(providerId);
				if (current.canRefresh()) {
					// The provider's expiry wins over our local clock. Keep the
					// refresh grant so the next explicit/lifecycle sync can renew it.
					current.accessToken.clear();
					tokens.insert(providerId, current);
					CloudCredentialStore::save(providerId, serializeTokens(current));
					emit providersChanged();
					setError(tr("%1 rejected the access token. Sync again to renew authorization.")
						.arg(cloudSyncProviderDescriptor(type).displayName));
				} else {
					disconnectProvider(providerId);
					setError(tr("%1 authorization expired or was revoked. Please connect it again.")
						.arg(cloudSyncProviderDescriptor(type).displayName));
				}
			} else {
				setError(message);
			}
			if (providerId == syncProviderId)
				clearSyncOperation();
		});
}

CloudSyncManager::~CloudSyncManager()
{
	cancelAccountCheck();
}

void CloudSyncManager::setWorkspacePersistenceHandlers(std::function<bool()> available, std::function<bool()> persist)
{
	workspacePersistenceAvailable = std::move(available);
	workspacePersist = std::move(persist);
}

QVariantList CloudSyncManager::providers() const
{
	const QString primary = primaryProviderId();
	const QString backup = backupProviderId();
	QVariantList result;
	for (const auto &provider : cloudSyncProviderDescriptors()) {
		if (provider.type == CloudSyncProviderType::SubsurfaceCloud)
			continue;
		const QString clientId = configuredClientId(provider);
		QString configurationError;
#if !defined(Q_OS_ANDROID) && !defined(Q_OS_IOS) && !defined(__EMSCRIPTEN__)
		if (provider.type == CloudSyncProviderType::GoogleDrive)
			configurationError = googleDesktopOAuthClient().error;
#endif
		const bool configured = !clientId.isEmpty() && configurationError.isEmpty();
		const OAuth2TokenSet tokenSet = tokens.value(provider.id);
		QVariantMap row;
		row.insert(QStringLiteral("id"), provider.id);
		row.insert(QStringLiteral("name"), provider.displayName);
		row.insert(QStringLiteral("accountName"), tokenSet.accountName);
		row.insert(QStringLiteral("accountEmail"), tokenSet.accountEmail);
		row.insert(QStringLiteral("accountVerified"), !tokenSet.accessToken.isEmpty() && verifiedAccountTokens.value(provider.id) == tokenSet.accessToken);
		row.insert(QStringLiteral("checkingAccount"), accountProviderId == provider.id);
		row.insert(QStringLiteral("storageReview"), storageReviews.value(provider.id));
		row.insert(QStringLiteral("history"), historyReports.value(provider.id));
		row.insert(QStringLiteral("workspace"), workspaceReport(provider.id));
		row.insert(QStringLiteral("configured"), configured);
		row.insert(QStringLiteral("configurationError"), configurationError);
		row.insert(QStringLiteral("connected"), tokenSet.hasAccessToken() || tokenSet.canRefresh());
		row.insert(QStringLiteral("state"), connectionState(configured, tokenSet.hasAccessToken() || tokenSet.canRefresh()));
		row.insert(QStringLiteral("scope"), provider.scopes.join(QLatin1Char(' ')));
		row.insert(QStringLiteral("primary"), provider.id == primary);
		row.insert(QStringLiteral("backup"), provider.id == backup);
		row.insert(QStringLiteral("lastSyncAt"), QSettings().value(
			QStringLiteral("subsurface-neo/cloud/last-sync/%1").arg(provider.id)).toString());
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

bool CloudSyncManager::autoSyncEnabled() const
{
	return QSettings().value(QString::fromLatin1(AUTO_SYNC_SETTING), false).toBool();
}

void CloudSyncManager::setAutoSyncEnabled(bool enabled)
{
	if (autoSyncEnabled() == enabled)
		return;
	QSettings().setValue(QString::fromLatin1(AUTO_SYNC_SETTING), enabled);
	emit autoSyncEnabledChanged();
}

bool CloudSyncManager::syncPrimaryIfReady()
{
	if (!autoSyncEnabled() || authorizationInProgress() || syncInProgress() || accountCheckInProgress())
		return false;

	const QString providerId = primaryProviderId();
	if (providerId.isEmpty() || !descriptorForId(providerId) || !lastSyncManifest(providerId).isValid())
		return false;

	// Automatic synchronization must remain silent. In particular, do not
	// launch Android's Google account chooser when its short-lived token has
	// expired; a manual Sync now can renew that authorization explicitly.
	const OAuth2TokenSet current = tokens.value(providerId);
	if ((!current.hasAccessToken() || current.isExpired()) && !current.canRefresh())
		return false;

	syncDiveLog(providerId);
	return syncInProgress();
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
#if !defined(Q_OS_ANDROID) && !defined(Q_OS_IOS) && !defined(__EMSCRIPTEN__)
	if (provider.type == CloudSyncProviderType::GoogleDrive && qEnvironmentVariableIsSet("SUBSURFACE_NEO_GOOGLE_CLIENT_FILE"))
		return googleDesktopOAuthClient().clientId;
#endif
	if (!provider.clientIdEnvironmentVariable.isEmpty()) {
		const QString overrideId = qEnvironmentVariable(provider.clientIdEnvironmentVariable.toUtf8().constData()).trimmed();
		if (!overrideId.isEmpty())
			return overrideId;
	}

	switch (provider.type) {
	case CloudSyncProviderType::GoogleDrive:
#if defined(Q_OS_ANDROID)
		return QString::fromLatin1(GOOGLE_ANDROID_CLIENT_ID);
#elif defined(Q_OS_IOS)
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

void CloudSyncManager::acceptNativeAccessToken(const QString &providerId, const QString &accessToken)
{
	OAuth2TokenSet nativeTokens;
	nativeTokens.accessToken = accessToken;
	nativeTokens.tokenType = QStringLiteral("Bearer");
	nativeTokens.scope = QStringLiteral("https://www.googleapis.com/auth/drive.appdata");
	// Google Identity Services returns an access token but no expiry. Google
	// tokens normally last an hour, so renew before that rather than treating one
	// as a long-lived credential.
	nativeTokens.expiresAt = QDateTime::currentDateTimeUtc().addSecs(50 * 60);

	if (pendingTokenContinuation) {
		const auto continuation = std::move(pendingTokenContinuation);
		pendingTokenContinuation = {};
		refreshProviderId.clear();
		if (tokens.value(providerId).accountId.isEmpty() && !resetAccountSyncState(providerId)) {
			clearSyncOperation();
			return;
		}
		storeUpdatedTokens(providerId, nativeTokens);
		emit providersChanged();
		verifyAccountThen(providerId, nativeTokens.accessToken, continuation);
		return;
	}

	if (activeProviderId != providerId)
		return;
	if (tokens.value(providerId).accountId.isEmpty() && !resetAccountSyncState(providerId)) {
		clearAuthorization();
		return;
	}
	storeUpdatedTokens(providerId, nativeTokens);
	clearAuthorization();
	emit providersChanged();
	verifyAccountThen(providerId, nativeTokens.accessToken, [this, providerId](const QString &) {
		emit providerConnected(providerId);
	});
}

void CloudSyncManager::requestAndroidGoogleAccessToken()
{
#if defined(Q_OS_ANDROID)
	QJniObject::callStaticMethod<void>("org/subsurfacedivelog/mobile/SubsurfaceMobileActivity",
		"authorizeGoogleDrive", "()V");
#else
	setError(tr("Google Drive authorization is not available on this platform."));
	clearAuthorization();
#endif
}

void CloudSyncManager::handleAndroidGoogleAccessToken(const QString &accessToken)
{
	if (accessToken.isEmpty()) {
		handleAndroidGoogleAuthorizationError(tr("Google did not return a Drive access token."));
		return;
	}
	acceptNativeAccessToken(QStringLiteral("google-drive"), accessToken);
}

void CloudSyncManager::handleAndroidGoogleAuthorizationError(const QString &message)
{
	pendingTokenContinuation = {};
	refreshProviderId.clear();
	setError(message.isEmpty() ? tr("Google Drive authorization failed.") : message);
	clearAuthorization();
	if (syncInProgress())
		clearSyncOperation();
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
	if (!activeProviderId.isEmpty())
		tokenClient.cancel();
	const bool wasActive = authorizationInProgress();
	activeProviderId.clear();
	activeClientId.clear();
	codeExchangeInProgress = false;
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
	if (syncInProgress() || pendingTokenContinuation || accountCheckInProgress()) {
		setError(tr("Wait for the current cloud operation to finish before connecting."));
		return;
	}
	const CloudSyncProviderDescriptor *provider = descriptorForId(providerId);
	if (!provider || provider->type == CloudSyncProviderType::SubsurfaceCloud) {
		setError(tr("Unknown OAuth cloud provider."));
		return;
	}

	const QString clientId = configuredClientId(*provider);
#if !defined(Q_OS_ANDROID) && !defined(Q_OS_IOS) && !defined(__EMSCRIPTEN__)
	if (provider->type == CloudSyncProviderType::GoogleDrive) {
		const auto client = googleDesktopOAuthClient();
		if (!client.error.isEmpty()) {
			setError(client.error);
			return;
		}
	}
#endif
	if (clientId.isEmpty()) {
		setError(tr("%1 OAuth client is not available on this platform yet.").arg(provider->displayName));
		return;
	}

	clearAuthorization();
	setError(QString());
	activeProviderId = providerId;
	activeClientId = clientId;
#if defined(Q_OS_ANDROID)
	if (provider->type == CloudSyncProviderType::GoogleDrive) {
		emit authorizationInProgressChanged();
		requestAndroidGoogleAccessToken();
		return;
	}
#endif
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
	// A browser can repeat the redirect while the first token exchange is pending.
	if (codeExchangeInProgress)
		return;
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
	codeExchangeInProgress = true;
	tokenClient.exchangeAuthorizationCode(*provider, activeClientId, code,
		activePkce->parameters().verifier, activeRedirectUri);
}

void CloudSyncManager::handleLoopbackConnection()
{
	if (!loopbackServer)
		return;
	while (QTcpSocket *socket = loopbackServer->nextPendingConnection()) {
		connect(socket, &QTcpSocket::disconnected, socket, &QObject::deleteLater);
		QTimer::singleShot(15000, socket, [socket] { socket->abort(); socket->deleteLater(); });
		socket->setReadBufferSize(16385);
		connect(socket, &QTcpSocket::readyRead, this, [this, socket, request = QByteArray(), handled = false]() mutable {
			if (handled)
				return;
			request += socket->readAll();
			if (request.size() > 16384) {
				handled = true;
				finishLoopbackSocket(socket, false, tr("Invalid OAuth callback request."));
				return;
			}
			// TCP may deliver the request line and headers in separate reads.
			if (!request.contains("\r\n\r\n"))
				return;
			handled = true;
			const int lineEnd = request.indexOf("\r\n");
			const QList<QByteArray> parts = request.left(lineEnd).split(' ');
			if (parts.size() != 3 || parts.at(0) != "GET" || !parts.at(1).startsWith('/')) {
				finishLoopbackSocket(socket, false, tr("Invalid OAuth callback request."));
				return;
			}
			QUrl callback(QStringLiteral("http://127.0.0.1") + QString::fromUtf8(parts.at(1)));
			const QUrlQuery query(callback);
			const bool stateMatches = activePkce && activePkce->matchesState(query.queryItemValue(QStringLiteral("state")));
			// Favicon requests, probes and stale callbacks must not cancel the
			// current authorization or discard its in-flight token response.
			if (callback.path() != activeRedirectUri.path() || !stateMatches) {
				finishLoopbackSocket(socket, false, tr("This request is not the active OAuth callback."));
				return;
			}
			const bool hasCode = !query.queryItemValue(QStringLiteral("code")).isEmpty();
			finishLoopbackSocket(socket, stateMatches && hasCode,
				stateMatches && hasCode ? tr("Authorization received. Return to Subsurface Neo to check the connection result.")
							 : tr("Authorization failed. Return to Subsurface Neo for details."));
			handleAuthorizationRedirect(callback);
		});
	}
}

void CloudSyncManager::finishLoopbackSocket(QTcpSocket *socket, bool success, const QString &message)
{
	const QByteArray body = QStringLiteral("<!doctype html><html><body><h2>%1</h2><p>%2</p></body></html>")
		.arg(success ? tr("Subsurface Neo authorization received") : tr("Subsurface Neo authorization error"), message.toHtmlEscaped())
		.toUtf8();
	QByteArray response = "HTTP/1.1 200 OK\r\nContent-Type: text/html; charset=utf-8\r\nConnection: close\r\nContent-Length: ";
	response += QByteArray::number(body.size()) + "\r\n\r\n" + body;
	socket->write(response);
	socket->disconnectFromHost();
}

void CloudSyncManager::disconnectProvider(const QString &providerId)
{
	clearStorageReview(providerId);
	const auto *provider = descriptorForId(providerId);
	if (!provider)
		return;
	fileStore.cancel(provider->type);
	if (accountProviderId == providerId)
		cancelAccountCheck();
	verifiedAccountTokens.remove(providerId);
	if (providerId == refreshProviderId) {
		tokenClient.cancel();
		pendingTokenContinuation = {};
		refreshProviderId.clear();
	}
	if (providerId == activeProviderId)
		clearAuthorization();
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
	QSettings().remove(QStringLiteral("subsurface-neo/cloud/last-sync/%1").arg(providerId));
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

void CloudSyncManager::storeUpdatedTokens(const QString &providerId, OAuth2TokenSet updated)
{
	clearStorageReview(providerId);
	const auto previous = tokens.value(providerId);
	updated.accountId = previous.accountId;
	updated.accountName = previous.accountName;
	updated.accountEmail = previous.accountEmail;
	verifiedAccountTokens.remove(providerId);
	tokens.insert(providerId, updated);
	CloudCredentialStore::save(providerId, serializeTokens(updated));
}

bool CloudSyncManager::resetAccountSyncState(const QString &providerId)
{
	clearStorageReview(providerId);
	// Remove the old baseline before saving a different account identity. If
	// removal fails, retain the old identity so a restart also refuses to reuse it.
	if (!CloudCredentialStore::remove(syncStateCredentialKey(providerId))) {
		setError(tr("Could not clear the previous account's sync history. Please try again before syncing."));
		return false;
	}
	QSettings settings;
	if (providerId == primaryProviderId())
		settings.remove(QStringLiteral("subsurface-neo/cloud/primary-provider"));
	if (providerId == backupProviderId())
		settings.remove(QStringLiteral("subsurface-neo/cloud/backup-provider"));
	settings.remove(QStringLiteral("subsurface-neo/cloud/last-sync/%1").arg(providerId));
	settings.sync();
	emit providerAssignmentsChanged();
	return true;
}

void CloudSyncManager::cancelAccountCheck()
{
	if (accountReply) {
		accountReply->disconnect(this);
		accountReply->abort();
		accountReply->deleteLater();
		accountReply = nullptr;
	}
	accountProviderId.clear();
}

void CloudSyncManager::checkAccount(const QString &providerId)
{
	const auto *provider = descriptorForId(providerId);
	if (!provider || syncInProgress() || authorizationInProgress() || accountCheckInProgress() || pendingTokenContinuation)
		return;
	setError(QString());
	verifiedAccountTokens.remove(providerId);
	refreshThen(*provider, providerId, [](const QString &) {});
}

void CloudSyncManager::clearStorageReview(const QString &providerId)
{
	workspaceCopyReviews.remove(providerId);
	workspaceVersionReviews.remove(providerId);
	if (workspaceReports.remove(providerId)) emit providersChanged();
	if (historyReports.remove(providerId)) emit providersChanged();
	if (storageReviews.remove(providerId)) emit providersChanged();
}

void CloudSyncManager::runHistory(const QString &providerId, const QString &action, const QString &revision)
{
	const auto *provider = descriptorForId(providerId);
	if (!historyEnabled() || !provider || provider->type == CloudSyncProviderType::SubsurfaceCloud || syncInProgress() ||
	    authorizationInProgress() || accountCheckInProgress() || pendingTokenContinuation) return;
	if (autoSyncEnabled() || !primaryProviderId().isEmpty() || !backupProviderId().isEmpty()) {
		setError(tr("Experimental history requires automatic sync off and no primary or backup assignment. Use an isolated test profile.")); return;
	}
	clearStorageReview(providerId); setError({});
	if (historyReports.remove(providerId)) emit providersChanged();
	syncLocalPayload = serializeCurrentDiveLog();
	if (syncLocalPayload.isEmpty()) return;
	if (!acquireCloudRoute(providerId, true)) return;
	syncLocalSha256 = CloudSyncManifest::sha256(syncLocalPayload);
	syncProviderId = providerId; syncOperation = SyncOperation::History;
	emit syncInProgressChanged();
	if (!historyInProgress() || syncProviderId != providerId) return;
	verifiedAccountTokens.remove(providerId);
	refreshThen(*provider, providerId, [this, providerId, action, revision, type = provider->type](const QString &token) {
		if (!historyInProgress() || syncProviderId != providerId) return;
		const QString account = CloudSyncManifest::sha256(tokens.value(providerId).accountId.toUtf8());
		const QString dataPath = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
		if (dataPath.isEmpty()) { setError(tr("A local history checkpoint cannot be stored on this device.")); clearSyncOperation(); return; }
		const QString scope = CloudSyncManifest::sha256((QSettings().fileName() + "|" + providerId + "|" + account + "|" + historyEpoch).toUtf8());
		historyClient.start(type, token, account, historyEpoch,
			action, revision, syncLocalPayload, [this] {
				return historyInProgress() && !autoSyncEnabled() && primaryProviderId().isEmpty() && backupProviderId().isEmpty() && localDiveLogUnchanged();
			}, dataPath + "/neo-history-authority/" + scope + ".json");
	});
}

void CloudSyncManager::cancelHistory()
{
	if (!historyInProgress()) return;
	cancelAccountCheck(); tokenClient.cancel(); pendingTokenContinuation = {}; refreshProviderId.clear();
	clearSyncOperation(); setError({}); emit providersChanged();
}

void CloudSyncManager::reviewStorage(const QString &providerId)
{
	const auto *provider = descriptorForId(providerId);
	if (!provider || provider->type == CloudSyncProviderType::SubsurfaceCloud || syncInProgress() ||
	    authorizationInProgress() || accountCheckInProgress() || pendingTokenContinuation)
		return;
	clearStorageReview(providerId);
	setError(QString());
	syncProviderId = providerId;
	syncOperation = SyncOperation::StorageReview;
	emit syncInProgressChanged();
	verifiedAccountTokens.remove(providerId);
	refreshThen(*provider, providerId, [this, providerId, type = provider->type](const QString &token) {
		if (storageReviewInProgress() && syncProviderId == providerId)
			storageReviewClient.start(type, token);
	});
}

void CloudSyncManager::cancelStorageReview()
{
	if (!storageReviewInProgress()) return;
	cancelAccountCheck();
	tokenClient.cancel();
	pendingTokenContinuation = {};
	refreshProviderId.clear();
	clearSyncOperation();
	setError(QString());
	emit providersChanged();
}

void CloudSyncManager::verifyAccountThen(const QString &providerId, const QString &accessToken,
					 const std::function<void(const QString &)> &continuation)
{
	const auto *provider = descriptorForId(providerId);
	if (!provider || accessToken.isEmpty())
		return;
	if (verifiedAccountTokens.value(providerId) == accessToken) {
		continuation(accessToken);
		return;
	}
	Q_ASSERT(!accountCheckInProgress());
	const bool google = provider->type == CloudSyncProviderType::GoogleDrive;
	QNetworkRequest request(QUrl(google
		? QStringLiteral("https://www.googleapis.com/drive/v3/about?fields=user(permissionId,displayName,emailAddress)")
		: QStringLiteral("https://api.dropboxapi.com/2/users/get_current_account")));
	request.setRawHeader("Authorization", "Bearer " + accessToken.toUtf8());
	request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
	request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::ManualRedirectPolicy);
	request.setTransferTimeout(30000);
	accountProviderId = providerId;
	QNetworkReply *reply = google ? networkManager->get(request) : networkManager->post(request, QByteArray("null"));
	accountReply = reply;
	// Bound both a stalled request and an unexpectedly large identity response.
	QTimer::singleShot(35000, reply, [reply] { if (!reply->isFinished()) reply->abort(); });
	connect(reply, &QNetworkReply::readyRead, this, [reply] { if (reply->bytesAvailable() > 65536) reply->abort(); });
	connect(reply, &QNetworkReply::finished, this, [this, providerId, accessToken, continuation, google, reply] {
		if (accountReply != reply)
			return;
		accountReply = nullptr;
		accountProviderId.clear();
		reply->deleteLater();
		if (tokens.value(providerId).accessToken != accessToken) {
			emit providersChanged();
			return;
		}
		const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
		if (status == 401) {
			// Use the existing refresh/reconnect recovery without exposing the body.
			fileStore.operationError(descriptorForId(providerId)->type, QString(), QString(), true);
			emit providersChanged();
			return;
		}
		const QByteArray body = reply->read(65537);
		const auto object = QJsonDocument::fromJson(body).object();
		const auto user = google ? object.value(QStringLiteral("user")).toObject() : object;
		const QString id = user.value(google ? QStringLiteral("permissionId") : QStringLiteral("account_id")).toString();
		if (reply->error() != QNetworkReply::NoError || status != 200 || body.size() > 65536 ||
		    id.trimmed().isEmpty() || id.size() > 512) {
			setError(tr("Could not verify the connected %1 account. Your local log was kept. Check your connection and try again.")
				.arg(descriptorForId(providerId)->displayName));
			if (syncProviderId == providerId)
				clearSyncOperation();
			emit providersChanged();
			return;
		}
		auto current = tokens.value(providerId);
		const bool changed = !current.accountId.isEmpty() && current.accountId != id;
		if (changed && !resetAccountSyncState(providerId)) {
			if (syncProviderId == providerId)
				clearSyncOperation();
			emit providersChanged();
			return;
		}
		current.accountId = id;
		current.accountName = (google ? user.value(QStringLiteral("displayName"))
			: user.value(QStringLiteral("name")).toObject().value(QStringLiteral("display_name"))).toString().left(512);
		current.accountEmail = user.value(google ? QStringLiteral("emailAddress") : QStringLiteral("email")).toString().left(512);
		tokens.insert(providerId, current);
		CloudCredentialStore::save(providerId, serializeTokens(current));
		verifiedAccountTokens.insert(providerId, accessToken);
		emit providersChanged();
		if (changed) {
			if (syncProviderId == providerId)
				clearSyncOperation();
			setError(tr("The connected %1 account changed. Your local log was kept. Sync again to compare this account's cloud log, then choose primary or backup.")
				.arg(descriptorForId(providerId)->displayName));
			emit providerAccountChanged(providerId);
			return;
		}
		// A UI callback may have disconnected the provider during notification.
		if (tokens.value(providerId).accessToken == accessToken && verifiedAccountTokens.value(providerId) == accessToken)
			continuation(accessToken);
	});
	emit providersChanged();
}

void CloudSyncManager::refreshThen(const CloudSyncProviderDescriptor &provider, const QString &providerId,
					   const std::function<void(const QString &)> &continuation)
{
	if (authorizationInProgress() || pendingTokenContinuation || accountCheckInProgress()) {
		setError(tr("Wait for the current authorization to finish before syncing."));
		if (providerId == syncProviderId)
			clearSyncOperation();
		return;
	}
	const OAuth2TokenSet current = tokens.value(providerId);
	if (!current.hasAccessToken() && !current.canRefresh()) {
		setError(tr("%1 is not connected.").arg(provider.displayName));
		if (providerId == syncProviderId)
			clearSyncOperation();
		return;
	}
	if (current.hasAccessToken() && !current.isExpired()) {
		verifyAccountThen(providerId, current.accessToken, continuation);
		return;
	}
	if (!current.canRefresh()) {
#if defined(Q_OS_ANDROID)
		if (provider.type == CloudSyncProviderType::GoogleDrive) {
			refreshProviderId = providerId;
			pendingTokenContinuation = continuation;
			requestAndroidGoogleAccessToken();
			return;
		}
#endif
		setError(tr("%1 authorization expired. Please reconnect it.").arg(provider.displayName));
		if (providerId == syncProviderId)
			clearSyncOperation();
		return;
	}
	const QString clientId = configuredClientId(provider);
	if (clientId.isEmpty()) {
		setError(tr("%1 OAuth client is not available on this platform.").arg(provider.displayName));
		if (providerId == syncProviderId)
			clearSyncOperation();
		return;
	}
	refreshProviderId = providerId;
	pendingTokenContinuation = continuation;
	tokenClient.refreshAccessToken(provider, clientId, current.refreshToken);
}

void CloudSyncManager::uploadBytes(const QString &providerId, const QString &fileName, const QByteArray &data)
{
	if (storageReviewInProgress() || historyInProgress() || workspaceInProgress() || syncOperation == SyncOperation::RawProtectedUpload) return;
	clearStorageReview(providerId);
	const CloudSyncProviderDescriptor *provider = descriptorForId(providerId);
	if (!provider)
		return;
	const bool protectedName = provider->type != CloudSyncProviderType::SubsurfaceCloud &&
		fileName.toLower().startsWith(QStringLiteral("subsurface-neo"));
	if (protectedName) {
		if (syncInProgress() || accountCheckInProgress() || !acquireCloudRoute(providerId, true)) return;
		syncProviderId = providerId; syncOperation = SyncOperation::RawProtectedUpload; rawProtectedName = fileName; emit syncInProgressChanged();
		if (syncOperation != SyncOperation::RawProtectedUpload || syncProviderId != providerId || rawProtectedName != fileName) return;
	}
	refreshThen(*provider, providerId, [this, type = provider->type, providerId, fileName, data, protectedName](const QString &accessToken) {
		if (protectedName && (syncOperation != SyncOperation::RawProtectedUpload || syncProviderId != providerId || fileName != rawProtectedName || !legacyRouteCurrent())) {
			if (syncOperation == SyncOperation::RawProtectedUpload && syncProviderId == providerId) clearSyncOperation();
			return;
		}
		fileStore.upload(type, accessToken, fileName, data);
	});
}

void CloudSyncManager::downloadBytes(const QString &providerId, const QString &fileName)
{
	if (storageReviewInProgress() || historyInProgress() || workspaceInProgress()) return;
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

	// The general import parser accepts other XML formats and can recover a
	// partial document. Cloud replacement must accept only a complete canonical
	// log, even when the damaged bytes have a matching manifest checksum.
	QXmlStreamReader xml(payload);
	bool canonicalRoot = false;
	while (!xml.atEnd()) {
		xml.readNext();
		if (xml.isDTD()) {
			xml.raiseError(QStringLiteral("Cloud logs must not contain a DTD."));
			break;
		}
		if (xml.isStartElement() && !canonicalRoot) {
			if (xml.name() != QLatin1String("divelog") || !xml.namespaceUri().isEmpty()) {
				xml.raiseError(QStringLiteral("Expected a Subsurface divelog."));
				break;
			}
			canonicalRoot = true;
		}
	}
	if (!canonicalRoot || xml.hasError()) {
		setError(tr("The downloaded cloud dive log is not a complete Subsurface XML document. No local data was changed."));
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
	fulltext_unregister_all();
	select_single_dive(nullptr);
	::divelog = std::move(replacement);
	// Processing also rebuilds the global search index and current selection.
	// Install the replacement first so neither retains the discarded log.
	::divelog.process_loaded_dives();
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

void CloudSyncManager::recordSuccessfulSync(const QString &providerId)
{
	QSettings().setValue(QStringLiteral("subsurface-neo/cloud/last-sync/%1").arg(providerId),
		QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs));
	emit providersChanged();
}

bool CloudSyncManager::localDiveLogUnchanged()
{
	if (!legacyRouteCurrent()) return false;
	const QByteArray current = serializeCurrentDiveLog();
	if (current.isEmpty())
		return false;
	if (CloudSyncManifest::sha256(current) == syncLocalSha256)
		return true;
	setError(tr("The local dive log changed during synchronization. Your edits were kept. Please sync again."));
	return false;
}

void CloudSyncManager::clearSyncOperation()
{
	if (storageReviewInProgress()) storageReviewClient.cancel();
	if (historyInProgress()) historyClient.cancel();
	if (workspaceInProgress()) { workspaceReader.cancel(); workspaceCreator.cancel(); workspaceSource.cancel(); workspaceCopy.cancel(); workspaceAcceptance.cancel(); workspaceUpload.cancel(); workspaceRestoreReader.cancel(); workspaceRestoreApply.cancel(); ++workspaceSerial; }
	workspaceAction.clear(); workspaceAccount.clear(); workspaceToken.clear(); workspacePath.clear(); workspaceChoiceVersion.clear(); workspaceReference.value = {}; workspaceCheckpointRequired = false;
	workspaceCopyReview = {}; workspaceReviewedRoot.value = {}; workspaceLegacyChanged = false;
	workspaceVersionTicket.clear();
	workspaceRestorePlan = {}; workspaceRestoreReview = {}; workspaceRestoreBefore.clear(); workspaceRestoreBeforeModel.clear(); workspaceRestoreSourceBytes.clear(); workspaceRestoreModelBytes.clear();
	workspaceRestoreDirectory.clear(); workspaceRestoreSource.clear(); workspaceRestoreProjected = false; workspaceRestoreHeadMatches = false; workspaceRestoreLocalMatches = false;
	workspaceRestoreStep = WorkspaceRestoreStep::None; workspaceRestoreApplyRetries = 0;
	const bool wasActive = syncInProgress();
	syncOperation = SyncOperation::None;
	syncProviderId.clear();
	syncLocalPayload.clear();
	syncLocalSha256.clear();
	syncRemoteManifest = CloudSyncManifest();
	syncUploadManifest = CloudSyncManifest();
	forceCloudDownload = false;
	forceLocalUpload = false;
	syncBackupOnly = false;
	syncRemoteVersion.clear();
	syncRemoteFileId.clear();
	routeLease.reset(); routeProvider.clear(); rawProtectedName.clear(); legacyRouteOperation = false;
	if (wasActive)
		emit syncInProgressChanged();
}

void CloudSyncManager::startUploadSequence(const QString &providerId, const QByteArray &payload,
					   const QString &parentSha256, bool backupOnly)
{
	clearStorageReview(providerId);
	if (payload.isEmpty()) {
		setError(tr("Cannot upload an empty dive log."));
		clearSyncOperation();
		return;
	}

	syncProviderId = providerId;
	syncLocalPayload = payload;
	syncLocalSha256 = CloudSyncManifest::sha256(payload);
	syncUploadManifest = CloudSyncManifest::forPayload(payload, parentSha256);
	syncUploadManifest.schemaVersion = 2;
	syncUploadManifest.payloadFile = QStringLiteral("subsurface-neo-%1.xml").arg(syncUploadManifest.revisionId);
	syncBackupOnly = backupOnly;
	syncOperation = backupOnly ? SyncOperation::BackupUploadPayload : SyncOperation::SyncUploadPayload;
	uploadSyncFile(providerId, syncUploadManifest.payloadFileName(), payload);
}

void CloudSyncManager::backupDiveLog(const QString &providerId)
{
	if (!descriptorForId(providerId)) {
		setError(tr("Unknown cloud provider."));
		return;
	}
	if (syncInProgress() || accountCheckInProgress()) {
		setError(tr("Another cloud operation is already in progress."));
		return;
	}

	const QByteArray payload = serializeCurrentDiveLog();
	if (payload.isEmpty())
		return;
	if (!acquireCloudRoute(providerId, true)) return;

	setError(QString());
	forceLocalUpload = true;
	syncBackupOnly = true;
	syncProviderId = providerId;
	syncLocalPayload = payload;
	syncLocalSha256 = CloudSyncManifest::sha256(payload);
	syncOperation = SyncOperation::SyncDownloadManifest;
	emit syncInProgressChanged();
	downloadSyncManifest(providerId);
}

void CloudSyncManager::syncDiveLog(const QString &providerId)
{
	if (!descriptorForId(providerId)) {
		setError(tr("Unknown cloud provider."));
		return;
	}
	if (syncInProgress() || accountCheckInProgress()) {
		setError(tr("Another cloud operation is already in progress."));
		return;
	}

	const QByteArray payload = serializeCurrentDiveLog();
	if (payload.isEmpty())
		return;
	if (!acquireCloudRoute(providerId, true)) return;

	setError(QString());
	forceCloudDownload = false;
	syncProviderId = providerId;
	syncLocalPayload = payload;
	syncLocalSha256 = CloudSyncManifest::sha256(payload);
	syncOperation = SyncOperation::SyncDownloadManifest;
	emit syncInProgressChanged();
	downloadSyncManifest(providerId);
}

void CloudSyncManager::useCloudDiveLog(const QString &providerId)
{
	if (!descriptorForId(providerId)) {
		setError(tr("Unknown cloud provider."));
		return;
	}
	if (syncInProgress() || accountCheckInProgress()) {
		setError(tr("Another cloud operation is already in progress."));
		return;
	}

	const QByteArray payload = serializeCurrentDiveLog();
	if (payload.isEmpty())
		return;
	if (!acquireCloudRoute(providerId, true)) return;
	setError(QString());
	forceCloudDownload = true;
	syncProviderId = providerId;
	syncLocalPayload = payload;
	syncLocalSha256 = CloudSyncManifest::sha256(payload);
	syncOperation = SyncOperation::SyncDownloadManifest;
	emit syncInProgressChanged();
	downloadSyncManifest(providerId);
}

void CloudSyncManager::handleDownloadedManifest(const QString &providerId, const QByteArray &data)
{
	if (!localDiveLogUnchanged()) {
		clearSyncOperation();
		return;
	}
	QString error;
	const CloudSyncManifest remote = CloudSyncManifest::fromJson(data, &error);
	if (!remote.isValid()) {
		setError(error.isEmpty() ? tr("The cloud sync manifest is invalid.") : error);
		clearSyncOperation();
		return;
	}

	syncRemoteManifest = remote;
	if (forceLocalUpload) {
		startUploadSequence(providerId, syncLocalPayload, remote.payloadSha256, true);
		return;
	}
	if (forceCloudDownload) {
		syncOperation = SyncOperation::SyncDownloadPayload;
		downloadBytes(providerId, remote.payloadFileName());
		return;
	}

	const CloudSyncManifest previous = lastSyncManifest(providerId);
	const CloudSyncRelation relation = compareCloudSyncState(syncLocalSha256, remote.payloadSha256,
		previous.isValid() ? previous.payloadSha256 : QString());

	switch (relation) {
	case CloudSyncRelation::Identical:
		saveLastSyncManifest(providerId, remote);
		recordSuccessfulSync(providerId);
		clearSyncOperation();
		emit diveLogSyncFinished(providerId, QStringLiteral("up-to-date"));
		break;
	case CloudSyncRelation::LocalOnlyChanged:
		startUploadSequence(providerId, syncLocalPayload, remote.payloadSha256, false);
		break;
	case CloudSyncRelation::CloudOnlyChanged:
		syncOperation = SyncOperation::SyncDownloadPayload;
		downloadBytes(providerId, remote.payloadFileName());
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

	// Even an explicitly chosen cloud replacement must not discard edits made
	// after the user confirmed that choice.
	if (!localDiveLogUnchanged() || !applyCloudDiveLog(data)) {
		clearSyncOperation();
		return;
	}

	const CloudSyncManifest appliedManifest = syncRemoteManifest;
	saveLastSyncManifest(providerId, appliedManifest);
	recordSuccessfulSync(providerId);
	clearSyncOperation();
	emit diveLogSyncFinished(providerId, QStringLiteral("downloaded"));
}

void CloudSyncManager::downloadSyncManifest(const QString &providerId)
{
	const auto *provider = descriptorForId(providerId);
	if (!provider || syncProviderId != providerId || !syncInProgress())
		return;
	if (!legacyRouteCurrent()) { clearSyncOperation(); return; }
	const auto expected = syncOperation;
	refreshThen(*provider, providerId, [this, type = provider->type, providerId, expected](const QString &token) {
		if (syncProviderId != providerId || syncOperation != expected) return;
		if (!legacyRouteCurrent()) { clearSyncOperation(); return; }
		fileStore.downloadVersioned(type, token, QString::fromLatin1(NEO_MANIFEST_FILENAME));
	});
}

void CloudSyncManager::uploadSyncFile(const QString &providerId, const QString &fileName, const QByteArray &data,
				     const QString &version, const QString &fileId)
{
	const auto *provider = descriptorForId(providerId);
	if (!provider || syncProviderId != providerId || !syncInProgress())
		return;
	if (!legacyRouteCurrent()) { clearSyncOperation(); return; }
	const auto expected = syncOperation;
	refreshThen(*provider, providerId, [this, type = provider->type, providerId, expected, fileName, data, version, fileId](const QString &token) {
		if (syncProviderId != providerId || syncOperation != expected) return;
		if (!legacyRouteCurrent()) { clearSyncOperation(); return; }
		fileStore.uploadConditional(type, token, fileName, data, version, fileId);
	});
}

void CloudSyncManager::finishUpload(const QString &providerId)
{
	const bool backupOnly = syncBackupOnly;
	saveLastSyncManifest(providerId, syncUploadManifest);
	// A late local edit remains dirty against the committed snapshot.
	if (!localDiveLogUnchanged()) {
		clearSyncOperation();
		return;
	}
	if (!backupOnly)
		recordSuccessfulSync(providerId);
	clearSyncOperation();
	if (backupOnly)
		emit diveLogBackupFinished(providerId);
	else
		emit diveLogSyncFinished(providerId, QStringLiteral("uploaded"));
}
