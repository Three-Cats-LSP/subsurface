// SPDX-License-Identifier: GPL-2.0
#include "testcloudsyncmanager.h"
#include "core/cloudsyncmanager.h"
#include "core/divelog.h"
#include "core/dive.h"
#include "core/fulltext.h"
#include "core/parse.h"
#include "core/pref.h"
#include "core/selection.h"
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QSignalSpy>
#include <QTimer>
#include <QTemporaryFile>
#include <QUrlQuery>
#include <QScopeGuard>
#include <QTcpSocket>
#include <QHostAddress>
#include <cstring>

namespace {
class FakeReply : public QNetworkReply {
public:
	FakeReply(const QNetworkRequest &request, const QByteArray &body, int status, QObject *parent, bool deferred = false) :
		QNetworkReply(parent), body(body)
	{
		setRequest(request);
		setUrl(request.url());
		setAttribute(QNetworkRequest::HttpStatusCodeAttribute, status);
		if (status >= 400)
			setError(QNetworkReply::ContentNotFoundError, "Request failed");
		open(QIODevice::ReadOnly);
		if (!deferred)
			QTimer::singleShot(0, this, [this] { complete(); });
	}
	void complete() { setFinished(true); emit finished(); }
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
	QByteArray posted;
	bool deferred = false;
	FakeReply *lastReply = nullptr;
protected:
	QNetworkReply *createRequest(Operation, const QNetworkRequest &request, QIODevice *outgoing) override
	{
		++requests;
		posted = outgoing ? outgoing->readAll() : QByteArray();
		timeout = request.transferTimeout();
		lastReply = new FakeReply(request, body, status, this, deferred);
		return lastReply;
	}
};
}

void TestCloudSyncManager::init()
{
	prefs = default_prefs;
	::divelog.clear();
	const QByteArray local("<divelog program='subsurface' version='3'><dives>"
		"<dive number='17' date='2026-01-02' time='10:00:00' duration='20:00 min'>"
		"<notes>Local notes must survive cloud failures.</notes>"
		"<divecomputer model='manually added dive'><depth max='18.0 m'/></divecomputer>"
		"</dive></dives></divelog>");
	QCOMPARE(parse_xml_buffer("cloud-regression", local.constData(), local.size(), &::divelog, nullptr), 0);
	::divelog.process_loaded_dives();
	QCOMPARE(::divelog.dives.size(), 1);
}

void TestCloudSyncManager::cleanup()
{
	::divelog.clear();
}

void TestCloudSyncManager::testLoopbackIgnoresBrowserRequests()
{
#if defined(Q_OS_ANDROID) || defined(Q_OS_IOS)
	QSKIP("Desktop loopback flow only");
#else
	FakeNetwork network;
	network.deferred = true;
	// Avoid writing credentials; complete with a provider rejection after the
	// browser requests, then verify that authorization can be retried.
	network.status = 400;
	network.body = R"({"error_description":"Synthetic token rejection"})";
	CloudSyncManager manager(&network);
	manager.tokens.clear();
	manager.activeProviderId = "dropbox";
	manager.activeClientId = "test-client";
	manager.activePkce = std::make_unique<OAuth2PkceSession>();
	// Google uses an ephemeral port, avoiding the fixed Dropbox callback port.
	manager.activeRedirectUri = manager.startLoopbackListener(cloudSyncProviderDescriptor(CloudSyncProviderType::GoogleDrive));
	QVERIFY(manager.activeRedirectUri.isValid());
	const quint16 port = manager.activeRedirectUri.port();
	const QByteArray state = manager.activePkce->parameters().state.toUtf8();
	const QByteArray target = "/oauth/callback?code=test-code&state=" + state;
	QTcpSocket callback;
	callback.connectToHost(QHostAddress::LocalHost, port);
	QTRY_COMPARE(callback.state(), QAbstractSocket::ConnectedState);
	callback.write("GET /oauth/callback?code=test-code&state=");
	callback.flush();
	QTest::qWait(30);
	QCOMPARE(network.requests, 0);
	QVERIFY(manager.authorizationInProgress());
	callback.write(state + " HTTP/1.1\r\nHost: localhost\r\n\r\n");
	callback.flush();
	QTRY_COMPARE(network.requests, 1);
	QVERIFY(manager.codeExchangeInProgress);
	QTRY_VERIFY(callback.bytesAvailable() > 0);
	const QByteArray response = callback.readAll();
	QVERIFY(response.contains("authorization received"));
	QVERIFY(!response.contains("Neo connected"));

	for (const QByteArray &path : {QByteArray("/favicon.ico"), QByteArray("/oauth/callback?code=stale&state=wrong"), target}) {
		QTcpSocket extra;
		extra.connectToHost(QHostAddress::LocalHost, port);
		QTRY_COMPARE(extra.state(), QAbstractSocket::ConnectedState);
		extra.write("GET " + path + " HTTP/1.1\r\nHost: localhost\r\n\r\n");
		extra.flush();
		QTRY_VERIFY(extra.bytesAvailable() > 0);
		QVERIFY(manager.authorizationInProgress());
		QVERIFY(manager.lastError().isEmpty());
		QCOMPARE(network.requests, 1);
	}
	network.lastReply->complete();
	QVERIFY(!manager.authorizationInProgress());
	QVERIFY(!manager.codeExchangeInProgress);
	QCOMPARE(manager.lastError(), QString("Synthetic token rejection"));
#endif
}

