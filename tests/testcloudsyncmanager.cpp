// SPDX-License-Identifier: GPL-2.0
#include "testcloudsyncmanager.h"
#include "core/cloudsyncmanager.h"
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QSignalSpy>
#include <QTimer>
#include <cstring>

namespace {
class FakeReply : public QNetworkReply {
public:
	FakeReply(const QNetworkRequest &request, const QByteArray &body, int status, QObject *parent) :
		QNetworkReply(parent), body(body)
	{
		setRequest(request);
		setUrl(request.url());
		setAttribute(QNetworkRequest::HttpStatusCodeAttribute, status);
		if (status >= 400)
			setError(QNetworkReply::ContentNotFoundError, "Request failed");
		open(QIODevice::ReadOnly);
		QTimer::singleShot(0, this, [this] { setFinished(true); emit finished(); });
	}
	void abort() override {}
	qint64 bytesAvailable() const override { return body.size() - offset + QNetworkReply::bytesAvailable(); }
protected:
	qint64 readData(char *data, qint64 maxSize) override
	{
		const qint64 count = qMin(maxSize, qint64(body.size()) - offset);
		if (!count)
			return -1;
		std::memcpy(data, body.constData() + offset, size_t(count));
		offset += count;
		return count;
	}
private:
	QByteArray body;
	qint64 offset = 0;
};

class FakeNetwork : public QNetworkAccessManager {
public:
	QByteArray body;
	int status = 200;
	int requests = 0;
	int timeout = 0;
protected:
	QNetworkReply *createRequest(Operation, const QNetworkRequest &request, QIODevice *) override
	{
		++requests;
		timeout = request.transferTimeout();
		return new FakeReply(request, body, status, this);
	}
};
}

void TestCloudSyncManager::testFileLookup_data()
{
	QTest::addColumn<QByteArray>("body");
	QTest::addColumn<bool>("missing");
	QTest::newRow("empty valid list") << QByteArray("{\"files\":[]}") << true;
	QTest::newRow("malformed") << QByteArray("not json") << false;
	QTest::newRow("missing files field") << QByteArray("{}") << false;
	QTest::newRow("invalid id") << QByteArray("{\"files\":[{}]}") << false;
	QTest::newRow("duplicates") << QByteArray("{\"files\":[{\"id\":\"a\"},{\"id\":\"b\"}]}") << false;
	QTest::newRow("incomplete page") << QByteArray("{\"files\":[],\"nextPageToken\":\"more\"}") << false;
}

void TestCloudSyncManager::testFileLookup()
{
	QFETCH(QByteArray, body);
	QFETCH(bool, missing);
	FakeNetwork network;
	network.body = body;
	CloudSyncFileStore store(&network);
	QSignalSpy errors(&store, &CloudSyncFileStore::operationError);
	QSignalSpy absent(&store, &CloudSyncFileStore::fileNotFound);
	store.download(CloudSyncProviderType::GoogleDrive, "test-token", "log.xml");
	QTRY_COMPARE(errors.count() + absent.count(), 1);
	QCOMPARE(absent.count(), missing ? 1 : 0);
	QCOMPARE(network.requests, 1);
	QCOMPARE(network.timeout, 30000);
}

void TestCloudSyncManager::testDropboxMissingFile_data()
{
	QTest::addColumn<QByteArray>("body");
	QTest::addColumn<bool>("missing");
	QTest::newRow("structured not found") << QByteArray("{\"error\":{\".tag\":\"path\",\"path\":{\".tag\":\"not_found\"}}}") << true;
	QTest::newRow("misleading text") << QByteArray("{\"error_summary\":\"authorization not found\"}") << false;
	QTest::newRow("permission error") << QByteArray("{\"error\":{\".tag\":\"path\",\"path\":{\".tag\":\"restricted_content\"}}}") << false;
}

void TestCloudSyncManager::testDropboxMissingFile()
{
	QFETCH(QByteArray, body);
	QFETCH(bool, missing);
	FakeNetwork network;
	network.body = body;
	network.status = 409;
	CloudSyncFileStore store(&network);
	QSignalSpy errors(&store, &CloudSyncFileStore::operationError);
	QSignalSpy absent(&store, &CloudSyncFileStore::fileNotFound);
	store.download(CloudSyncProviderType::Dropbox, "test-token", "log.xml");
	QTRY_COMPARE(errors.count() + absent.count(), 1);
	QCOMPARE(absent.count(), missing ? 1 : 0);
}

