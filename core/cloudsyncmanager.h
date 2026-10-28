// SPDX-License-Identifier: GPL-2.0
#ifndef CLOUD_SYNC_MANAGER_H
#define CLOUD_SYNC_MANAGER_H

#include "cloudsyncfilestore.h"
#include "cloudsyncmanifest.h"
#include "cloudsyncreview.h"
#include "cloudsynchistory.h"
#include "cloudsyncworkspace.h"
#include "cloudsyncworkspacecopy.h"
#include "cloudsyncworkspaceacceptance.h"
#include "cloudsyncworkspaceupload.h"
#include "cloudsyncworkspaceapply.h"
#include "cloudsyncworkspaceroute.h"
#include "cloudsyncworkspaceactivation.h"
#include "cloudsyncworkspacesyncreview.h"
#include "cloudsyncworkspacesyncupload.h"
#include "cloudsyncworkspacesyncreceive.h"
#include "oauth2pkce.h"
#include "oauth2tokenclient.h"

#include <QHash>
#include <QObject>
#include <QPointer>
#include <QUrl>
#include <QVariantList>

#include <functional>
#include <memory>

class QNetworkAccessManager;
class QTcpServer;
class QNetworkReply;
class QTcpSocket;

class CloudSyncManager : public QObject {
	Q_OBJECT
	Q_PROPERTY(QVariantList providers READ providers NOTIFY providersChanged)
	Q_PROPERTY(bool authorizationInProgress READ authorizationInProgress NOTIFY authorizationInProgressChanged)
	Q_PROPERTY(bool accountCheckInProgress READ accountCheckInProgress NOTIFY providersChanged)
	Q_PROPERTY(bool syncInProgress READ syncInProgress NOTIFY syncInProgressChanged)
	Q_PROPERTY(bool storageReviewInProgress READ storageReviewInProgress NOTIFY syncInProgressChanged)
	Q_PROPERTY(bool historyEnabled READ historyEnabled CONSTANT)
	Q_PROPERTY(bool historyInProgress READ historyInProgress NOTIFY syncInProgressChanged)
	Q_PROPERTY(bool workspaceEnabled READ workspaceEnabled CONSTANT)
	Q_PROPERTY(bool workspaceInProgress READ workspaceInProgress NOTIFY syncInProgressChanged)
	Q_PROPERTY(QString workspaceProviderId READ workspaceProviderId NOTIFY syncInProgressChanged)
	Q_PROPERTY(QString storageReviewProviderId READ storageReviewProviderId NOTIFY syncInProgressChanged)
	Q_PROPERTY(QString lastError READ lastError NOTIFY lastErrorChanged)
	Q_PROPERTY(QString primaryProviderId READ primaryProviderId NOTIFY providerAssignmentsChanged)
	Q_PROPERTY(QString backupProviderId READ backupProviderId NOTIFY providerAssignmentsChanged)
	Q_PROPERTY(bool autoSyncEnabled READ autoSyncEnabled WRITE setAutoSyncEnabled NOTIFY autoSyncEnabledChanged)
public:
	explicit CloudSyncManager(QNetworkAccessManager *networkManager, QObject *parent = nullptr);
	~CloudSyncManager() override;

	QVariantList providers() const;
	bool authorizationInProgress() const { return !activeProviderId.isEmpty(); }
	bool accountCheckInProgress() const { return !accountProviderId.isEmpty(); }
	bool syncInProgress() const { return !syncProviderId.isEmpty(); }
	bool storageReviewInProgress() const { return syncOperation == SyncOperation::StorageReview; }
	QString storageReviewProviderId() const { return storageReviewInProgress() ? syncProviderId : QString(); }
	bool historyEnabled() const { return CloudHistoryRecord::validEpoch(historyEpoch); }
	bool historyInProgress() const { return syncOperation == SyncOperation::History; }
	bool workspaceEnabled() const { return historyEnabled(); }
	bool workspaceInProgress() const { return syncOperation == SyncOperation::Workspace; }
	QString workspaceProviderId() const { return workspaceInProgress() ? syncProviderId : QString(); }
	QString lastError() const { return errorText; }
	QString primaryProviderId() const;
	QString backupProviderId() const;
	bool autoSyncEnabled() const;

