// SPDX-License-Identifier: GPL-2.0
#ifndef CLOUD_SYNC_WORKSPACE_SYNC_REVIEW_H
#define CLOUD_SYNC_WORKSPACE_SYNC_REVIEW_H
#include "cloudsyncworkspacedecision.h"
#include <QTimer>

// Read-only remote review plus checkpoint advancement. No upload, local
// replacement, activation or reservation release is performed. Results are
// point-in-time only and require fresh checks before a subsequent mutation.
class CloudWorkspaceSyncReview : public QObject {
	Q_OBJECT
public:
	explicit CloudWorkspaceSyncReview(QNetworkAccessManager *network, QObject *parent = nullptr);
	~CloudWorkspaceSyncReview() override;
	void start(const QString &token, const CloudWorkspaceReference &workspace, const QString &choiceFile,
		const QString &choiceVersion, const QString &checkpointFile, const QString &bindingFile,
		const QByteArray &expectedModel, const std::function<bool()> &scopeCurrent,
		const std::function<QByteArray()> &localSnapshot);
	void cancel();
signals:
	void finished(int action, const QJsonObject &root, const QString &version, const QString &bindingRevision);
	void failed(const QString &message, bool tokenRejected);
private:
	enum class Step { Idle, FirstRoot, FirstSource, FinalRoot, FinalSource };
	bool current();
	void fail(bool rejected = false);
	void readRoot(Step next);
	void observedRoot(const QJsonObject &root, const QString &version);
	void readSource(Step next);
	void observedSource(const QJsonObject &source, bool missing);
	CloudWorkspaceOpen opener;
	CloudWorkspaceSourceReview sourceReview;
	QTimer deadline;
	CloudWorkspaceReference reference;
	CloudWorkspaceRecord root;
	CloudWorkspaceCopySource firstSource;
	QJsonObject bindingValue;
	Step step = Step::Idle;
	bool firstMissing = false;
	quint64 generation = 0;
	QString token, choiceFile, choiceVersion, checkpointFile, bindingFile, rootVersion, bindingRevision;
	QByteArray localModel;
	std::function<bool()> scopeCurrent;
	std::function<QByteArray()> localSnapshot;
};
#endif