void TestCloudSyncManager::testAuthorizationFailureClearsBusy_data()
{
	QTest::addColumn<bool>("expired");
	QTest::newRow("disconnected") << false;
	QTest::newRow("expired without refresh") << true;
}

void TestCloudSyncManager::testAuthorizationFailureClearsBusy()
{
	QFETCH(bool, expired);
	FakeNetwork network;
	CloudSyncManager manager(&network);
	manager.tokens.clear(); // Never depend on credentials from the user's account.
	if (expired) {
		OAuth2TokenSet token;
		token.accessToken = "test-token";
		token.expiresAt = QDateTime::currentDateTimeUtc().addSecs(-60);
		manager.tokens.insert("dropbox", token);
	}
	manager.syncProviderId = "dropbox";
	manager.syncOperation = CloudSyncManager::SyncOperation::SyncDownloadManifest;
	QSignalSpy busy(&manager, &CloudSyncManager::syncInProgressChanged);
	manager.downloadBytes("dropbox", "subsurface-neo-sync.json");
	QVERIFY(!manager.syncInProgress());
	QVERIFY(!manager.lastError().isEmpty());
	QCOMPARE(busy.count(), 1);
	QCOMPARE(network.requests, 0);
}

void TestCloudSyncManager::testDownloadKeepsConcurrentEdits()
{
	FakeNetwork network;
	CloudSyncManager manager(&network);
	const QByteArray current = manager.serializeCurrentDiveLog();
	QVERIFY(!current.isEmpty());
	manager.syncProviderId = "dropbox";
	manager.syncOperation = CloudSyncManager::SyncOperation::SyncDownloadPayload;
	manager.syncLocalSha256 = CloudSyncManifest::sha256("older local snapshot");
	manager.syncRemoteManifest = CloudSyncManifest::forPayload("remote data");
	manager.handleDownloadedDiveLog("dropbox", "remote data");
	QVERIFY(!manager.syncInProgress());
	QVERIFY(manager.lastError().contains("changed during synchronization"));
	QCOMPARE(manager.serializeCurrentDiveLog(), current);
	QCOMPARE(network.requests, 0);
}

void TestCloudSyncManager::testManifestKeepsConcurrentEdits()
{
	FakeNetwork network;
	CloudSyncManager manager(&network);
	manager.syncProviderId = "dropbox";
	manager.syncOperation = CloudSyncManager::SyncOperation::SyncDownloadManifest;
	manager.syncLocalSha256 = CloudSyncManifest::sha256("older local snapshot");
	manager.handleDownloadedManifest("dropbox", CloudSyncManifest::forPayload("remote data").toJson());
	QVERIFY(!manager.syncInProgress());
	QVERIFY(manager.lastError().contains("changed during synchronization"));
	QCOMPARE(network.requests, 0);
}

void TestCloudSyncManager::testNetworkFailureAllowsRetry()
{
	FakeNetwork network;
	network.status = 503;
	network.body = "service unavailable";
	CloudSyncManager manager(&network);
	manager.tokens.clear();
	OAuth2TokenSet token;
	token.accessToken = "test-token";
	manager.tokens.insert("dropbox", token);
	manager.syncDiveLog("dropbox");
	QVERIFY(manager.syncInProgress());
	QTRY_VERIFY(!manager.syncInProgress());
	QVERIFY(!manager.lastError().isEmpty());
	QCOMPARE(network.requests, 1);
	manager.syncDiveLog("dropbox");
	QVERIFY(manager.syncInProgress());
	QTRY_VERIFY(!manager.syncInProgress());
	QCOMPARE(network.requests, 2);
}

void TestCloudSyncManager::testMissingTextDoesNotStartUpload()
{
	FakeNetwork network;
	CloudSyncManager manager(&network);
	manager.syncProviderId = "dropbox";
	manager.syncOperation = CloudSyncManager::SyncOperation::SyncDownloadManifest;
	manager.fileStore.operationError(CloudSyncProviderType::Dropbox,
		"subsurface-neo-sync.json", "Authorization not found");
	QVERIFY(!manager.syncInProgress());
	QCOMPARE(manager.lastError(), QString("Authorization not found"));
	QCOMPARE(network.requests, 0);
}

QTEST_GUILESS_MAIN(TestCloudSyncManager)