	Q_INVOKABLE void beginAuthorization(const QString &providerId);
	Q_INVOKABLE void handleAuthorizationRedirect(const QUrl &url);
	Q_INVOKABLE void checkAccount(const QString &providerId);
	Q_INVOKABLE void reviewStorage(const QString &providerId);
	Q_INVOKABLE void cancelStorageReview();
	Q_INVOKABLE void runHistory(const QString &providerId, const QString &action, const QString &revision = {});
	Q_INVOKABLE void cancelHistory();
	Q_INVOKABLE void runWorkspace(const QString &providerId, const QString &action, const QString &argument = {});
	Q_INVOKABLE void runWorkspaceVersion(const QString &providerId, const QString &action, const QString &argument = {});
	Q_INVOKABLE void cancelWorkspace();
	Q_INVOKABLE void disconnectProvider(const QString &providerId);
	Q_INVOKABLE void uploadBytes(const QString &providerId, const QString &fileName, const QByteArray &data);
	Q_INVOKABLE void downloadBytes(const QString &providerId, const QString &fileName);
	Q_INVOKABLE void backupDiveLog(const QString &providerId);
	Q_INVOKABLE void syncDiveLog(const QString &providerId);
	// Kept separate from normal sync: this requires the explicit review ID.
	Q_INVOKABLE void resolveWorkspaceConflict(const QString &providerId, const QString &reviewId, const QString &choice);
	Q_INVOKABLE void useCloudDiveLog(const QString &providerId);
	Q_INVOKABLE void setPrimaryProvider(const QString &providerId);
	Q_INVOKABLE void setBackupProvider(const QString &providerId);
	Q_INVOKABLE void setAutoSyncEnabled(bool enabled);
	Q_INVOKABLE bool syncPrimaryIfReady();
	void handleAndroidGoogleAccessToken(const QString &accessToken);
	void handleAndroidGoogleAuthorizationError(const QString &message);
	// C++ lifecycle hooks: the UI must check the owned local destination before
	// replacing a model and acknowledge only a successful durable local save.
	void setWorkspacePersistenceHandlers(std::function<bool()> available, std::function<bool()> persist);

signals:
	void providersChanged();
	void providerAssignmentsChanged();
	void autoSyncEnabledChanged();
	void authorizationInProgressChanged();
	void syncInProgressChanged();
	void lastErrorChanged();
	void authorizationUrlOpened(const QUrl &url);
	void providerConnected(const QString &providerId);
	void providerDisconnected(const QString &providerId);
	void providerAccountChanged(const QString &providerId);
	void uploadFinished(const QString &providerId, const QString &fileName);
	void downloadFinished(const QString &providerId, const QString &fileName, const QByteArray &data);
	void diveLogBackupFinished(const QString &providerId);
	void diveLogSyncFinished(const QString &providerId, const QString &result);
	void diveLogSyncConflict(const QString &providerId);
	void diveLogInitialChoiceRequired(const QString &providerId);
	void storageReviewFinished(const QString &providerId);
	void historyFinished(const QString &providerId);
	void workspaceFinished(const QString &providerId);

private:
	friend class TestCloudSyncManager;
	enum class SyncOperation {
		None,
		StorageReview,
		History,
		Workspace,
		RawProtectedUpload,
		WorkspaceSync,
		BackupUploadPayload,
		BackupUploadManifest,
		SyncDownloadManifest,
		SyncCheckOrphanPayload,
		SyncUploadPayload,
		SyncUploadManifest,
		SyncDownloadPayload,
		SyncVerifyCommit,
	};

