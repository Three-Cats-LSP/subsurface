// SPDX-License-Identifier: GPL-2.0
#include "testcloudsyncreview.h"
#include "core/cloudsyncreview.h"
#include "core/cloudsyncmanifest.h"
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QSignalSpy>
#include <QUrlQuery>
#include <cstring>
#include <limits>

namespace {
CloudSyncManifest manifest()
{
	auto m = CloudSyncManifest::forPayload("synthetic XML");
	m.schemaVersion = 2;
	m.payloadFile = "subsurface-neo-" + m.revisionId + ".xml";
	return m;
}

class Reply : public QNetworkReply {
public:
	Reply(const QNetworkRequest &request, QByteArray body, int status, QObject *parent, bool defer) :
		QNetworkReply(parent), body(std::move(body))
	{
		setRequest(request); setUrl(request.url()); setAttribute(QNetworkRequest::HttpStatusCodeAttribute, status);
		if (status >= 400) setError(ContentAccessDenied, "synthetic error");
		open(ReadOnly);
		if (!defer) QTimer::singleShot(0, this, [this] { complete(); });
	}
	void header(const QByteArray &value) { setRawHeader("Dropbox-API-Result", value); }
	void complete() { setFinished(true); emit finished(); }
	void abort() override { setError(OperationCanceledError, "cancelled"); complete(); }
	qint64 bytesAvailable() const override { return body.size() - offset + QNetworkReply::bytesAvailable(); }
protected:
	qint64 readData(char *target, qint64 max) override {
		const qint64 size = qMin(max, qint64(body.size()) - offset);
		if (!size) return -1;
		std::memcpy(target, body.constData() + offset, size_t(size)); offset += size; return size;
	}
private:
	QByteArray body;
	qint64 offset = 0;
};

class ReviewNetwork : public QNetworkAccessManager {
public:
	CloudSyncManifest m = manifest();
	int fault = 0, listings = 0, downloads = 0;
	bool readOnly = true, defer = false;
	Reply *last = nullptr;
	QByteArray json(const QJsonObject &object) { return QJsonDocument(object).toJson(QJsonDocument::Compact); }
	QJsonObject entry(bool google, QString id, QString name, int size) {
		if (google) return {{"id", id}, {"name", name}, {"version", "1"}, {"mimeType", "application/octet-stream"}, {"size", QString::number(size)}};
		return {{"id", "id:" + id}, {"name", name}, {"rev", "rev-1"}, {".tag", "file"}, {"size", size}};
	}
protected:
	QNetworkReply *createRequest(Operation op, const QNetworkRequest &request, QIODevice *outgoing) override {
		const bool google = request.url().host() == "www.googleapis.com";
		const QString path = request.url().path();
		const auto sent = QJsonDocument::fromJson(outgoing ? outgoing->readAll() : QByteArray()).object();
		const bool list = path == "/drive/v3/files" || path == "/2/files/list_folder" || path == "/2/files/list_folder/continue";
		const bool download = path == "/drive/v3/files/head" || path == "/drive/v3/files/duplicate" || path == "/2/files/download";
		readOnly &= (google ? op == GetOperation : op == PostOperation) && (list || download) && request.transferTimeout() == 30000;
		QByteArray body, header;
		int status = 200;
		QByteArray content = fault == 7 ? QByteArray("{\"schema\":99}") : m.toJson();
		if (list) {
			++listings;
			const bool next = google ? !QUrlQuery(request.url()).queryItemValue("pageToken").isEmpty() : sent.contains("cursor");
			auto head = entry(google, "head", "subsurface-neo-sync.json", content.size());
			auto payload = entry(google, "payload", m.payloadFileName(), 100);
			if (fault == 6) payload.remove("size");
			if (fault == 15) head.remove(google ? "version" : "rev");
			if (fault == 9 && listings >= 3) head[google ? "version" : "rev"] = "changed";
			QJsonArray files = next ? QJsonArray{payload} : QJsonArray{head};
			if (fault == 1 && next) files.append(entry(google, "duplicate", "subsurface-neo-sync.json", content.size()));
			if (fault == 5 && next) files.append(head);
			if (fault == 8 && next) files = {};
			if (fault == 14 && !next) files[0] = entry(google, "head", "subsurface-neo-sync.json", 300000);
			if (fault == 16 && next) {
				auto folder = entry(google, "folder", "preserved folder", 0);
				if (google) folder["mimeType"] = "application/vnd.google-apps.folder"; else folder[".tag"] = "folder";
				folder.remove("size"); files.append(folder);
			}
			if (fault == 17) files = {};
			QJsonObject page = google ? QJsonObject{{"files", files}, {"incompleteSearch", fault == 4}}
				: QJsonObject{{"entries", files}, {"has_more", !next || fault == 3}, {"cursor", "next-page"}};
			if (google && (!next || fault == 3) && fault != 17) page["nextPageToken"] = "next-page";
			if (!google && fault == 17) page["has_more"] = false;
			if (!google && fault == 4) page.remove("has_more");
			body = fault == 2 ? QByteArray("invalid JSON") : json(page);
		} else if (download) {
			++downloads; body = content;
			if (fault == 10) status = 503;
			if (fault == 11) status = 401;
			if (fault == 12) body.append('x');
			header = json({{"id", "id:head"}, {"rev", fault == 13 ? "changed" : "rev-1"}});
		} else status = 500;
		last = new Reply(request, body, status, this, defer);
		last->header(header);
		return last;
	}
};
}

