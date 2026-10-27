// SPDX-License-Identifier: GPL-2.0
#ifndef CLOUD_SYNC_WORKSPACE_H
#define CLOUD_SYNC_WORKSPACE_H
#include "cloudsynchistory.h"
#include <QJsonObject>
#include <QString>

// An explicitly selected workspace is identified by its physical root, not a
// filename/list ordering. A reference is not authorization or a verified root.
class CloudWorkspaceReference {
public:
	QJsonObject value;
	static CloudWorkspaceReference create(const QString &provider, const QString &account,
		const QString &epoch, const QString &rootId);
	static CloudWorkspaceReference parse(const QByteArray &bytes);
	bool valid() const;
	QByteArray json() const;
	QString key() const;
	QString rootName() const;
	QString payloadName(const QString &ticket) const;
	bool matches(const QString &provider, const QString &account) const;
};

// Local user choice only. Callers must verify the provider/account/root before
// selecting and before every operation. This never changes XML, credentials,
// existing cloud history checkpoints or a provider's schema 2 sync baseline.
class CloudWorkspaceSelection {
public:
	CloudWorkspaceSelection(const QString &fileName, const QString &provider, const QString &account);
	bool load();
	bool select(const QString &expectedVersion, const CloudWorkspaceReference &reference);
	CloudWorkspaceReference selected() const;
	QString version() const { return choiceVersion; }
	QJsonObject knownReferences() const { return references; }
private:
	bool readUnlocked();
	QString fileName, provider, account, selectedKey, choiceVersion;
	QJsonObject references;
};

// A separate, self-bound control envelope. Journal transitions reuse the history
// validator, but every wire payload name belongs to this physical workspace.
// Existing schema 3 history readers reject this envelope. No expiry/deletion.
class CloudWorkspaceRecord {
public:
	QJsonObject value;
	static CloudWorkspaceRecord create(const CloudWorkspaceReference &reference, const QString &legacyHash);
	static CloudWorkspaceRecord parse(const QByteArray &bytes);
	CloudWorkspaceReference reference() const;
	bool valid() const;
	bool follows(const CloudWorkspaceRecord &previous) const;
	QByteArray json() const;
	QString head() const;
	QJsonObject ticket(const QString &id) const;
	bool registerUpload(const QString &id, const QString &expectedHead, const QString &hash, qint64 bytes,
		const QString &restoreSource = {}, const QString &reservation = {});
	bool completeUpload(const QString &id, const QString &objectId, const QString &version);
	bool publish(const QString &id, const QString &expectedHead);
	bool reserve(const QString &id, const QString &commit);
	bool release(const QString &id);
private:
	CloudHistoryRecord journal() const;
	bool replaceJournal(const CloudHistoryRecord &journal);
};

// A monotonic checkpoint for one immutable workspace reference. Observations
// must already be provider verified. This preserves metadata only and cannot
// change the selected workspace, credentials, XML or the old sync baseline.
class CloudWorkspaceAuthority {
public:
	CloudWorkspaceAuthority(const QString &fileName, const CloudWorkspaceReference &reference);
	bool load();
	bool observe(const CloudWorkspaceRecord &record, const QString &version, bool requireExisting = false);
	CloudWorkspaceRecord record() const { return checkpoint; }
	QString version() const { return observedVersion; }
private:
	bool readUnlocked();
	QString fileName, observedVersion;
	CloudWorkspaceReference reference;
	CloudWorkspaceRecord checkpoint;
};

// Durable creation state; submitted without an identity is uncertain forever.
// Never resubmit that intent or replace a known identity through a filename.
class CloudWorkspaceCreationIntent {
public:
	CloudWorkspaceCreationIntent(const QString &fileName, const QString &provider, const QString &account,
		const QString &epoch, const QString &nonce, const QString &originHash);
	bool prepare();
	bool reload();
	bool submit();
	bool identify(const QString &id);
	bool beginBinding();
	bool bind();
	QString state() const;
	QString id() const;
	QString rootName() const;
	QByteArray bootstrap() const;
	CloudWorkspaceReference reference() const;
private:
	bool readUnlocked();
	bool writeUnlocked(const QJsonObject &next);
	bool advance(const QString &state, const QString &id = {});
	QString fileName;
	QJsonObject scope, saved;
};

// Create/bind an empty workspace; no payload publication, local selection,
// migration, XML replacement or credentials. The caller supplies verified scope.
class CloudWorkspaceCreate : public QObject {
	Q_OBJECT
public:
	explicit CloudWorkspaceCreate(QNetworkAccessManager *network, QObject *parent = nullptr);
	~CloudWorkspaceCreate() override;
	void start(const QString &intentFile, const QString &checkpointDirectory, const QString &provider, const QString &token,
		const QString &account, const QString &epoch, const QString &nonce, const QString &originHash, const std::function<bool()> &scopeCurrent);
	void cancel();
signals:
	void finished(const QJsonObject &record, const QString &version, const QString &checkpointFile);
	void failed(const QString &message, bool tokenRejected);
private:
	enum class Step { Idle, Create, Read, Bind, Confirm };
	bool current();
	void read(Step step);
	void received(CloudSyncProviderType provider, const QString &name, const QByteArray &bytes, const QString &version, const QString &id);
	void fail(bool rejected = false);
	CloudSyncFileStore files;
	QTimer deadline;
	std::unique_ptr<CloudWorkspaceCreationIntent> intent;
	std::function<bool()> scopeCurrent;
	CloudSyncProviderType provider = CloudSyncProviderType::GoogleDrive;
	QString token, checkpointDirectory, originHash;
	Step step = Step::Idle;
};

// Read-only exact-root verification. The caller must supply a freshly verified
// provider/account and retain/recheck scope before recording an explicit choice.
// No filename lookup, creation, local binding, XML application or cloud write.
class CloudWorkspaceOpen : public QObject {
	Q_OBJECT
public:
	explicit CloudWorkspaceOpen(QNetworkAccessManager *network, QObject *parent = nullptr);
	~CloudWorkspaceOpen() override;
	void start(const QString &token, const QString &provider, const QString &account,
		const CloudWorkspaceReference &reference, const std::function<bool()> &scopeCurrent,
		const CloudWorkspaceRecord &checkpoint = {});
	void cancel();
signals:
	void finished(const QJsonObject &record, const QString &version);
	void failed(const QString &message, bool tokenRejected);
private:
	void fail(bool rejected = false);
	CloudSyncFileStore files;
	QTimer deadline;
	CloudWorkspaceReference reference;
	CloudWorkspaceRecord checkpoint;
	std::function<bool()> scopeCurrent;
	CloudSyncProviderType provider = CloudSyncProviderType::GoogleDrive;
	bool active = false;
};
#endif