void TestCloudSyncManager::testGoogleDesktopCredentials_data()
{
	QTest::addColumn<QByteArray>("json");
	QTest::addColumn<bool>("valid");
	QTest::newRow("desktop") << QByteArray(R"({"installed":{"client_id":"test-client","client_secret":"test-secret"}})") << true;
	QTest::newRow("web client") << QByteArray(R"({"web":{"client_id":"test-client","client_secret":"test-secret"}})") << false;
	QTest::newRow("missing secret") << QByteArray(R"({"installed":{"client_id":"test-client"}})") << false;
	QTest::newRow("invalid JSON") << QByteArray("{broken") << false;
	QTest::newRow("oversize") << QByteArray(65537, ' ') << false;
}

void TestCloudSyncManager::testGoogleDesktopCredentials()
{
	QFETCH(QByteArray, json);
	QFETCH(bool, valid);
	const auto oldFile = qgetenv("SUBSURFACE_NEO_GOOGLE_CLIENT_FILE");
	const auto oldId = qgetenv("SUBSURFACE_NEO_GOOGLE_CLIENT_ID");
	const auto restore = qScopeGuard([&] {
		oldFile.isNull() ? qunsetenv("SUBSURFACE_NEO_GOOGLE_CLIENT_FILE") : qputenv("SUBSURFACE_NEO_GOOGLE_CLIENT_FILE", oldFile);
		oldId.isNull() ? qunsetenv("SUBSURFACE_NEO_GOOGLE_CLIENT_ID") : qputenv("SUBSURFACE_NEO_GOOGLE_CLIENT_ID", oldId);
	});
	QTemporaryFile file;
	QVERIFY(file.open());
	QCOMPARE(file.write(json), qint64(json.size()));
	file.close();
	qputenv("SUBSURFACE_NEO_GOOGLE_CLIENT_FILE", file.fileName().toUtf8());
	qunsetenv("SUBSURFACE_NEO_GOOGLE_CLIENT_ID");
	const auto client = googleDesktopOAuthClient();
	QCOMPARE(client.error.isEmpty(), valid);
#if !defined(Q_OS_ANDROID) && !defined(Q_OS_IOS) && !defined(__EMSCRIPTEN__)
	FakeNetwork network;
	CloudSyncManager manager(&network);
	manager.tokens.clear();
	for (const auto &entry : manager.providers()) {
		const auto row = entry.toMap();
		if (row.value("id").toString() == "google-drive") {
			QCOMPARE(row.value("configured").toBool(), valid);
			QCOMPARE(row.value("configurationError").toString().isEmpty(), valid);
		}
	}
#endif
	if (valid) {
		QCOMPARE(client.clientId, QString("test-client"));
		QCOMPARE(client.clientSecret, QString("test-secret"));
		qputenv("SUBSURFACE_NEO_GOOGLE_CLIENT_ID", "other-client");
		QVERIFY(!googleDesktopOAuthClient().error.isEmpty());
	}
	qunsetenv("SUBSURFACE_NEO_GOOGLE_CLIENT_FILE");
	QVERIFY(!googleDesktopOAuthClient().error.isEmpty());
}

