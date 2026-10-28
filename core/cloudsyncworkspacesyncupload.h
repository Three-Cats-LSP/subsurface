// SPDX-License-Identifier: GPL-2.0
#ifndef CLOUD_SYNC_WORKSPACE_SYNC_UPLOAD_H
#define CLOUD_SYNC_WORKSPACE_SYNC_UPLOAD_H
#include "cloudsyncworkspacesyncreview.h"
#include "cloudsyncworkspaceupload.h"
#include "cloudsyncworkspaceuploadplan.h"
#include <QTimer>
#include <memory>
class QLockFile;

// One guarded local-only sync publication with durable recovery. Fresh account
// verification and a continuing choice/local guard are caller responsibilities.
// No app caller, automatic routing, legacy writes, or conflict merge.
class CloudWorkspaceSyncUpload : public QObject {
	Q_OBJECT
public:
	explicit CloudWorkspaceSyncUpload(QNetworkAccessManager *network, QObject *parent = nullptr);
	~CloudWorkspaceSyncUpload() override;
	void start(const QString &token, const CloudWorkspaceReference &workspace, const QString &choiceFile, const QString &choiceVersion,
		const QString &checkpointFile, const QString &bindingFile, const QString &bindingRevision, const QString &nextRevision,
		const QString &baseline, const QString &ticket, const QString &operation, const QString &planDirectory, const QString &intentFile,
		const QByteArray &localModel, const std::function<bool()> &scopeCurrent, const std::function<QByteArray()> &localSnapshot);
	void cancel();
signals:
	void finished(const QJsonObject &root, const QString &version, const QString &ticket, bool headMatches);
	void failed(const QString &message, bool tokenRejected);
private:
	enum class Step { Idle, InitialReview, Uploading, FinalReview };
	bool current();
	void fail(bool rejected = false);
	void review(Step next);
	void reviewed(int action, const QJsonObject &root, const QString &version, const QString &revision);
	void uploaded(const QJsonObject &root, const QString &version, const QString &ticket, bool headMatches);
	CloudWorkspaceSyncReview reviewer;
	CloudWorkspaceUpload uploader;
	QTimer deadline;
	std::unique_ptr<CloudWorkspaceUploadPlan> plan;
	std::unique_ptr<QLockFile> operationLock;
	CloudWorkspaceReference reference;
	QJsonObject originalBinding;
	Step step = Step::Idle;
	bool priorPlan = false;
	quint64 generation = 0;
	QString token, choiceFile, choiceVersion, checkpointFile, bindingFile, bindingRevision, nextRevision;
	QString baseline, ticket, operation, planDirectory, intentFile;
	QByteArray model;
	std::function<bool()> scopeCurrent;
	std::function<QByteArray()> localSnapshot;
};
#endif