	const CloudSyncProviderDescriptor *descriptorForId(const QString &providerId) const;
	QString configuredClientId(const CloudSyncProviderDescriptor &provider) const;
	void setError(const QString &message);
	void storeUpdatedTokens(const QString &providerId, OAuth2TokenSet updated);
	bool resetAccountSyncState(const QString &providerId);
	void cancelAccountCheck();
	void verifyAccountThen(const QString &providerId, const QString &accessToken,
		const std::function<void(const QString &)> &continuation);
	void acceptNativeAccessToken(const QString &providerId, const QString &accessToken);
	void requestAndroidGoogleAccessToken();
	void clearAuthorization();
	void exchangeCode(const QString &code);
	QUrl startLoopbackListener(const CloudSyncProviderDescriptor &provider);
	void handleLoopbackConnection();
	void finishLoopbackSocket(QTcpSocket *socket, bool success, const QString &message);
	void refreshThen(const CloudSyncProviderDescriptor &provider, const QString &providerId,
			 const std::function<void(const QString &)> &continuation);
	QByteArray serializeCurrentDiveLog();
	bool applyCloudDiveLog(const QByteArray &payload);
	CloudSyncManifest lastSyncManifest(const QString &providerId) const;
	void saveLastSyncManifest(const QString &providerId, const CloudSyncManifest &manifest);
	void recordSuccessfulSync(const QString &providerId);
	void startUploadSequence(const QString &providerId, const QByteArray &payload,
				 const QString &parentSha256, bool backupOnly);
	void handleDownloadedManifest(const QString &providerId, const QByteArray &data);
	void handleDownloadedDiveLog(const QString &providerId, const QByteArray &data);
	void clearSyncOperation();
	QString workspaceRoutePath(const QString &providerId) const;
	bool acquireCloudRoute(const QString &providerId, bool legacy);
	bool legacyRouteCurrent();
	bool workspaceRouteRequired(const QString &providerId) const;
	bool workspaceRouteConfirmed(const QString &providerId) const;
	bool requireWorkspaceRoute(const QString &providerId);
	bool workspaceSyncInProgress() const { return syncOperation == SyncOperation::WorkspaceSync; }
	bool workspaceSyncScopeCurrent();
	void startWorkspaceSync(const QString &providerId);
	void workspaceSyncReviewed(int action, const QJsonObject &root, const QString &version, const QString &revision);
	void workspaceSyncApplied(const QString &directory);
	void workspaceSyncCompleted(const QJsonObject &root, const QString &version, const QString &ticket, bool headMatches, bool received);
	bool resumeWorkspaceSync();
	static QString syncStateCredentialKey(const QString &providerId);
	bool localDiveLogUnchanged();
	void downloadSyncManifest(const QString &providerId);
	void uploadSyncFile(const QString &providerId, const QString &fileName, const QByteArray &data,
		const QString &version = {}, const QString &fileId = {});
	void finishUpload(const QString &providerId);
	void clearStorageReview(const QString &providerId);
	QString workspaceDirectory(const QString &providerId, const QString &account) const;
	QList<QJsonObject> workspaceCreations(const QString &directory, const QString &providerId, const QString &account, bool *limited = nullptr, bool *invalid = nullptr) const;
	QList<QJsonObject> workspaceCopies(const QString &directory, const CloudWorkspaceReference &reference, bool *limited = nullptr, bool *invalid = nullptr) const;
	QList<QJsonObject> workspaceUploads(const QString &directory, const CloudWorkspaceReference &reference, bool *limited = nullptr, bool *invalid = nullptr) const;
	QList<QJsonObject> workspaceAcceptances(const QString &directory, const CloudWorkspaceReference &reference, bool *limited = nullptr, bool *invalid = nullptr) const;
	QList<QJsonObject> workspaceActivations(const QString &directory, const CloudWorkspaceReference &reference, bool *limited = nullptr, bool *invalid = nullptr) const;
	QVariantList workspaceRestores(const QString &directory, const CloudWorkspaceReference &reference, const QString &choiceVersion, bool *limited = nullptr, bool *invalid = nullptr) const;
	void finishWorkspaceVersions(const QJsonObject &record, const QString &version, const QString &ticket = {}, bool headMatches = false);
	void startWorkspaceRestore(const QJsonObject &root);
	void workspaceRestoreReadFinished(const QJsonObject &root, const QString &version, const QString &commit, const QString &reservation, const QByteArray &payload);
	void workspaceRestorePublished(const QJsonObject &root, const QString &version, const QString &ticket, bool headMatches);
	void workspaceRestoreApplied(const QString &directory);
	void workspaceRestoreApplyFailed(const QString &message, bool rejected);
	void workspaceRestoreCompleted(const QJsonObject &root, const QString &version, const QString &commit, bool headMatches, bool localMatches);
	void workspaceRestoreReleased(const QJsonObject &root, const QString &version, const QString &reservation);
	void workspaceRestoreBindingReviewed(int action, const QJsonObject &root, const QString &version, const QString &revision);
	void finishWorkspaceRestoreRelease(const QJsonObject &root);
	bool prepareWorkspaceRestoreBinding(bool resuming);
	void beginWorkspaceRestoreApply();
	QByteArray workspaceRestoreSnapshot() const;
	bool prepareWorkspaceRestoreProjection(const QByteArray &payload);
	bool loadWorkspaceRestoreProjection();
	QVariantMap workspaceReport(const QString &providerId) const;
	bool workspaceScopeCurrent();
	void finishWorkspace(const QJsonObject &record, const QString &version);
	void finishWorkspaceSource(const QJsonObject &source);
	void finishWorkspaceMissingSource();
	void failWorkspace(const QString &message, bool rejected = false);

