// SPDX-License-Identifier: GPL-2.0
#ifndef CLOUD_SYNC_WORKSPACE_ACTIVATION_H
#define CLOUD_SYNC_WORKSPACE_ACTIVATION_H
#include "cloudsyncworkspaceactivationplan.h"
#include <QTimer>
#include <memory>
class QLockFile;

// Explicit activation only. Caller must hold the profile/provider operation
// fence, freshly verify the account and durably require a route marker before
// confirming the callback. No app caller or automatic routing is provided.
class CloudWorkspaceActivation : public QObject {
	Q_OBJECT
public:
	explicit CloudWorkspaceActivation(QNetworkAccessManager *network, QObject *parent = nullptr);
	~CloudWorkspaceActivation() override;
	void start(const QString &token, const CloudWorkspaceReference &workspace, const QString &choiceFile, const QString &choiceVersion,
		const QString &checkpointFile, const QString &bindingFile, const QString &routeFile, const QString &revision,
		const QString &target, const QString &reservation, const QString &operation, const QString &acceptance,
		const QString &planDirectory, const QString &readIntentFile, const QByteArray &model,
		const std::function<bool()> &requireRoute, const std::function<bool()> &routeRequired,
		const std::function<bool()> &scopeCurrent, const std::function<QByteArray()> &localSnapshot);
	void cancel();
signals:
	void finished(const QJsonObject &root, const QString &version, const QString &target, bool headMatches);
	void failed(const QString &message, bool tokenRejected);
private:
	enum class Step { Idle, InitialRoot, InitialSource, Reading, VerifySource, FinalRoot, Releasing };
	bool current();
	void fail(bool rejected = false);
	void readRoot(Step next);
	void opened(const QJsonObject &root, const QString &version);
	void readSource(Step next);
	void observedSource(const QJsonObject &source, bool missing);
	void readPayload(const QJsonObject &root, const QString &version, const QString &target, const QString &reservation, const QByteArray &payload);
	void release();
	void released(const QJsonObject &root, const QString &version, const QString &reservation);
	void finish(const CloudWorkspaceRecord &root, const QString &version);
	CloudWorkspaceOpen opener;
	CloudWorkspaceSourceReview sourceReview;
	CloudWorkspaceRead reader;
	QTimer deadline;
	std::unique_ptr<CloudWorkspaceActivationPlan> plan;
	std::unique_ptr<QLockFile> operationLock;
	CloudWorkspaceReference reference;
	CloudWorkspaceRecord root;
	CloudWorkspaceCopySource firstSource;
	Step step = Step::Idle;
	bool firstMissing = false, routePrepared = false, routeRequiredSet = false;
	quint64 generation = 0;
	QString token, choiceFile, choiceVersion, checkpointFile, bindingFile, routeFile, revision, target, reservation, operation, acceptance;
	QString planDirectory, readFile, rootVersion;
	QByteArray model;
	std::function<bool()> requireRoute, routeRequired, scopeCurrent;
	std::function<QByteArray()> localSnapshot;
};
#endif
