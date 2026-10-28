// SPDX-License-Identifier: GPL-2.0
#ifndef CLOUD_SYNC_WORKSPACE_UPLOAD_H
#define CLOUD_SYNC_WORKSPACE_UPLOAD_H
#include "cloudsyncworkspace.h"
#include "cloudsyncworkspacearchive.h"
#include <QElapsedTimer>

// Local metadata for one canonical-XML snapshot; no XML or credentials stored.
// A submitted create without its directly acknowledged ID is never replayed.
class CloudWorkspaceUploadIntent {
public:
	CloudWorkspaceUploadIntent(const QString &file, const CloudWorkspaceReference &workspace, const QString &hash, qint64 bytes,
		const QString &ticket, const QString &parent, const QJsonObject &restore = {});
	bool prepare();
	bool inspect();
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
	QJsonObject scope, saved;
};

// Workspace-local version publication only. Caller verifies account afresh and
// supplies a continuing account/local-edit guard. No selection, XML application,
// legacy acknowledgement, baseline, activation or deletion operation is added.
class CloudWorkspaceUpload : public QObject {
	Q_OBJECT
public:
	explicit CloudWorkspaceUpload(QNetworkAccessManager *network, QObject *parent = nullptr);
	~CloudWorkspaceUpload() override;
	void start(const QString &token, const CloudWorkspaceReference &workspace, const QByteArray &payload,
		const QString &choiceFile, const QString &choiceVersion, const QString &checkpointFile,
		const QString &intentFile, const QString &ticket, const QString &parent, const std::function<bool()> &scopeCurrent);
	// Publishes a new owned copy of a reserved version. No local XML application
	// or reservation release; a caller must keep guarding the current local log.
	void startRestore(const QString &token, const CloudWorkspaceReference &workspace, const QByteArray &payload,
		const QString &choiceFile, const QString &choiceVersion, const QString &checkpointFile,
		const QString &intentFile, const QString &ticket, const QString &parent,
		const QString &readIntentFile, const QString &source, const QString &reservation, const std::function<bool()> &scopeCurrent);
	void cancel();
signals:
	// Whether this ticket is head in the final exact root observation. The
	// caller must still revalidate current state before any later operation.
	void finished(const QJsonObject &root, const QString &version, const QString &ticket, bool headMatches);
	void failed(const QString &message, bool tokenRejected);
private:
	enum class Step { Idle, Starting, InitialRoot, WriteRoot, ConfirmRoot, Upload, Payload, PayloadBeforePublish, PublishRoot, PayloadAfter, FinalRoot, SourcePayload, SourceRoot };
	void startImpl(const QString &token, const CloudWorkspaceReference &workspace, const QByteArray &payload,
		const QString &choiceFile, const QString &choiceVersion, const QString &checkpointFile,
		const QString &intentFile, const QString &ticket, const QString &parent,
		const QString &readIntentFile, const QString &source, const QString &reservation, const std::function<bool()> &scopeCurrent);
	void begin();
	void retryStartupOrFail(const QString &path);
	bool current();
	bool matchesTicket() const;
	bool restoreCurrent();
	bool restoreMatches(bool active) const;
	void readSource(const std::function<void()> &then);
	bool accepts(const QByteArray &bytes) const;
	void fail(bool rejected = false);
	void readRoot(Step next);
	void received(const QString &name, const QByteArray &bytes, const QString &version, const QString &id);
	void receivedRoot(const QJsonObject &value, const QString &version);
	void writeRoot(const CloudWorkspaceRecord &next, const std::function<void()> &then);
	void continueUpload();
	void prepareUpload();
	void uploaded();
	void publish();
	void complete();
	CloudSyncFileStore files;
	CloudWorkspaceOpen rootReader;
	CloudWorkspaceArchive archiver;
	QTimer deadline, startupRetry;
	QElapsedTimer startupWait;
	std::unique_ptr<CloudWorkspaceUploadIntent> intent;
	CloudWorkspaceReference reference;
	CloudWorkspaceRecord root, pending;
	CloudSyncProviderType provider = CloudSyncProviderType::GoogleDrive;
	QString token, choiceFile, choiceVersion, checkpointFile, rootVersion, payloadHash;
	QString initialIntentFile, initialTicket, initialParent;
	QString readIntentFile;
	QJsonObject restoreProof;
	QByteArray payload;
	Step step = Step::Idle;
	std::function<bool()> scopeCurrent;
	std::function<void()> continuation;
	std::function<void()> sourceContinuation;
};
#endif