	QNetworkAccessManager *networkManager;
	OAuth2TokenClient tokenClient;
	CloudSyncFileStore fileStore;
	CloudStorageReview storageReviewClient;
	CloudSyncHistory historyClient;
	CloudWorkspaceOpen workspaceReader;
	CloudWorkspaceCreate workspaceCreator;
	CloudWorkspaceSourceReview workspaceSource;
	CloudWorkspaceCopy workspaceCopy;
	CloudWorkspaceAcceptance workspaceAcceptance;
	CloudWorkspaceActivation workspaceActivation;
	CloudWorkspaceSyncReview workspaceSyncReview;
	CloudWorkspaceSyncUpload workspaceSyncUpload;
	CloudWorkspaceSyncReceive workspaceSyncReceive;
	CloudWorkspaceUpload workspaceUpload;
	CloudWorkspaceRead workspaceRestoreReader;
	CloudWorkspaceApply workspaceRestoreApply;
	const QString historyEpoch = qEnvironmentVariable("SUBSURFACE_NEO_HISTORY_DEV_EPOCH");
	QHash<QString, QVariantMap> historyReports;
	QHash<QString, QVariantMap> storageReviews;
	QHash<QString, QVariantMap> workspaceReports;
	QString workspaceAction, workspaceAccount, workspaceToken, workspacePath, workspaceChoiceVersion, workspaceVersionTicket;
	CloudWorkspaceReference workspaceReference;
	bool workspaceCheckpointRequired = false;
	bool workspaceAssignedReview = false, workspaceExpectedAutoSync = false;
	QString workspaceExpectedPrimary, workspaceExpectedBackup;
	QJsonObject workspaceExpectedRoute;
	quint64 workspaceSerial = 0;
	struct WorkspaceCopyReview {
		QString id, choiceVersion, localSha256, parent, previousAcceptance, copyTicket;
		CloudWorkspaceReference reference;
		CloudWorkspaceCopySource source;
		bool sourceMissing = false;
	};
	struct WorkspaceConflictReview {
		QString id, choiceVersion, localSha256, head;
		CloudWorkspaceReference reference;
		QJsonObject binding;
	};
	QHash<QString, WorkspaceConflictReview> workspaceConflictReviews;
	QString workspaceConflictRequested, workspaceConflictProvider;
	QHash<QString, WorkspaceCopyReview> workspaceCopyReviews;
	WorkspaceCopyReview workspaceCopyReview;
	CloudWorkspaceRecord workspaceReviewedRoot;
	bool workspaceLegacyChanged = false;
	std::function<bool()> workspacePersistenceAvailable, workspacePersist;
	struct WorkspaceVersionReview {
		QString id, choiceVersion, localSha256, parent;
		CloudWorkspaceReference reference;
		QStringList commits;
	};
	QHash<QString, WorkspaceVersionReview> workspaceVersionReviews;
	WorkspaceVersionReview workspaceRestoreReview;
	QJsonObject workspaceRestorePlan;
	QJsonObject workspaceRestoreBindingIntent, workspaceRestoreRoute;
	QByteArray workspaceRestoreBefore, workspaceRestoreBeforeModel, workspaceRestoreSourceBytes, workspaceRestoreModelBytes;
	QString workspaceRestoreDirectory, workspaceRestoreSource;
	bool workspaceRestoreProjected = false, workspaceRestoreHeadMatches = false, workspaceRestoreLocalMatches = false;
	bool workspaceRestoreUsesBinding = false;
	enum class WorkspaceRestoreStep { None, PreflightBinding, Source, Publishing, Applying, ReleaseApply, ReleaseSource, PostBindingReview };
	WorkspaceRestoreStep workspaceRestoreStep = WorkspaceRestoreStep::None;
	int workspaceRestoreApplyRetries = 0;
	QString workspaceDataRootOverride; // Friend tests use a temporary storage root.
	std::unique_ptr<CloudWorkspaceRouteLease> routeLease;
	QString routeProvider, rawProtectedName;
	bool legacyRouteOperation = false;
	QString workspaceSyncAccount, workspaceSyncToken, workspaceSyncPath, workspaceSyncChoiceVersion, workspaceSyncBindingFile, workspaceSyncCheckpointFile;
	CloudWorkspaceReference workspaceSyncReference;
	QJsonObject workspaceSyncRoute;
	QByteArray workspaceSyncModel;
	QHash<QString, OAuth2TokenSet> tokens;
	QHash<QString, QString> verifiedAccountTokens;
	QPointer<QNetworkReply> accountReply;
	QString accountProviderId;
	QString activeProviderId;
	QString activeClientId;
	bool codeExchangeInProgress = false;
	QUrl activeRedirectUri;
	std::unique_ptr<OAuth2PkceSession> activePkce;
	QPointer<QTcpServer> loopbackServer;
	QString errorText;
	QString refreshProviderId;
	std::function<void(const QString &)> pendingTokenContinuation;

	SyncOperation syncOperation = SyncOperation::None;
	QString syncProviderId;
	QByteArray syncLocalPayload;
	QString syncLocalSha256;
	CloudSyncManifest syncRemoteManifest;
	CloudSyncManifest syncUploadManifest;
	bool forceCloudDownload = false;
	bool forceLocalUpload = false;
	bool syncBackupOnly = false;
	QString syncRemoteVersion;
	QString syncRemoteFileId;
};

#endif // CLOUD_SYNC_MANAGER_H
