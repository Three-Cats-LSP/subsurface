// SPDX-License-Identifier: GPL-2.0
#ifndef CLOUD_SYNC_WORKSPACE_APPLY_H
#define CLOUD_SYNC_WORKSPACE_APPLY_H
#include "cloudsyncworkspaceread.h"
class QLockFile;

// Metadata and private, retained before.xml/after.xml recovery copies. An
// applying receipt is durable before the caller can replace local contents.
class CloudWorkspaceApplyIntent {
public:
	CloudWorkspaceApplyIntent(const QString &directory, const CloudWorkspaceReference &workspace,
		const QString &choiceVersion, const QString &commit, const QString &reservation);
	bool inspect();
	bool reload();
	bool prepare(const QByteArray &before, const QByteArray &after, const CloudWorkspaceRecord &root);
	bool beginApply();
	bool applied(const QByteArray &observed);
	bool persisted(const QByteArray &observed);
	QString state() const;
	QJsonObject proof() const { return saved; }
	QByteArray before() const;
	QByteArray after() const;
private:
	bool read(bool verifyXml = true);
	bool valid(const QJsonObject &body) const;
	bool write(const QJsonObject &body);
	bool transition(const QString &from, const QString &to, const QByteArray &observed = {});
	QString directory;
	CloudWorkspaceReference reference;
	QJsonObject scope, saved;
};

// Guarded local application of an already owned, committed restore. The caller
// verifies account freshly and confirms replacement before start. applyPayload
// runs synchronously; applied() requests durable application-level persistence.
// confirmPersisted() is an explicit acknowledgement after that persistence.
// No baseline, activation, automatic release, deletion or application UI caller.
class CloudWorkspaceApply : public QObject {
	Q_OBJECT
public:
	explicit CloudWorkspaceApply(QNetworkAccessManager *network, QObject *parent = nullptr);
	~CloudWorkspaceApply() override;
	void start(const QString &token, const CloudWorkspaceReference &workspace, const QString &commit, const QString &reservation,
		const QString &choiceFile, const QString &choiceVersion, const QString &checkpointFile,
		const QString &sourceReadIntentFile, const QString &uploadIntentFile, const QString &applyReadIntentFile,
		const QString &recoveryDirectory, const QByteArray &expectedBefore,
		const std::function<bool()> &scopeCurrent, const std::function<QByteArray()> &localSnapshot,
		const std::function<bool(const QByteArray &)> &applyPayload);
	void confirmPersisted();
	void cancel();
signals:
	void applied(const QString &recoveryDirectory);
	// Point-in-time head and local-content observations, not sync authority.
	void finished(const QJsonObject &root, const QString &version, const QString &commit, bool headMatches, bool localMatches);
	void failed(const QString &message, bool tokenRejected);
private:
	enum class Step { Idle, Starting, Reading, Persistence, FinalRoot };
	void begin();
	bool current(bool local = true);
	bool proofs(const CloudWorkspaceRecord &root, bool active);
	void retryStartupOrFail(const QString &path);
	void fail(bool rejected = false);
	void received(const QJsonObject &root, const QString &version, const QByteArray &payload);
	void readFinalRoot();
	void completed(const QJsonObject &root, const QString &version);
	CloudWorkspaceRead reader;
	CloudWorkspaceOpen opener;
	QTimer deadline, startupRetry;
	QElapsedTimer startupWait;
	std::unique_ptr<CloudWorkspaceApplyIntent> intent;
	std::unique_ptr<QLockFile> operationLock;
	CloudWorkspaceReference reference;
	Step step = Step::Idle;
	bool recoveryEstablished = false;
	quint64 generation = 0;
	QString token, commit, reservation, choiceFile, choiceVersion, checkpointFile, sourceFile, uploadFile, readFile, recoveryDirectory;
	QByteArray beforePayload, afterPayload;
	std::function<bool()> scopeCurrent;
	std::function<QByteArray()> localSnapshot;
	std::function<bool(const QByteArray &)> applyPayload;
};
#endif
