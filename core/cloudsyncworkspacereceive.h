// SPDX-License-Identifier: GPL-2.0
#ifndef CLOUD_SYNC_WORKSPACE_RECEIVE_H
#define CLOUD_SYNC_WORKSPACE_RECEIVE_H

#include "cloudsyncworkspacereceipt.h"
#include <QElapsedTimer>
#include <QTimer>
#include <memory>
class QLockFile;

// Reserved remote receive with private crash recovery and explicit persistence.
// The caller must freshly verify account scope, use a deterministic pure
// projection, and confirm actual durable local saving. No application caller
// or activation is provided by this service.
class CloudWorkspaceReceive : public QObject {
	Q_OBJECT
public:
	explicit CloudWorkspaceReceive(QNetworkAccessManager *network, QObject *parent = nullptr);
	~CloudWorkspaceReceive() override;
	void start(const QString &token, const CloudWorkspaceReference &workspace,
		const QString &baseline, const QString &target, const QString &reservation,
		const QString &choiceFile, const QString &choiceVersion, const QString &checkpointFile,
		const QString &bindingFile, const QString &bindingRevision, const QString &nextRevision,
		const QString &readIntentFile, const QString &recoveryDirectory, const QByteArray &expectedBefore,
		const std::function<bool()> &scopeCurrent, const std::function<QByteArray()> &localSnapshot,
		const std::function<QByteArray(const QByteArray &, const QByteArray &)> &project,
		const std::function<bool(const QByteArray &, const QByteArray &, const QByteArray &)> &apply,
		const QJsonObject &reviewedBinding = {});
	void confirmPersisted();
	void cancel();
signals:
	void applied(const QString &recoveryDirectory);
	// A completed point-in-time observation, not permission for a later sync.
	void finished(const QJsonObject &root, const QString &version, const QString &target);
	void failed(const QString &message, bool tokenRejected);
private:
	enum class Step { Idle, Starting, Reading, Persistence, FinalRoot, Releasing };
	void begin();
	void retryStartupOrFail(const QString &path);
	bool current(bool local = true);
	void fail(bool rejected = false);
	void received(const QJsonObject &root, const QByteArray &raw);
	void readFinalRoot();
	void completed(const QJsonObject &root, const QString &version);
	void released(const QJsonObject &root, const QString &version, const QString &id);
	CloudWorkspaceRead reader;
	CloudWorkspaceOpen opener;
	QTimer deadline, startupRetry;
	QElapsedTimer startupWait;
	std::unique_ptr<CloudWorkspaceReceiveReceipt> receipt;
	std::unique_ptr<QLockFile> operationLock;
	CloudWorkspaceReference reference;
	QJsonObject bindingBefore, reviewedBinding;
	Step step = Step::Idle;
	quint64 generation = 0;
	bool recoveryEstablished = false;
	QString token, baseline, target, reservation, choiceFile, choiceVersion, checkpointFile;
	QString bindingFile, bindingRevision, nextRevision, readFile, recoveryDirectory;
	QByteArray before, raw, after;
	std::function<bool()> scopeCurrent;
	std::function<QByteArray()> localSnapshot;
	std::function<QByteArray(const QByteArray &, const QByteArray &)> projectPayload;
	std::function<bool(const QByteArray &, const QByteArray &, const QByteArray &)> applyPayload;
};
#endif