void TestCloudSyncReview::classification_data()
{
	QTest::addColumn<int>("fault");
	for (int fault = 0; fault < 9; ++fault) QTest::newRow(qPrintable(QString::number(fault))) << fault;
}

void TestCloudSyncReview::classification()
{
	QFETCH(int, fault);
	auto current = manifest(), recovery = manifest(), unknown = manifest();
	QList<CloudStorageEntry> entries{
		{"head", "subsurface-neo-sync.json", "1", 100}, {"payload", current.payloadFile, "1", 200},
		{"r1", "subsurface-neo-sync-recovered-r1.json", "1", 100},
		{"r2", "subsurface-neo-sync-recovered-r2.json", "1", 100}, {"rp", recovery.payloadFile, "1", 300},
		{"legacy", "subsurface-neo.xml", "1", 400}, {"unknown", unknown.payloadFile, "1", 500},
		{"other", "unrelated.json", "1", 600}};
	QMap<QString, QByteArray> bodies{{"head", current.toJson()}, {"r1", recovery.toJson()}, {"r2", recovery.toJson()}};
	if (fault == 1) entries.append(entries.first());
	if (fault == 2) entries[1].name = "missing.xml";
	if (fault == 3) bodies["r1"] = "broken";
	if (fault == 4) entries[4].folder = true;
	if (fault == 5) entries[1].bytes = -1;
	if (fault == 6) { auto duplicate = entries[1]; duplicate.id = "other-id"; entries.append(duplicate); }
	if (fault == 7) { entries[0].bytes = std::numeric_limits<qint64>::max(); }
	if (fault == 8) { current.schemaVersion = 1; entries[1].name = "unrelated.xml"; bodies["head"] = current.toJson(); }
	QString error;
	const auto report = CloudStorageReview::classify(entries, bodies, &error);
	const bool success = fault == 0 || fault == 8;
	QCOMPARE(error.isEmpty(), success);
	QCOMPARE(report.isEmpty(), !success);
	if (!success) return;
	QCOMPARE(report.value("entryCount").toInt(), 8);
	QCOMPARE(report.value("bytes").toLongLong(), qint64(2300));
	const auto categories = report.value("categories").toList();
	QCOMPARE(categories[0].toMap().value("count").toInt(), 2);
	QCOMPARE(categories[1].toMap().value("count").toInt(), 3); // Shared recovery payload counted once.
	QCOMPARE(categories[3].toMap().value("count").toInt(), 1);
	QVERIFY(!report.contains("deletableBytes"));
}

void TestCloudSyncReview::transport_data()
{
	QTest::addColumn<bool>("google"); QTest::addColumn<int>("fault");
	for (bool google : {false, true})
		for (int fault = 0; fault < 18; ++fault)
			QTest::newRow(qPrintable(QString(google ? "google-" : "dropbox-") + QString::number(fault))) << google << fault;
}

void TestCloudSyncReview::transport()
{
	QFETCH(bool, google); QFETCH(int, fault);
	ReviewNetwork network; network.fault = fault;
	CloudStorageReview review(&network);
	QSignalSpy finished(&review, &CloudStorageReview::finished), failed(&review, &CloudStorageReview::failed);
	review.start(google ? CloudSyncProviderType::GoogleDrive : CloudSyncProviderType::Dropbox, "synthetic");
	QTRY_COMPARE(finished.count() + failed.count(), 1);
	const bool success = fault == 0 || fault == 16 || fault == 17 || (google && fault == 13);
	QCOMPARE(finished.count(), success ? 1 : 0);
	QVERIFY(network.readOnly);
	if (success) {
		QCOMPARE(network.listings, fault == 17 ? 2 : 4);
		QCOMPARE(network.downloads, fault == 17 ? 0 : 1); // Never fetch XML or unrelated contents.
		const auto report = finished[0][0].toMap();
		QCOMPARE(report.value("entryCount").toInt(), fault == 17 ? 0 : fault == 16 ? 3 : 2);
	} else {
		QVERIFY(!failed[0][0].toString().isEmpty());
		QCOMPARE(failed[0][1].toBool(), fault == 11);
		// Failed pagination or download must leave the instance usable for retry.
		network.fault = 0; network.listings = 0; network.downloads = 0;
		review.start(google ? CloudSyncProviderType::GoogleDrive : CloudSyncProviderType::Dropbox, "synthetic");
		QTRY_COMPARE(finished.count(), 1);
		QCOMPARE(failed.count(), 1);
	}
}

void TestCloudSyncReview::cancellation_data()
{
	QTest::addColumn<bool>("google");
	QTest::newRow("google") << true; QTest::newRow("dropbox") << false;
}

void TestCloudSyncReview::cancellation()
{
	QFETCH(bool, google);
	ReviewNetwork network; network.defer = true;
	CloudStorageReview review(&network);
	QSignalSpy finished(&review, &CloudStorageReview::finished), failed(&review, &CloudStorageReview::failed);
	const auto provider = google ? CloudSyncProviderType::GoogleDrive : CloudSyncProviderType::Dropbox;
	review.start(provider, "synthetic");
	QCOMPARE(network.listings, 1);
	auto *old = network.last;
	review.cancel();
	old->complete(); // Late completion cannot restart pagination.
	QCOMPARE(network.listings, 1); QCOMPARE(failed.count(), 0); QCOMPARE(finished.count(), 0);
	network.defer = false; network.listings = 0;
	review.start(provider, "synthetic");
	QTRY_COMPARE(finished.count(), 1);
	QVERIFY(network.readOnly);
}

QTEST_GUILESS_MAIN(TestCloudSyncReview)
