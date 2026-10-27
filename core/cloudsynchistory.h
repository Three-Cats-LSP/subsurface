// SPDX-License-Identifier: GPL-2.0
#ifndef CLOUD_SYNC_HISTORY_H
#define CLOUD_SYNC_HISTORY_H

#include "cloudsyncfilestore.h"
#include <QJsonObject>
#include <QTimer>
#include <QVariantMap>
#include <memory>

// Experimental schema 3. All state changes are published through one provider
// CAS record. There is deliberately no retirement, expiry or deletion operation.
class CloudHistoryRecord {
public:
	QJsonObject value;
	static bool validEpoch(const QString &epoch);
	static QString rootName(const QString &epoch);
	static QString payloadName(const QString &epoch, const QString &ticket);
	static bool validXml(const QByteArray &payload);
	static CloudHistoryRecord create(const QString &provider, const QString &account,
		const QString &epoch, const QString &legacyHash);
	static CloudHistoryRecord parse(const QByteArray &data);
	bool valid() const;
	bool pristine() const;
	bool follows(const CloudHistoryRecord &previous) const;
	QByteArray json() const;
	QString head() const;
	QJsonObject ticket(const QString &id) const;
	bool upgradeLegacyProvenance();
	bool registerUpload(const QString &id, const QString &expectedHead, const QString &hash,
		qint64 bytes, const QString &restoreSource = {}, const QString &reservation = {}, const QString &legacySource = {});
	bool completeUpload(const QString &id, const QString &objectId, const QString &version);
	bool publish(const QString &id, const QString &expectedHead);
	bool reserve(const QString &id, const QString &commit);
	bool release(const QString &id);
	QVariantMap report() const;
private:
	bool replace(const QJsonObject &candidate);
};

// Protocol foundation only; no application action publishes these records yet.
// Publish in place of the original root with its exact-ID provider CAS. The
// different format fences ordinary history writers while retaining all state.
// There is intentionally no unfreeze, redirect, activation or deletion method.
class CloudHistoryFence {
public:
	QJsonObject value;
	static CloudHistoryFence freeze(const QString &id, const CloudHistoryRecord &root);
	static CloudHistoryFence seal(const QString &id, const CloudHistoryRecord &root,
		const QString &version, const CloudHistoryFence &coordinator);
	static CloudHistoryFence parse(const QByteArray &bytes);
	static QString archiveName(const QString &epoch, const QString &id);
	CloudHistoryRecord record() const;
	QJsonObject intent(const QString &sourceId) const;
	QByteArray json() const;
	bool valid() const;
	bool follows(const CloudHistoryFence &previous) const;
	bool intend(const QString &sourceId);
	// Caller must confirm exact source ID, archived name, revision and bytes
	// through the provider before publishing this acknowledgement by target CAS.
	bool acknowledge(const QString &sourceId, const CloudHistoryFence &source, const QString &version, const QString &name);
	bool acknowledges(const QString &sourceId, const CloudHistoryFence &source, const QString &version, const QString &name) const;
private:
	friend class CloudHistoryArchive;
	bool matchesSource(const QString &sourceId, const CloudHistoryFence &source, const QString &version, const QString &name) const;
	bool replace(const QJsonObject &candidate);
};

// A local checkpoint contains only the control record, never XML or credentials.
// Updates are atomic and serialized across processes using the same profile.
class CloudHistoryAuthority {
public:
	explicit CloudHistoryAuthority(const QString &fileName);
	bool load();
	bool observe(const QString &id, const CloudHistoryRecord &root);
	bool rebindPristine(const QString &oldId, const CloudHistoryRecord &archived,
		const QString &newId, const CloudHistoryRecord &root);
	// Preserve a confirmed handoff and the exact current checkpoint in a local
	// immutable recovery file. Does not change the active identity/checkpoint.
	// Caller supplies provider-verified observations; this performs no network I/O.
	// Returned path is empty on any mismatch, lock or storage failure.
	QString preserveHandoff(const QString &sourceId, const CloudHistoryFence &source,
		const QString &sourceVersion, const QString &sourceName,
		const QString &targetId, const CloudHistoryFence &target, const QString &targetVersion);
	QString id() const { return rootId; }
	CloudHistoryRecord record() const { return checkpoint; }
private:
	bool readUnlocked();
	bool writeUnlocked(const QString &id, const CloudHistoryRecord &root);
	QString fileName, rootId;
	CloudHistoryRecord checkpoint;
};

