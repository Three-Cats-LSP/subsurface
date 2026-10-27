// SPDX-License-Identifier: GPL-2.0
#ifndef CLOUD_SYNC_HISTORY_H
#define CLOUD_SYNC_HISTORY_H

#include "cloudsyncfilestore.h"
#include <QJsonObject>
#include <QTimer>
#include <QVariantMap>

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

class CloudSyncHistory : public QObject {
	Q_OBJECT
public:
	explicit CloudSyncHistory(QNetworkAccessManager *network, QObject *parent = nullptr);
	~CloudSyncHistory() override;
	void start(CloudSyncProviderType provider, const QString &token, const QString &account,
		const QString &epoch, const QString &action, const QString &revision,
		const QByteArray &local, const std::function<bool()> &localUnchanged);
	void cancel();
signals:
	void finished(const QVariantMap &report, const QByteArray &restored);
	void failed(const QString &message, bool tokenRejected);
private:
	enum class Step { Idle, Legacy, Root, MigrationPayload, CheckLegacy, VerifyRoot,
		Upload, VerifyPayload, RestorePayload, VerifyReservation, RetryInitialization };
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
	QTimer deadline;
	QTimer initializationRetry;
	CloudSyncProviderType provider = CloudSyncProviderType::GoogleDrive;
	QString token, account, epoch, action, revision, legacyHash, rootVersion, rootId, expectedHead, uploadId, reservationId;
	QString legacyPayload, legacyPayloadHash;
	CloudHistoryRecord root, pendingRoot;
	QByteArray local, payload;
	Step step = Step::Idle;
	bool retriedRootRead = false;
	int initializationRetries = 0;
	std::function<void()> continuation;
	std::function<bool()> localUnchanged;
};
#endif
