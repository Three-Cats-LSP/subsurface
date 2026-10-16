// SPDX-License-Identifier: GPL-2.0
#ifndef CLOUD_SYNC_REVIEW_H
#define CLOUD_SYNC_REVIEW_H

#include "cloudsyncprovider.h"
#include <QMap>
#include <QObject>
#include <QPointer>
#include <QSet>
#include <QTimer>
#include <QVariantMap>
#include <functional>

class QNetworkAccessManager;
class QNetworkReply;
class QUrl;

struct CloudStorageEntry {
	QString id, name, version;
	qint64 bytes = 0;
	bool folder = false;
	bool operator==(const CloudStorageEntry &other) const;
};

// A separate read-only transport: never call the sync lookup, which can rename
// competing Google manifests. No upload, rename or delete operation exists here.
class CloudStorageReview : public QObject {
	Q_OBJECT
public:
	explicit CloudStorageReview(QNetworkAccessManager *network, QObject *parent = nullptr);
	~CloudStorageReview() override;
	void start(CloudSyncProviderType provider, const QString &accessToken);
	void cancel();
	static bool isManifest(const QString &name);
	static QVariantMap classify(const QList<CloudStorageEntry> &entries,
		const QMap<QString, QByteArray> &manifests, QString *error);
signals:
	void finished(const QVariantMap &report);
	void failed(const QString &message, bool tokenRejected);
private:
	void listPage(const QString &cursor = {});
	void readNextManifest();
	void fail(const QString &message, bool rejected = false);
	void request(const QUrl &url, const QByteArray &data, const QByteArray &argument,
		qint64 limit, const std::function<void(const QByteArray &, QNetworkReply *)> &done);
	QNetworkAccessManager *network;
	QPointer<QNetworkReply> pending;
	QTimer deadline;
	CloudSyncProviderType provider = CloudSyncProviderType::GoogleDrive;
	QString token;
	QMap<QString, CloudStorageEntry> entries, original;
	QMap<QString, QByteArray> manifests;
	QStringList manifestIds;
	QSet<QString> cursors;
	int pages = 0;
	bool verifying = false;
};
#endif
