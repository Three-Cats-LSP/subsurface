// SPDX-License-Identifier: GPL-2.0
#ifndef CLOUD_SYNC_WORKSPACE_ACCEPTANCE_H
#define CLOUD_SYNC_WORKSPACE_ACCEPTANCE_H

#include "cloudsyncworkspace.h"
#include <QTimer>
#include <QElapsedTimer>
#include <memory>

class QLockFile;

// Metadata-only intent. A saved event UUID can be reconciled after a lost CAS
// acknowledgement; an unrelated or stale confirmation is never substituted.
class CloudWorkspaceAcceptanceIntent {
public:
	CloudWorkspaceAcceptanceIntent(const QString &path, const CloudWorkspaceReference &workspace,
		const QString &choiceVersion, const CloudWorkspaceCopySource &source, const QString &event,
		const QString &previous, const QString &head, const QString &copyTicket);
	bool inspect();
	bool prepare();
	bool submitted();
	bool committed();
	QString state() const;
	QJsonObject proof() const { return saved; }
private:
	bool read();
	bool write(const QJsonObject &body);
	bool advance(const QStringList &from, const QString &to);
	QString fileName;
	QJsonObject scope, saved;
};

// Caller provides fresh account verification and a local-edit/scope guard.
// Only a previously copied source may be explicitly accepted. The source and
// physical root are freshly rechecked around a conditional root update.
class CloudWorkspaceAcceptance : public QObject {
	Q_OBJECT
public:
	explicit CloudWorkspaceAcceptance(QNetworkAccessManager *network, QObject *parent = nullptr);
	~CloudWorkspaceAcceptance() override;
	void start(const QString &token, const CloudWorkspaceReference &workspace, const CloudWorkspaceCopySource &source,
		const QString &choiceFile, const QString &choiceVersion, const QString &checkpointFile, const QString &intentFile,
		const QString &event, const QString &previous, const QString &head, const QString &copyTicket,
		const std::function<bool()> &scopeCurrent);
	void cancel();
signals:
	void finished(const QJsonObject &root, const QString &version, const QString &event, bool latest, bool sourceChanged);
	void failed(const QString &message, bool tokenRejected);
private:
	enum class Step { Idle, Starting, InitialRoot, SourceBefore, CopiedPayload, PublishRoot, WriteRoot, ConfirmRoot, SourceAfter, FinalRoot, FinalPayload };
	bool current();
	void fail(bool rejected = false);
	void begin();
	void retryStartupOrFail(const QString &path);
	void readRoot(Step next);
	void received(const QString &name, const QByteArray &bytes, const QString &version, const QString &id);
	bool eventPresent(const CloudWorkspaceRecord &root) const;
	void observeSource(Step next);
	CloudSyncFileStore files;
	CloudWorkspaceSourceReview sourceReview;
	QTimer deadline, startupRetry;
	QElapsedTimer startupWait;
	std::unique_ptr<CloudWorkspaceAcceptanceIntent> intent;
	std::unique_ptr<QLockFile> operationLock;
	CloudWorkspaceReference reference;
	CloudWorkspaceCopySource source;
	CloudWorkspaceRecord root, pending;
	CloudSyncProviderType provider = CloudSyncProviderType::GoogleDrive;
	Step step = Step::Idle;
	bool sourceChanged = false;
	quint64 generation = 0;
	QString token, choiceFile, choiceVersion, checkpointFile, intentFile, event, previous, expectedHead, copyTicket, rootVersion;
	std::function<bool()> scopeCurrent;
};
#endif
