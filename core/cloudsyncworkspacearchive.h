// SPDX-License-Identifier: GPL-2.0
#ifndef CLOUD_SYNC_WORKSPACE_ARCHIVE_H
#define CLOUD_SYNC_WORKSPACE_ARCHIVE_H
#include "cloudsyncworkspace.h"
class QLockFile;

// Rolls a freshly verified root into an immutable page without deleting or
// forgetting any tickets, reservations or provenance. The caller keeps its
// existing operation/review guard and resumes the same operation afterwards.
class CloudWorkspaceArchive : public QObject {
	Q_OBJECT
public:
	explicit CloudWorkspaceArchive(QNetworkAccessManager *network, QObject *parent = nullptr);
	~CloudWorkspaceArchive() override;
	void start(const QString &token, const CloudWorkspaceRecord &root, const QString &rootVersion,
		const QString &choiceFile, const QString &choiceVersion, const QString &checkpointFile,
		const QString &intentFile, const std::function<bool()> &scopeCurrent);
	void cancel();
signals:
	void finished(const QJsonObject &root, const QString &version);
	void failed(const QString &message, bool tokenRejected);
private:
	enum class Step { Idle, Create, Archive, Root, Publish, Confirm };
	bool current();
	bool prepare();
	bool load();
	bool save(const QJsonObject &next);
	bool advance(const QString &from, const QString &to, const QString &object = {}, const QString &version = {});
	void fail(bool rejected = false);
	void readArchive();
	void readRoot();
	void receivedArchive(CloudSyncProviderType provider, const QString &name, const QByteArray &bytes, const QString &version, const QString &id);
	void receivedRoot(const QJsonObject &root, const QString &version);
	CloudSyncFileStore files;
	CloudWorkspaceOpen reader;
	QTimer deadline;
	std::unique_ptr<QLockFile> operationLock;
	CloudWorkspaceRecord base, pending;
	CloudWorkspaceReference reference;
	QJsonObject scope, saved;
	QString token, baseVersion, choiceFile, choiceVersion, checkpointFile, intentFile;
	std::function<bool()> scopeCurrent;
	CloudSyncProviderType provider = CloudSyncProviderType::GoogleDrive;
	Step step = Step::Idle;
};
#endif
