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

// A local checkpoint contains only the control record, never XML or credentials.
// Updates are atomic and serialized across processes using the same profile.
class CloudHistoryAuthority {
public:
	explicit CloudHistoryAuthority(const QString &fileName);
	bool load();
	bool observe(const QString &id, const CloudHistoryRecord &root);
	bool rebindPristine(const QString &oldId, const CloudHistoryRecord &archived,
		const QString &newId, const CloudHistoryRecord &root);
	QString id() const { return rootId; }
	CloudHistoryRecord record() const { return checkpoint; }
private:
	bool readUnlocked();
	bool writeUnlocked(const QString &id, const CloudHistoryRecord &root);
	QString fileName, rootId;
	CloudHistoryRecord checkpoint;
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