// Archive one already sealed Google source, retaining its complete body/ID.
// Does not elect, seal, acknowledge, rebind or activate an authority. The caller
// supplies fresh verified account scope and a durable coordinator intent.
class CloudHistoryArchive : public QObject {
	Q_OBJECT
public:
	explicit CloudHistoryArchive(QNetworkAccessManager *network, QObject *parent = nullptr);
	~CloudHistoryArchive() override;
	void start(const QString &token, const QString &account, const QString &sourceId, const CloudHistoryFence &source,
		const QString &sealedVersion, const QString &targetId, const CloudHistoryFence &coordinator, const std::function<bool()> &scopeCurrent);
	void cancel();
signals:
	// Fresh coordinator/control revision and exact archived-source revision.
	// Publication of a matching acknowledgement is a separate target CAS.
	void finished(const QJsonObject &coordinator, const QString &targetVersion, const QString &sourceVersion);
	void failed(const QString &message, bool tokenRejected);
private:
	enum class Step { Idle, CoordinatorBefore, ArchivedBefore, OriginalBefore, Rename, ArchivedAfter, CoordinatorAfter };
	bool current();
	void read(const QString &name, const QString &id, Step next);
	void received(const QString &name, const QByteArray &data, const QString &version, const QString &id);
	void fail(bool rejected = false);
	CloudSyncFileStore files;
	QTimer deadline;
	CloudHistoryFence source, coordinator;
	QString token, sourceId, targetId, rootName, archiveName, sealedVersion, archivedVersion;
	std::function<bool()> scopeCurrent;
	Step step = Step::Idle;
};

class CloudSyncHistory : public QObject {
	Q_OBJECT
public:
	explicit CloudSyncHistory(QNetworkAccessManager *network, QObject *parent = nullptr);
	~CloudSyncHistory() override;
	void start(CloudSyncProviderType provider, const QString &token, const QString &account,
		const QString &epoch, const QString &action, const QString &revision,
		const QByteArray &local, const std::function<bool()> &localUnchanged, const QString &authorityFile = {});
	void cancel();
signals:
	void finished(const QVariantMap &report, const QByteArray &restored);
	void failed(const QString &message, bool tokenRejected);
private:
	enum class Step { Idle, Legacy, Root, MigrationPayload, CheckLegacy, VerifyRoot,
		Upload, VerifyPayload, RestorePayload, VerifyReservation, RetryInitialization, VerifyPristineAlias,
		ReviewList, ReviewRoot, VerifyReviewRoot, VerifyReviewList };
	void reviewNextRoot();
	void completeRootReview();
	void read(const QString &name, Step next);
	void received(const QString &name, const QByteArray &data, const QString &version, const QString &id);
	void processRoot();
	bool retryInitialization();
	void writeRoot(const CloudHistoryRecord &next, const std::function<void()> &then);
	void registerUpload();
	void publish();
	void complete();
	void fail(const QString &message = {}, bool rejected = false);
	CloudSyncFileStore files;
	std::unique_ptr<CloudHistoryAuthority> authority;
	QTimer deadline;
	QTimer initializationRetry;
	CloudSyncProviderType provider = CloudSyncProviderType::GoogleDrive;
	QString token, account, epoch, action, revision, legacyHash, rootVersion, rootId, expectedHead, uploadId, reservationId;
	QString legacyPayload, legacyPayloadHash;
	CloudHistoryRecord root, pendingRoot;
	struct ReviewedRoot { QString id, version; CloudHistoryRecord record; };
	QList<ReviewedRoot> reviewedRoots;
	QStringList reviewIds;
	int reviewIndex = 0;
	QByteArray local, payload;
	Step step = Step::Idle;
	Step aliasStep = Step::Idle;
	bool retriedRootRead = false;
	int initializationRetries = 0;
	std::function<void()> continuation;
	std::function<bool()> localUnchanged;
};
#endif
