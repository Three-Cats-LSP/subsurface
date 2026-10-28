// SPDX-License-Identifier: GPL-2.0
#ifndef CLOUD_SYNC_WORKSPACE_SYNC_RECEIVE_H
#define CLOUD_SYNC_WORKSPACE_SYNC_RECEIVE_H
#include "cloudsyncworkspacereceive.h"
#include "cloudsyncworkspacereceiveplan.h"
#include "cloudsyncworkspacesyncreview.h"
#include <QTimer>
#include <memory>
class QLockFile;

// One guarded remote-only sync receive. The caller supplies pure projection,
// installation and actual durable local saving; account scope must stay fresh.
// No app caller, automatic route, legacy writes or conflict merge.
class CloudWorkspaceSyncReceive : public QObject {
	Q_OBJECT
public:
	explicit CloudWorkspaceSyncReceive(QNetworkAccessManager *network, QObject *parent = nullptr);
	~CloudWorkspaceSyncReceive() override;
	void start(const QString &token, const CloudWorkspaceReference &workspace, const QString &choiceFile, const QString &choiceVersion,
		const QString &checkpointFile, const QString &bindingFile, const QString &bindingRevision, const QString &nextRevision,
		const QString &baseline, const QString &target, const QString &reservation, const QString &operation,
		const QString &planDirectory, const QString &readIntentFile, const QString &recoveryDirectory, const QByteArray &beforeModel,
		const std::function<bool()> &scopeCurrent, const std::function<QByteArray()> &localSnapshot,
		const std::function<QByteArray(const QByteArray &, const QByteArray &)> &project,
		const std::function<bool(const QByteArray &, const QByteArray &, const QByteArray &)> &apply);
	void confirmPersisted();
	void cancel();
signals:
	void applied(const QString &recoveryDirectory);
	void finished(const QJsonObject &root, const QString &version, const QString &target, bool headMatches);
	void failed(const QString &message, bool tokenRejected);
private:
	enum class Step { Idle, InitialReview, Receiving, PostReview };
	bool current();
	void fail(bool rejected = false);
	void review(Step next, const QByteArray &expectedModel);
	void reviewed(int action, const QJsonObject &root, const QString &version, const QString &revision);
	void received(const QJsonObject &root, const QString &version, const QString &target);
	CloudWorkspaceSyncReview reviewer;
	CloudWorkspaceReceive receiver;
	QTimer deadline;
	std::unique_ptr<CloudWorkspaceReceivePlan> plan;
	std::unique_ptr<QLockFile> operationLock;
	CloudWorkspaceReference reference;
	Step step = Step::Idle;
	bool priorPlan = false;
	quint64 generation = 0;
	QString token, choiceFile, choiceVersion, checkpointFile, bindingFile, bindingRevision, nextRevision;
	QString baseline, target, reservation, operation, planDirectory, readFile, recoveryDirectory;
	QByteArray before;
	std::function<bool()> scopeCurrent;
	std::function<QByteArray()> localSnapshot;
	std::function<QByteArray(const QByteArray &, const QByteArray &)> projectPayload;
	std::function<bool(const QByteArray &, const QByteArray &, const QByteArray &)> applyPayload;
};
#endif
