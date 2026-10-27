// SPDX-License-Identifier: GPL-2.0
#ifndef CLOUD_SYNC_WORKSPACE_COPY_H
#define CLOUD_SYNC_WORKSPACE_COPY_H
#include "cloudsyncworkspace.h"
#include <QElapsedTimer>

// Durable per-attempt metadata. A submitted payload without an acknowledged
// physical ID cannot be replayed. No XML, token or credentials are stored here.
class CloudWorkspaceCopyIntent {
public:
	CloudWorkspaceCopyIntent(const QString &file, const CloudWorkspaceReference &workspace, const CloudWorkspaceCopySource &source,
		const QString &ticket, const QString &parent);
	bool prepare();
	bool reload();
	bool beginRegistration();
	bool registerTicket();
	bool submit();
	bool identify(const QString &id);
	bool verify(const QString &version);
	bool upload();
	bool commit();
	QString state() const;
	QString id() const;
	QString version() const;
	QString ticket() const;
	QString parent() const;
private:
	bool read();
	bool write(const QJsonObject &next);
	bool advance(const QString &from, const QString &to, const QString &id = {}, const QString &version = {}, bool once = false);
	QString fileName;
	CloudWorkspaceReference reference;
	CloudWorkspaceCopySource source;
	QJsonObject scope, saved;
};

// Copy only into an explicitly selected physical workspace. The caller supplies
// fresh account verification and a scope/local-edit guard. Source objects,
// local choice, XML, old baselines and credentials are never written or deleted.
class CloudWorkspaceCopy : public QObject {
	Q_OBJECT
public:
	explicit CloudWorkspaceCopy(QNetworkAccessManager *network, QObject *parent = nullptr);
	~CloudWorkspaceCopy() override;
	void start(const QString &token, const CloudWorkspaceReference &workspace, const CloudWorkspaceCopySource &source,
		const QString &choiceFile, const QString &choiceVersion, const QString &checkpointFile,
		const QString &intentFile, const QString &proofFile, const QString &ticket, const QString &parent,
		const std::function<bool()> &scopeCurrent);
	void cancel();
signals:
	void finished(const QJsonObject &root, const QString &version, const QString &ticket, bool legacyChanged);
	void failed(const QString &message, bool tokenRejected);
private:
	enum class Step { Idle, Starting, InitialRoot, SourceBefore, WriteRoot, ConfirmRoot, Upload, Payload, SourceBeforePublish, PayloadBeforePublish, PublishRoot, ExistingPayload, PayloadAfter, SourceAfter };
	void begin();
	void retryStartupOrFail(const QString &path);
	bool current();
	void fail(bool rejected = false);
	void readRoot(Step next);
	void received(const QString &name, const QByteArray &bytes, const QString &version, const QString &id);
	void writeRoot(const CloudWorkspaceRecord &next, const std::function<void()> &then);
	void continueCopy();
	void prepareUpload();
	void uploaded();
	void publish();
	void complete();
	CloudSyncFileStore files;
	CloudWorkspaceSourceReview sourceReview;
	QTimer deadline;
	QTimer startupRetry;
	QElapsedTimer startupWait;
	std::unique_ptr<CloudWorkspaceCopyIntent> intent;
	CloudWorkspaceReference reference;
	CloudWorkspaceCopySource source;
	CloudWorkspaceRecord root, pending;
	CloudSyncProviderType provider = CloudSyncProviderType::GoogleDrive;
	QString token, choiceFile, choiceVersion, checkpointFile, rootVersion, resultTicket;
	QString initialIntentFile, initialProofFile, initialTicket, initialParent;
	QByteArray payload;
	Step step = Step::Idle;
	std::function<bool()> scopeCurrent;
	std::function<void()> continuation;
};
#endif
