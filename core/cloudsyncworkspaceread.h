// SPDX-License-Identifier: GPL-2.0
#ifndef CLOUD_SYNC_WORKSPACE_READ_H
#define CLOUD_SYNC_WORKSPACE_READ_H
#include "cloudsyncworkspace.h"
#include "cloudsyncworkspacearchive.h"
#include <QElapsedTimer>

// Durable ownership of one reservation and immutable snapshot metadata.
// XML/tokens are never stored. Releasing/released reads cannot resume.
class CloudWorkspaceReadIntent {
public:
	CloudWorkspaceReadIntent(const QString &file, const CloudWorkspaceReference &reference, const QString &commit, const QString &reservation);
	bool prepare();
	bool inspect();
	bool reload();
	bool pin(const QJsonObject &snapshot);
	bool markReserved();
	bool markVerified();
	bool beginRelease();
	bool markReleased();
	QString state() const;
	QJsonObject snapshot() const;
	bool matches(const CloudWorkspaceReference &ref, const QString &commit, const QString &reservation) const;
private:
	bool validSnapshot(const QJsonObject &snapshot) const;
	bool read();
	bool write(const QJsonObject &next);
	bool advance(const QStringList &from, const QString &to, const QJsonObject &snapshot = {});
	QString fileName, commit;
	CloudWorkspaceReference reference;
	QJsonObject scope, saved;
};

// Reserved exact reads only; no local application or activation. The caller
// verifies account freshly and supplies its continuing account/local-edit guard.
// Successful read keeps the reservation active for the caller's next guarded
// step. Explicit release fences even a not-yet-visible registration by CAS.
class CloudWorkspaceRead : public QObject {
	Q_OBJECT
public:
	enum class Action { Read, Release };
	explicit CloudWorkspaceRead(QNetworkAccessManager *network, QObject *parent = nullptr);
	~CloudWorkspaceRead() override;
	void start(Action action, const QString &token, const CloudWorkspaceReference &workspace, const QString &commit, const QString &reservation,
		const QString &choiceFile, const QString &choiceVersion, const QString &checkpointFile, const QString &intentFile, const std::function<bool()> &scopeCurrent);
	void cancel();
signals:
	// Point-in-time validated bytes with an active reservation. The caller must
	// retain its scope/local-edit protections and revalidate before application.
	void finished(const QJsonObject &root, const QString &version, const QString &commit, const QString &reservation, const QByteArray &payload);
	void released(const QJsonObject &root, const QString &version, const QString &reservation);
	void failed(const QString &message, bool tokenRejected);
private:
	enum class Step { Idle, Starting, InitialRoot, WriteRoot, ConfirmRoot, BeforePayload, Payload, FinalRoot };
	bool current();
	void begin();
	void retryStartupOrFail(const QString &path);
	void fail(bool rejected = false);
	void readRoot(Step next);
	void received(const QString &name, const QByteArray &bytes, const QString &version, const QString &id);
	void receivedRoot(const QJsonObject &value, const QString &version);
	void writeRoot(const CloudWorkspaceRecord &next);
	QJsonObject target() const;
	bool pinned() const;
	bool active() const;
	void reserve();
	void access();
	void release();
	void finishRelease();
	CloudSyncFileStore files;
	CloudWorkspaceOpen rootReader;
	CloudWorkspaceArchive archiver;
	QTimer deadline, startupRetry;
	QElapsedTimer startupWait;
	CloudWorkspaceReference reference;
	CloudWorkspaceRecord root, pending;
	std::unique_ptr<CloudWorkspaceReadIntent> intent;
	Action action = Action::Read;
	Step step = Step::Idle;
	CloudSyncProviderType provider = CloudSyncProviderType::GoogleDrive;
	QString token, commit, reservation, choiceFile, choiceVersion, checkpointFile, intentFile, rootVersion;
	QByteArray payload;
	std::function<bool()> scopeCurrent;
};
#endif