void TestCloudSyncManager::testGoogleDesktopTokenRequests()
{
#if defined(Q_OS_ANDROID) || defined(Q_OS_IOS) || defined(__EMSCRIPTEN__)
	QSKIP("Desktop OAuth flow only");
#else
	const auto oldFile = qgetenv("SUBSURFACE_NEO_GOOGLE_CLIENT_FILE");
	const auto oldId = qgetenv("SUBSURFACE_NEO_GOOGLE_CLIENT_ID");
	const auto restore = qScopeGuard([&] {
		oldFile.isNull() ? qunsetenv("SUBSURFACE_NEO_GOOGLE_CLIENT_FILE") : qputenv("SUBSURFACE_NEO_GOOGLE_CLIENT_FILE", oldFile);
		oldId.isNull() ? qunsetenv("SUBSURFACE_NEO_GOOGLE_CLIENT_ID") : qputenv("SUBSURFACE_NEO_GOOGLE_CLIENT_ID", oldId);
	});
	QTemporaryFile file;
	QVERIFY(file.open());
	file.write(R"({"installed":{"client_id":"test-client","client_secret":"test-secret"}})");
	file.close();
	qputenv("SUBSURFACE_NEO_GOOGLE_CLIENT_FILE", file.fileName().toUtf8());
	qunsetenv("SUBSURFACE_NEO_GOOGLE_CLIENT_ID");
	FakeNetwork network;
	network.body = R"({"access_token":"new-access","expires_in":3600})";
	OAuth2TokenClient tokenClient(&network);
	QSignalSpy received(&tokenClient, &OAuth2TokenClient::tokenReceived);
	QSignalSpy errors(&tokenClient, &OAuth2TokenClient::tokenError);
	const auto &provider = cloudSyncProviderDescriptor(CloudSyncProviderType::GoogleDrive);
	tokenClient.exchangeAuthorizationCode(provider, "test-client", "code", "verifier", QUrl("http://127.0.0.1:1234/oauth/callback"));
	QTRY_COMPARE(received.count(), 1);
	QUrlQuery form(QString::fromUtf8(network.posted));
	QCOMPARE(form.queryItemValue("client_secret"), QString("test-secret"));
	QCOMPARE(form.queryItemValue("code_verifier"), QString("verifier"));
	tokenClient.refreshAccessToken(provider, "test-client", "existing-refresh");
	QTRY_COMPARE(received.count(), 2);
	form.setQuery(QString::fromUtf8(network.posted));
	QCOMPARE(form.queryItemValue("client_secret"), QString("test-secret"));
	QCOMPARE(form.queryItemValue("grant_type"), QString("refresh_token"));
	QCOMPARE(qvariant_cast<OAuth2TokenSet>(received.at(1).at(0)).refreshToken, QString("existing-refresh"));
	tokenClient.refreshAccessToken(provider, "different-client", "existing-refresh");
	QCOMPARE(errors.count(), 1);
	QCOMPARE(network.requests, 2);
	qunsetenv("SUBSURFACE_NEO_GOOGLE_CLIENT_FILE");
	tokenClient.refreshAccessToken(provider, "test-client", "existing-refresh");
	QCOMPARE(errors.count(), 2);
	QCOMPARE(network.requests, 2);
	// Google configuration must never affect Dropbox's public-client exchange.
	tokenClient.refreshAccessToken(cloudSyncProviderDescriptor(CloudSyncProviderType::Dropbox), "dropbox-client", "dropbox-refresh");
	QTRY_COMPARE(received.count(), 3);
	form.setQuery(QString::fromUtf8(network.posted));
	QVERIFY(!form.hasQueryItem("client_secret"));
#endif
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

void TestCloudSyncManager::testCorruptDownload_data()
{
	QTest::addColumn<QByteArray>("payload");
	QTest::addColumn<bool>("matchingChecksum");
	QTest::newRow("checksum mismatch") << QByteArray("<divelog/>") << false;
	QTest::newRow("truncated XML with matching hash") << QByteArray("<divelog><dives><dive number='1'>") << true;
	QTest::newRow("unrelated XML with matching hash") << QByteArray("<html><body>Service unavailable</body></html>") << true;
	QTest::newRow("trailing corruption") << QByteArray("<divelog/>garbage") << true;
	QTest::newRow("DTD") << QByteArray("<!DOCTYPE divelog [<!ENTITY note 'external'>]><divelog/>") << true;
	QTest::newRow("empty") << QByteArray() << true;
}

void TestCloudSyncManager::testCorruptDownload()
{
	QFETCH(QByteArray, payload);
	QFETCH(bool, matchingChecksum);
	FakeNetwork network;
	CloudSyncManager manager(&network);
	manager.tokens.clear();
	const QByteArray original = manager.serializeCurrentDiveLog();
	QVERIFY(!original.isEmpty());
	manager.syncProviderId = "dropbox";
	manager.syncOperation = CloudSyncManager::SyncOperation::SyncDownloadPayload;
	manager.syncLocalSha256 = CloudSyncManifest::sha256(original);
	manager.syncRemoteManifest = CloudSyncManifest::forPayload(matchingChecksum ? payload : "other bytes");
	manager.forceCloudDownload = true;
	QSignalSpy success(&manager, &CloudSyncManager::diveLogSyncFinished);
	manager.handleDownloadedDiveLog("dropbox", payload);
	QVERIFY(!manager.syncInProgress());
	QVERIFY(!manager.lastError().isEmpty());
	QCOMPARE(success.count(), 0);
	QCOMPARE(manager.serializeCurrentDiveLog(), original);
	QCOMPARE(network.requests, 0);
	// A rejected replacement must not leave the next manual retry stuck busy.
	OAuth2TokenSet token;
	token.accessToken = "test-token";
	manager.tokens.insert("dropbox", token);
	network.status = 503;
	manager.syncDiveLog("dropbox");
	QVERIFY(manager.syncInProgress());
	QTRY_VERIFY(!manager.syncInProgress());
	QCOMPARE(network.requests, 1);
}

void TestCloudSyncManager::testCanonicalCloudLog()
{
	FakeNetwork network;
	CloudSyncManager manager(&network);
	QByteArray replacement = manager.serializeCurrentDiveLog();
	QVERIFY(!replacement.isEmpty());
	replacement.replace("Local notes must survive cloud failures.", "Downloaded replacement notes.");
	QVERIFY(manager.applyCloudDiveLog(replacement));
	// The canonical importer normalizes its serialized representation; verify
	// the dive contents rather than requiring byte-identical XML after import.
	QCOMPARE(::divelog.dives.size(), 1);
	QCOMPARE(::divelog.dives[0]->number, 17);
	QCOMPARE(QString::fromStdString(::divelog.dives[0]->notes), QString("Downloaded replacement notes."));
	QCOMPARE(::divelog.dives[0]->duration.seconds, 1200);
	QCOMPARE(::divelog.dives[0]->maxdepth.mm, 18000);
	QCOMPARE(current_dive, ::divelog.dives[0].get());
	FullTextQuery query;
	query = QStringLiteral("replacement");
	const auto found = fulltext_find_dives(query, StringFilterMode::EXACT);
	QCOMPARE(found.dives.size(), size_t(1));
	QVERIFY(found.dive_matches(::divelog.dives[0].get()));
	query = QStringLiteral("survive");
	QVERIFY(fulltext_find_dives(query, StringFilterMode::EXACT).dives.empty());
}

void TestCloudSyncManager::testOrphanCloudLog()
{
	FakeNetwork network;
	network.body = "orphaned cloud log";
	CloudSyncManager manager(&network);
	manager.tokens.clear();
	OAuth2TokenSet token;
	token.accessToken = "test-token";
	manager.tokens.insert("dropbox", token);
	const QByteArray original = manager.serializeCurrentDiveLog();
	manager.syncProviderId = "dropbox";
	manager.syncOperation = CloudSyncManager::SyncOperation::SyncDownloadManifest;
	manager.syncLocalPayload = original;
	manager.syncLocalSha256 = CloudSyncManifest::sha256(original);
	QSignalSpy success(&manager, &CloudSyncManager::diveLogSyncFinished);
	QSignalSpy uploads(&manager, &CloudSyncManager::uploadFinished);
	manager.fileStore.fileNotFound(CloudSyncProviderType::Dropbox, "subsurface-neo-sync.json");
	QTRY_VERIFY(!manager.syncInProgress());
	QVERIFY(manager.lastError().contains("without its sync manifest"));
	QCOMPARE(network.requests, 1); // Only download the orphan; never upload over it.
	QCOMPARE(success.count(), 0);
	QCOMPARE(uploads.count(), 0);
	QCOMPARE(manager.serializeCurrentDiveLog(), original);
}

QTEST_GUILESS_MAIN(TestCloudSyncManager)
