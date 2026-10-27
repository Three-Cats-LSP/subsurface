// SPDX-License-Identifier: GPL-2.0
#include "testcloudsyncmanager.h"
#include "core/cloudsyncmanager.h"
#include "core/cloudcredentialstore.h"
#include "core/subsurfacestartup.h"
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
#include <QTemporaryDir>
#include <QDir>
#include <QFile>
#include <QUrlQuery>
#include <QScopeGuard>
#include <QTcpSocket>
#include <QHostAddress>
#include <QSettings>
#include <QUuid>
#include <cstring>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

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
	void header(const QByteArray &name, const QByteArray &value) { setRawHeader(name, value); }
	void complete() { setFinished(true); emit finished(); }
	void abort() override { setError(QNetworkReply::OperationCanceledError, "Canceled"); complete(); }
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

bool isAccountRequest(const QNetworkRequest &request)
{
	return request.url().path() == "/drive/v3/about" || request.url().path() == "/2/users/get_current_account";
}

QByteArray accountResponse(const QString &id = "test-account", const QString &email = "test@example.invalid")
{
	return QJsonDocument(QJsonObject{{"account_id", id}, {"email", email}, {"name", QJsonObject{{"display_name", "Test Diver"}}},
		{"user", QJsonObject{{"permissionId", id}, {"displayName", "Test Diver"}, {"emailAddress", email}}}}).toJson();
}

class FakeNetwork : public QNetworkAccessManager {
public:
	QByteArray body;
	int status = 200;
	int requests = 0; // File/token requests, counted separately from identity.
	int accountRequests = 0;
	QByteArray accountBody = accountResponse();
	int accountStatus = 200;
	bool accountDeferred = false;
	FakeReply *lastAccountReply = nullptr;
	int timeout = 0;
	QByteArray posted;
	bool deferred = false;
	FakeReply *lastReply = nullptr;
	QStringList routes;
protected:
	QNetworkReply *createRequest(Operation, const QNetworkRequest &request, QIODevice *outgoing) override
	{
		if (isAccountRequest(request)) {
			++accountRequests;
			lastAccountReply = new FakeReply(request, accountBody, accountStatus, this, accountDeferred);
			return lastAccountReply;
		}
		++requests;
		routes.append(request.url().path());
		posted = outgoing ? outgoing->readAll() : QByteArray();
		timeout = request.transferTimeout();
		lastReply = new FakeReply(request, body, status, this, deferred);
		lastReply->header("Dropbox-API-Result", R"({"rev":"test-revision"})");
		return lastReply;
	}
};

// Model provider-side atomic operations, with independent files/revisions and
// injected failures before or after the remote commit. No real network or keys.
class MemoryCloud : public QNetworkAccessManager {
public:
	bool workspaceMode = false, workspaceReadDeferred = false, workspacePayloadAckLost = false;
	int workspaceRequests = 0, accountRequests = 0;
	std::function<void(const QNetworkRequest &)> requested;
	struct File { QString name, id, version; QByteArray data; QString created; };
	QVector<File> files;
	int serial = 0;
	int failStage = 0; // 1: payload rejected, 2: commit rejected, 3: commit ACK lost
	bool changeDuringRead = false;
	bool omitVersion = false;
	int puts = 0;
	int renames = 0;
	int recoveryFault = 0; // 1: reject, 2: ACK lost, 3: revision race, 4: continuous race, 5: defer
	bool reverseListing = false;
	QByteArray lateInitialManifest;
	FakeReply *recoveryReply = nullptr;
	QString manifestName = "subsurface-neo-sync.json";
	QByteArray json(const QJsonObject &object) { return QJsonDocument(object).toJson(QJsonDocument::Compact); }
	void add(const QString &name, const QByteArray &data) {
		const QString id = QString::number(++serial);
		files.append({name, id, "rev-" + id, data, QDateTime::fromMSecsSinceEpoch(serial, Qt::UTC).toString(Qt::ISODateWithMs)});
	}
	int byName(const QString &name) const {
		for (int i = 0; i < files.size(); ++i) if (files[i].name == name) return i;
		return -1;
	}
	QByteArray content(const QString &name) const { const int i = byName(name); return i < 0 ? QByteArray() : files[i].data; }
protected:
	QNetworkReply *createRequest(Operation op, const QNetworkRequest &request, QIODevice *outgoing) override {
		if (requested) requested(request);
		if (isAccountRequest(request)) {
			++accountRequests;
			return new FakeReply(request, accountResponse(), 200, this);
		}
		++workspaceRequests;
		const QByteArray sent = outgoing ? outgoing->readAll() : QByteArray();
		const auto url = request.url();
		const bool google = url.host() == "www.googleapis.com";
		const QUrlQuery query(url);
		QByteArray body;
		QByteArray metadata;
		int status = 200;
		QString name;
		int index = -1;
		bool write = false, create = false, rename = false;
		QByteArray data = sent;
		QString expected;
		if (!google) {
			const auto arg = QJsonDocument::fromJson(request.rawHeader("Dropbox-API-Arg")).object();
			const auto target = arg.value("path").toString();
			if (workspaceMode && target.startsWith("id:")) {
				for (int i = 0; i < files.size(); ++i) if ("id:" + files[i].id == target) index = i;
				if (index >= 0) name = files[index].name;
			} else { name = target.mid(1); index = byName(name); }
			write = url.path().endsWith("/upload");
			create = arg.value("mode").toString() == "add";
			expected = arg.value("mode").toObject().value("update").toString();
		} else if (op == GetOperation && url.path() == "/drive/v3/files") {
			const QString filter = query.queryItemValue("q");
			name = filter.section('\'', 1, 1);
			QJsonArray matches;
			for (const auto &file : files)
				if (file.name == name) {
					const QJsonObject entry{{"id", file.id}, {"name", file.name}, {"createdTime", file.created}};
					if (reverseListing) matches.prepend(entry); else matches.append(entry);
				}
			body = json({{"files", matches}});
		} else if (op == PostOperation) {
			create = write = true;
			const int metaBegin = sent.indexOf("\r\n\r\n") + 4;
			const int metaEnd = sent.indexOf("\r\n--", metaBegin);
			name = QJsonDocument::fromJson(sent.mid(metaBegin, metaEnd - metaBegin)).object().value("name").toString();
			const int dataBegin = sent.indexOf("\r\n\r\n", metaEnd) + 4;
			data = sent.mid(dataBegin, sent.lastIndexOf("\r\n--") - dataBegin);
		} else {
			const QString id = url.path().section('/', -1);
			for (int i = 0; i < files.size(); ++i) if (files[i].id == id) index = i;
			if (index >= 0) name = files[index].name;
			rename = op == CustomOperation && request.attribute(QNetworkRequest::CustomVerbAttribute).toByteArray() == "PATCH";
			write = op == PutOperation;
			expected = QString::fromUtf8(request.rawHeader("If-Match"));
		}
		bool defer = false;
		if (rename) {
			++renames;
			const int fault = recoveryFault;
			if (fault != 4) recoveryFault = 0;
			if ((fault == 3 || fault == 4) && index >= 0) files[index].version = "rev-" + QString::number(++serial);
			if (fault == 1) status = 503;
			else if (index < 0 || expected != files[index].version) status = 412;
			else {
				files[index].name = QJsonDocument::fromJson(sent).object().value("title").toString();
				files[index].version = "rev-" + QString::number(++serial);
				body = json({{"id", files[index].id}});
				if (fault == 2) status = 503;
				if (fault == 5) defer = true;
				if (!lateInitialManifest.isEmpty()) {
					add(manifestName, lateInitialManifest);
					lateInitialManifest.clear();
				}
			}
		} else if (write) {
			++puts;
			const bool commit = name == manifestName;
			if ((failStage == 1 && !commit) || (failStage == 2 && commit)) {
				status = 503;
			} else if ((!create && (index < 0 || expected != files[index].version)) || (!google && create && index >= 0)) {
				status = google ? 412 : 409;
				body = json({{"error_summary", "path/conflict/file/"}});
			} else {
				if (create) { add(name, data); index = files.size() - 1; }
				else { files[index].data = data; files[index].version = "rev-" + QString::number(++serial); }
				body = json({{"id", workspaceMode && !google ? "id:" + files[index].id : files[index].id}, {"rev", files[index].version}});
				if (workspaceMode) { auto metadata = QJsonDocument::fromJson(body).object(); metadata["name"] = files[index].name; body = json(metadata); }
				if (workspacePayloadAckLost && create && name.startsWith("subsurface-neo-workspace-") && name.endsWith(".xml")) status = 503;
				if (failStage == 3 && commit) status = 503;
			}
		} else if (body.isEmpty()) {
			if (index < 0) {
				status = google ? 404 : 409;
				body = R"({"error":{".tag":"path","path":{".tag":"not_found"}}})";
			} else if (google && url.path().startsWith("/drive/v2/")) {
				body = json(omitVersion ? QJsonObject{{"id", files[index].id}}
					: QJsonObject{{"id", files[index].id}, {"etag", files[index].version}, {"title", files[index].name}});
			} else {
				body = files[index].data;
				if (!omitVersion) metadata = workspaceMode ? json({{"rev", files[index].version}, {"name", files[index].name}, {"id", "id:" + files[index].id}}) : json({{"rev", files[index].version}});
				if (changeDuringRead && google) files[index].version = "rev-" + QString::number(++serial);
			}
		}
		defer |= workspaceMode && workspaceReadDeferred && !write && !rename;
		auto *reply = new FakeReply(request, body, status, this, defer);
		if (defer) recoveryReply = reply;
		if (!metadata.isEmpty()) reply->header("Dropbox-API-Result", metadata);
		return reply;
	}
};

}

void TestCloudSyncManager::init()
{
	previousApplicationName = QCoreApplication::applicationName();
	previousOrganizationName = QCoreApplication::organizationName();
	previousSettingsSuffix = settings_suffix;
	settings_suffix = "neo-cloud-regression-" + QUuid::createUuid().toString(QUuid::WithoutBraces).toStdString();
	QCoreApplication::setOrganizationName("SubsurfaceNeoTests");
	QCoreApplication::setApplicationName(QString::fromStdString(settings_suffix));
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
	for (const QString &provider : {QStringLiteral("google-drive"), QStringLiteral("dropbox")}) {
		CloudCredentialStore::remove(provider);
		CloudCredentialStore::remove(CloudSyncManager::syncStateCredentialKey(provider));
	}
	QSettings().clear();
	settings_suffix = previousSettingsSuffix;
	QCoreApplication::setApplicationName(previousApplicationName);
	QCoreApplication::setOrganizationName(previousOrganizationName);
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

void TestCloudSyncManager::testRefreshFailure_data()
{
	QTest::addColumn<int>("status");
	QTest::addColumn<QByteArray>("body");
	QTest::addColumn<bool>("reconnect");
	QTest::newRow("revoked grant") << 400 << QByteArray(R"({"error":"invalid_grant"})") << true;
	QTest::newRow("unauthorized grant") << 401 << QByteArray(R"({"error":"invalid_grant"})") << true;
	QTest::newRow("client configuration") << 400 << QByteArray(R"({"error":"invalid_client"})") << false;
	QTest::newRow("server failure") << 503 << QByteArray(R"({"error":"invalid_grant"})") << false;
	QTest::newRow("network failure") << 0 << QByteArray() << false;
	QTest::newRow("malformed response") << 200 << QByteArray("broken") << false;
}

void TestCloudSyncManager::testRefreshFailure()
{
	QFETCH(int, status);
	QFETCH(QByteArray, body);
	QFETCH(bool, reconnect);
	FakeNetwork network;
	network.status = status;
	network.body = body;
	CloudSyncManager manager(&network);
	OAuth2TokenSet token;
	token.accessToken = "expired-token";
	token.refreshToken = "test-refresh";
	token.expiresAt = QDateTime::currentDateTimeUtc().addSecs(-60);
	manager.tokens.insert("dropbox", token);
	manager.setPrimaryProvider("dropbox");
	manager.setAutoSyncEnabled(true);
	CloudCredentialStore::save("dropbox", R"({"access_token":"expired-token","refresh_token":"test-refresh"})");
	manager.saveLastSyncManifest("dropbox", CloudSyncManifest::forPayload(manager.serializeCurrentDiveLog()));
	const QByteArray credentials = CloudCredentialStore::load("dropbox");
	const QByteArray baseline = CloudCredentialStore::load(manager.syncStateCredentialKey("dropbox"));
	const QByteArray original = manager.serializeCurrentDiveLog();
	QSignalSpy disconnected(&manager, &CloudSyncManager::providerDisconnected);
	QSignalSpy browser(&manager, &CloudSyncManager::authorizationUrlOpened);
	manager.syncDiveLog("dropbox");
	QVERIFY(manager.syncInProgress());
	QTRY_VERIFY(!manager.syncInProgress());
	QVERIFY(!manager.pendingTokenContinuation);
	QVERIFY(manager.refreshProviderId.isEmpty());
	QCOMPARE(manager.serializeCurrentDiveLog(), original);
	QCOMPARE(disconnected.count(), reconnect ? 1 : 0);
	QCOMPARE(manager.tokens.contains("dropbox"), !reconnect);
	QCOMPARE(manager.primaryProviderId(), reconnect ? QString() : QString("dropbox"));
	QCOMPARE(CloudCredentialStore::load("dropbox"), reconnect ? QByteArray() : credentials);
	QCOMPARE(CloudCredentialStore::load(manager.syncStateCredentialKey("dropbox")), reconnect ? QByteArray() : baseline);
	if (reconnect) {
		QVERIFY(manager.lastError().contains("connect it again"));
		QVERIFY(!manager.syncPrimaryIfReady());
		manager.syncDiveLog("dropbox");
		QVERIFY(!manager.syncInProgress());
		QCOMPARE(network.requests, 1);
	} else {
		manager.syncDiveLog("dropbox");
		QTRY_VERIFY(!manager.syncInProgress());
		QCOMPARE(network.requests, 2);
	}
	QCOMPARE(browser.count(), 0);
}

void TestCloudSyncManager::testAccessTokenRejected_data()
{
	QTest::addColumn<QString>("providerId");
	QTest::addColumn<bool>("refreshable");
	QTest::addColumn<bool>("upload");
	QTest::newRow("dropbox refreshable") << QString("dropbox") << true << false;
	QTest::newRow("dropbox upload refreshable") << QString("dropbox") << true << true;
	QTest::newRow("dropbox no refresh") << QString("dropbox") << false << false;
	QTest::newRow("google refreshable") << QString("google-drive") << true << false;
	QTest::newRow("google upload refreshable") << QString("google-drive") << true << true;
	QTest::newRow("google no refresh") << QString("google-drive") << false << false;
}

void TestCloudSyncManager::testAccessTokenRejected()
{
	QFETCH(QString, providerId);
	QFETCH(bool, refreshable);
	QFETCH(bool, upload);
	FakeNetwork network;
	network.status = 401;
	network.body = R"({"error_summary":"invalid_access_token"})";
	CloudSyncManager manager(&network);
	OAuth2TokenSet token;
	token.accessToken = "rejected-token";
	token.expiresAt = QDateTime::currentDateTimeUtc().addSecs(3600);
	if (refreshable)
		token.refreshToken = "test-refresh";
	manager.tokens.insert(providerId, token);
	manager.setPrimaryProvider(providerId);
	const QByteArray original = manager.serializeCurrentDiveLog();
	if (upload)
		manager.backupDiveLog(providerId);
	else
		manager.syncDiveLog(providerId);
	QTRY_VERIFY(!manager.syncInProgress());
	QVERIFY(!manager.tokens.value(providerId).hasAccessToken());
	QCOMPARE(manager.tokens.value(providerId).canRefresh(), refreshable);
	QCOMPARE(manager.primaryProviderId(), refreshable ? providerId : QString());
	QCOMPARE(manager.serializeCurrentDiveLog(), original);
	QCOMPARE(network.requests, 1);
	// Dropbox needs no external desktop client JSON. Complete a real manager
	// refresh + manifest sequence to prove retry uses the new access token.
	if (refreshable && providerId == "dropbox") {
		network.deferred = true;
		network.status = 200;
		network.body = R"({"access_token":"renewed-token","expires_in":3600})";
		QSignalSpy success(&manager, &CloudSyncManager::diveLogSyncFinished);
		manager.syncDiveLog(providerId);
		QCOMPARE(network.requests, 2);
		QUrlQuery form(QString::fromUtf8(network.posted));
		QCOMPARE(form.queryItemValue("grant_type"), QString("refresh_token"));
		network.body = CloudSyncManifest::forPayload(original).toJson();
		network.lastReply->complete();
		QTRY_COMPARE(network.requests, 3);
		QCOMPARE(network.lastReply->request().rawHeader("Authorization"), QByteArray("Bearer renewed-token"));
		network.lastReply->complete();
		QVERIFY(!manager.syncInProgress());
		QCOMPARE(success.count(), 1);
		QCOMPARE(success.at(0).at(1).toString(), QString("up-to-date"));
		QCOMPARE(manager.serializeCurrentDiveLog(), original);
	}
}

void TestCloudSyncManager::testDisconnectCancelsRefresh()
{
	FakeNetwork network;
	network.deferred = true;
	network.body = R"({"access_token":"late-token","expires_in":3600})";
	CloudSyncManager manager(&network);
	OAuth2TokenSet token;
	token.refreshToken = "test-refresh";
	manager.tokens.insert("dropbox", token);
	manager.setPrimaryProvider("dropbox");
	const QByteArray original = manager.serializeCurrentDiveLog();
	manager.syncDiveLog("dropbox");
	QVERIFY(manager.pendingTokenContinuation);
	FakeReply *lateReply = network.lastReply;
	manager.disconnectProvider("dropbox");
	QVERIFY(!manager.syncInProgress());
	QVERIFY(!manager.pendingTokenContinuation);
	QVERIFY(manager.refreshProviderId.isEmpty());
	QVERIFY(manager.primaryProviderId().isEmpty());
	// Even a reply that completes after abort cannot restore credentials or
	// continue to the manifest request.
	lateReply->complete();
	QVERIFY(!manager.tokens.contains("dropbox"));
	QVERIFY(CloudCredentialStore::load("dropbox").isEmpty());
	QCOMPARE(network.requests, 1);
	QCOMPARE(manager.serializeCurrentDiveLog(), original);
}

void TestCloudSyncManager::testDisconnectCancelsAuthorization()
{
	FakeNetwork network;
	network.deferred = true;
	network.body = R"({"access_token":"late-token","refresh_token":"late-refresh"})";
	CloudSyncManager manager(&network);
	manager.activeProviderId = "dropbox";
	manager.tokenClient.exchangeAuthorizationCode(cloudSyncProviderDescriptor(CloudSyncProviderType::Dropbox),
		"test-client", "code", "verifier", QUrl("http://127.0.0.1/callback"));
	FakeReply *lateReply = network.lastReply;
	manager.disconnectProvider("dropbox");
	QVERIFY(!manager.authorizationInProgress());
	// Starting a subsequent flow must not attribute the old reply to it.
	manager.activeProviderId = "google-drive";
	lateReply->complete();
	QVERIFY(manager.tokens.isEmpty());
	QCOMPARE(manager.activeProviderId, QString("google-drive"));
	QVERIFY(CloudCredentialStore::load("google-drive").isEmpty());
	manager.clearAuthorization();
}

void TestCloudSyncManager::testDisconnectCancelsFileRequests_data()
{
	QTest::addColumn<QString>("providerId");
	QTest::addColumn<bool>("upload");
	QTest::newRow("google lookup before download") << QString("google-drive") << false;
	QTest::newRow("google lookup before upload") << QString("google-drive") << true;
	QTest::newRow("dropbox download") << QString("dropbox") << false;
	QTest::newRow("dropbox upload") << QString("dropbox") << true;
}

void TestCloudSyncManager::testDisconnectCancelsFileRequests()
{
	QFETCH(QString, providerId);
	QFETCH(bool, upload);
	FakeNetwork network;
	network.deferred = true;
	network.body = R"({"files":[{"id":"test-file"}]})";
	CloudSyncManager manager(&network);
	OAuth2TokenSet token;
	token.accessToken = "test-token";
	manager.tokens.insert(providerId, token);
	QSignalSpy downloaded(&manager, &CloudSyncManager::downloadFinished);
	QSignalSpy uploaded(&manager, &CloudSyncManager::uploadFinished);
	if (upload)
		manager.backupDiveLog(providerId);
	else
		manager.syncDiveLog(providerId);
	QVERIFY(manager.syncInProgress());
	QTRY_COMPARE(network.requests, 1);
	FakeReply *lateReply = network.lastReply;
	manager.disconnectProvider(providerId);
	lateReply->complete();
	QVERIFY(!manager.syncInProgress());
	QCOMPARE(network.requests, 1);
	QCOMPARE(downloaded.count(), 0);
	QCOMPARE(uploaded.count(), 0);
	QVERIFY(manager.tokens.isEmpty());
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


void TestCloudSyncManager::testTransactionalUpload_data()
{
	QTest::addColumn<QString>("providerId");
	QTest::addColumn<int>("failure");
	QTest::addColumn<bool>("initial");
	for (const QString &provider : {QString("google-drive"), QString("dropbox")})
		for (bool initial : {false, true})
			for (int failure = 0; failure <= 4; ++failure)
				QTest::newRow(qPrintable(provider + QString("-%1-%2").arg(initial).arg(failure))) << provider << failure << initial;
}

void TestCloudSyncManager::testTransactionalUpload()
{
	QFETCH(QString, providerId);
	QFETCH(int, failure);
	QFETCH(bool, initial);
	MemoryCloud network;
	CloudSyncManager manager(&network);
	OAuth2TokenSet token;
	token.accessToken = "synthetic";
	manager.tokens.insert(providerId, token);
	const QByteArray original = manager.serializeCurrentDiveLog();
	const auto baseline = CloudSyncManifest::forPayload(original);
	if (!initial) {
		network.add("subsurface-neo.xml", original);
		network.add(network.manifestName, baseline.toJson());
		manager.saveLastSyncManifest(providerId, baseline);
	}
	const auto savedBaseline = CloudCredentialStore::load(manager.syncStateCredentialKey(providerId));
	::divelog.dives[0]->notes = "A local edit to publish.";
	const QByteArray edited = manager.serializeCurrentDiveLog();
	network.failStage = failure <= 3 ? failure : 0;
	QSignalSpy backedUp(&manager, &CloudSyncManager::diveLogBackupFinished);
	QSignalSpy synced(&manager, &CloudSyncManager::diveLogSyncFinished);
	if (failure == 4) {
		connect(&manager, &CloudSyncManager::uploadFinished, &manager, [&](const QString &, const QString &name) {
			if (name.endsWith(".xml")) ::divelog.dives[0]->notes = "Edited while the payload was uploading.";
		});
	}
	manager.backupDiveLog(providerId);
	QTRY_VERIFY(!manager.syncInProgress());
	QCOMPARE(backedUp.count(), failure == 0 ? 1 : 0);
	QCOMPARE(synced.count(), 0);
	QCOMPARE(network.content("subsurface-neo.xml"), initial ? QByteArray() : original);
	if (failure == 4) {
		QVERIFY(manager.lastError().contains("changed during synchronization"));
		QCOMPARE(QString::fromStdString(::divelog.dives[0]->notes), QString("Edited while the payload was uploading."));
	} else {
		QCOMPARE(manager.serializeCurrentDiveLog(), edited);
	}
	if (failure == 1 || failure == 2 || failure == 4) {
		QCOMPARE(network.content(network.manifestName), initial ? QByteArray() : baseline.toJson());
		QCOMPARE(CloudCredentialStore::load(manager.syncStateCredentialKey(providerId)), savedBaseline);
	} else {
		const auto committed = CloudSyncManifest::fromJson(network.content(network.manifestName));
		QVERIFY(committed.isValid());
		QCOMPARE(committed.schemaVersion, 2);
		QCOMPARE(network.content(committed.payloadFileName()), edited);
		QCOMPARE(CloudSyncManifest::sha256(network.content(committed.payloadFileName())), committed.payloadSha256);
	}
	if (failure > 0 && failure < 4) {
		QVERIFY(!manager.lastError().isEmpty());
		network.failStage = 0;
		const int writesBeforeRetry = network.puts;
		if (failure == 3) manager.syncDiveLog(providerId); // Discover the committed write after losing its ACK.
		else manager.backupDiveLog(providerId);
		QTRY_VERIFY(!manager.syncInProgress());
		QVERIFY2(manager.lastError().isEmpty(), qPrintable(manager.lastError()));
		QCOMPARE(backedUp.count() + synced.count(), 1);
		if (failure == 3) QCOMPARE(network.puts, writesBeforeRetry);
		const auto committed = CloudSyncManifest::fromJson(network.content(network.manifestName));
		QVERIFY(committed.isValid());
		QCOMPARE(network.content(committed.payloadFileName()), edited);
	}
}

void TestCloudSyncManager::testSimultaneousWriters_data()
{
	QTest::addColumn<QString>("providerId");
	QTest::addColumn<bool>("initial");
	for (const QString &provider : {QString("google-drive"), QString("dropbox")})
		for (bool initial : {false, true})
			QTest::newRow(qPrintable(provider + QString::number(initial))) << provider << initial;
}

void TestCloudSyncManager::testSimultaneousWriters()
{
	QFETCH(QString, providerId);
	QFETCH(bool, initial);
	MemoryCloud network;
	CloudSyncManager first(&network), second(&network);
	OAuth2TokenSet token;
	token.accessToken = "synthetic";
	first.tokens.insert(providerId, token);
	second.tokens.insert(providerId, token);
	const QByteArray original = first.serializeCurrentDiveLog();
	if (!initial) {
		network.add("subsurface-neo.xml", original);
		network.add(network.manifestName, CloudSyncManifest::forPayload(original).toJson());
	}
	QSignalSpy firstDone(&first, &CloudSyncManager::diveLogBackupFinished);
	QSignalSpy secondDone(&second, &CloudSyncManager::diveLogBackupFinished);
	QSignalSpy firstConflict(&first, &CloudSyncManager::diveLogSyncConflict);
	QSignalSpy secondConflict(&second, &CloudSyncManager::diveLogSyncConflict);
	first.backupDiveLog(providerId);
	second.backupDiveLog(providerId);
	QTRY_VERIFY(!first.syncInProgress() && !second.syncInProgress());
	QCOMPARE(first.serializeCurrentDiveLog(), original);
	QCOMPARE(network.content("subsurface-neo.xml"), initial ? QByteArray() : original);
	if (initial && providerId == "google-drive") {
		QCOMPARE(firstDone.count() + secondDone.count(), 2);
		QCOMPARE(firstConflict.count() + secondConflict.count(), 0);
		int active = 0, recovered = 0;
		for (const auto &file : network.files) {
			if (file.name == network.manifestName) ++active;
			if (file.name.startsWith("subsurface-neo-sync-recovered-")) ++recovered;
		}
		QCOMPARE(active, 1);
		QCOMPARE(recovered, 1);
	} else {
		QCOMPARE(firstDone.count() + secondDone.count(), 1);
		QCOMPARE(firstConflict.count() + secondConflict.count(), 1);
	}
	int immutablePayloads = 0;
	for (const auto &file : network.files) {
		if (file.name.startsWith("subsurface-neo-") && file.name.endsWith(".xml")) {
			++immutablePayloads;
			QCOMPARE(file.data, original);
		}
	}
	QCOMPARE(immutablePayloads, 2);
	const auto committed = CloudSyncManifest::fromJson(network.content(network.manifestName));
	QVERIFY(committed.isValid());
	QCOMPARE(network.content(committed.payloadFileName()), original);
}

void TestCloudSyncManager::testGoogleInitialRecovery_data()
{
	QTest::addColumn<int>("scenario");
	const char *names[] = {"different initial logs", "rename rejected", "rename ACK lost", "revision race",
		"bounded contention", "established competing branch", "invalid competing manifest", "missing creation time",
		"two recovery clients", "delayed first upload", "reversed listing", "equal creation timestamps",
		"partial recovery and restart", "cancel during recovery", "invalid canonical manifest"};
	for (int i = 0; i < 15; ++i) QTest::newRow(names[i]) << i;
}

void TestCloudSyncManager::testGoogleInitialRecovery()
{
	QFETCH(int, scenario);
	MemoryCloud network;
	const auto initial = [](const QByteArray &data, const QString &parent = {}) {
		auto manifest = CloudSyncManifest::forPayload(data, parent);
		manifest.schemaVersion = 2;
		manifest.payloadFile = "subsurface-neo-" + manifest.revisionId + ".xml";
		return manifest;
	};
	const auto first = initial("first log");
	const auto second = initial("second log", scenario == 5 ? first.payloadSha256 : QString());
	network.add(network.manifestName, scenario == 14 ? QByteArray("invalid") : first.toJson());
	const QString canonicalId = network.files[0].id;
	network.add(network.manifestName, scenario == 6 ? QByteArray("invalid") : second.toJson());
	const QString candidateId = network.files[1].id;
	const QString candidateVersion = network.files[1].version;
	if (scenario == 7) network.files[1].created.clear();
	if (scenario == 11) network.files[1].created = network.files[0].created;
	if (scenario == 12) network.add(network.manifestName, initial("third log").toJson());
	network.add(first.payloadFileName(), "first log");
	network.add(second.payloadFileName(), "second log");
	network.add("unrelated.json", "untouched");
	if (scenario >= 1 && scenario <= 4) network.recoveryFault = scenario;
	if (scenario == 12) network.recoveryFault = 2;
	if (scenario == 13) network.recoveryFault = 5;
	if (scenario == 9) network.lateInitialManifest = initial("late log").toJson();
	network.reverseListing = scenario == 10 || scenario == 11;
	CloudSyncFileStore store(&network), other(&network);
	QSignalSpy received(&store, &CloudSyncFileStore::versionedDownloadFinished);
	QSignalSpy errors(&store, &CloudSyncFileStore::operationError);
	QSignalSpy otherReceived(&other, &CloudSyncFileStore::versionedDownloadFinished);
	store.downloadVersioned(CloudSyncProviderType::GoogleDrive, "synthetic", network.manifestName);
	if (scenario == 8) other.downloadVersioned(CloudSyncProviderType::GoogleDrive, "synthetic", network.manifestName);
	if (scenario == 13) {
		QTRY_VERIFY(network.recoveryReply);
		store.cancel(CloudSyncProviderType::GoogleDrive);
		QTest::qWait(10);
		QCOMPARE(received.count() + errors.count(), 0);
		store.downloadVersioned(CloudSyncProviderType::GoogleDrive, "synthetic", network.manifestName);
	}
	if (scenario == 1 || scenario == 2 || scenario == 12) {
		QTRY_COMPARE(errors.count(), 1);
		QCOMPARE(received.count(), 0);
		store.downloadVersioned(CloudSyncProviderType::GoogleDrive, "synthetic", network.manifestName);
	}
	if (scenario == 4 || scenario == 5 || scenario == 6 || scenario == 7 || scenario == 14) {
		QTRY_COMPARE(errors.count(), 1);
		QCOMPARE(received.count(), 0);
		QCOMPARE(network.renames, scenario == 4 ? 8 : 0);
		QCOMPARE(network.files[0].name, network.manifestName);
		QCOMPARE(network.files[1].name, network.manifestName);
	} else {
		QTRY_COMPARE(received.count(), 1);
		if (scenario == 8) QTRY_COMPARE(otherReceived.count(), 1);
		QCOMPARE(received[0][2].toByteArray(), first.toJson());
		QCOMPARE(received[0][4].toString(), canonicalId);
		int active = 0;
		for (const auto &file : network.files) if (file.name == network.manifestName) ++active;
		QCOMPARE(active, 1);
		QCOMPARE(network.files[1].name, "subsurface-neo-sync-recovered-" + candidateId + ".json");
		// Renaming must fence a writer which read the candidate before recovery.
		QSignalSpy conflicts(&store, &CloudSyncFileStore::writeConflict);
		store.uploadConditional(CloudSyncProviderType::GoogleDrive, "synthetic", network.manifestName,
			"stale overwrite", candidateVersion, candidateId);
		QTRY_COMPARE(conflicts.count(), 1);
	}
	QCOMPARE(network.files[1].data, scenario == 6 ? QByteArray("invalid") : second.toJson());
	QCOMPARE(network.content(first.payloadFileName()), QByteArray("first log"));
	QCOMPARE(network.content(second.payloadFileName()), QByteArray("second log"));
	QCOMPARE(network.content("unrelated.json"), QByteArray("untouched"));
}

void TestCloudSyncManager::testGoogleDifferentInitialLog()
{
	MemoryCloud network;
	CloudSyncManager manager(&network);
	OAuth2TokenSet token;
	token.accessToken = "synthetic";
	manager.tokens.insert("google-drive", token);
	const QByteArray local = manager.serializeCurrentDiveLog();
	// Inject another device's first manifest after this device captured an
	// empty account, but before its first manifest can be committed.
	bool competingCreated = false;
	connect(&manager, &CloudSyncManager::uploadFinished, &manager, [&](const QString &, const QString &name) {
		if (competingCreated || !name.endsWith(".xml")) return;
		competingCreated = true;
		auto remote = CloudSyncManifest::forPayload("different log");
		remote.schemaVersion = 2;
		remote.payloadFile = "subsurface-neo-" + remote.revisionId + ".xml";
		network.add(remote.payloadFileName(), "different log");
		network.add(network.manifestName, remote.toJson());
	});
	QSignalSpy conflicts(&manager, &CloudSyncManager::diveLogSyncConflict);
	QSignalSpy done(&manager, &CloudSyncManager::diveLogBackupFinished);
	manager.backupDiveLog("google-drive");
	QTRY_VERIFY(!manager.syncInProgress());
	QCOMPARE(conflicts.count(), 1);
	QCOMPARE(done.count(), 0);
	QCOMPARE(manager.serializeCurrentDiveLog(), local);
	QVERIFY(manager.lastSyncManifest("google-drive").payloadSha256.isEmpty());
	// Retry reaches the ordinary initial-choice flow instead of a duplicate
	// filename error; no provider-console repair or local replacement occurs.
	QSignalSpy choice(&manager, &CloudSyncManager::diveLogInitialChoiceRequired);
	manager.syncDiveLog("google-drive");
	QTRY_VERIFY(!manager.syncInProgress());
	QCOMPARE(choice.count(), 1);
	QCOMPARE(manager.serializeCurrentDiveLog(), local);
}

void TestCloudSyncManager::testVersionedRead_data()
{
	QTest::addColumn<QString>("providerId");
	QTest::addColumn<int>("fault");
	for (const QString &provider : {QString("google-drive"), QString("dropbox")})
		for (int fault : {0, 1, 2}) {
			if (provider == "dropbox" && fault == 2) continue;
			QTest::newRow(qPrintable(provider + QString::number(fault))) << provider << fault;
		}
}

void TestCloudSyncManager::testVersionedRead()
{
	QFETCH(QString, providerId);
	QFETCH(int, fault);
	const auto type = providerId == "dropbox" ? CloudSyncProviderType::Dropbox : CloudSyncProviderType::GoogleDrive;
	MemoryCloud network;
	network.add(network.manifestName, "known manifest");
	network.omitVersion = fault == 1;
	network.changeDuringRead = fault == 2;
	CloudSyncFileStore store(&network);
	QSignalSpy received(&store, &CloudSyncFileStore::versionedDownloadFinished);
	QSignalSpy errors(&store, &CloudSyncFileStore::operationError);
	QSignalSpy conflicts(&store, &CloudSyncFileStore::writeConflict);
	store.downloadVersioned(type, "synthetic", network.manifestName);
	QTRY_COMPARE(received.count() + errors.count() + conflicts.count(), 1);
	QCOMPARE(received.count(), fault == 0 ? 1 : 0);
	QCOMPARE(errors.count(), fault == 1 ? 1 : 0);
	QCOMPARE(conflicts.count(), fault == 2 ? 1 : 0);
	QCOMPARE(network.puts, 0);
	if (!fault) {
		QCOMPARE(received[0][2].toByteArray(), QByteArray("known manifest"));
		QCOMPARE(received[0][3].toString(), network.files[0].version);
	}
}

void TestCloudSyncManager::testAccountIdentity_data()
{
	QTest::addColumn<QString>("providerId");
	QTest::addColumn<int>("mode");
	for (const auto &provider : {QString("google-drive"), QString("dropbox")})
		for (int mode = 0; mode < 7; ++mode)
			QTest::newRow(qPrintable(provider + QString::number(mode))) << provider << mode;
}

void TestCloudSyncManager::testAccountIdentity()
{
	QFETCH(QString, providerId);
	QFETCH(int, mode); // Legacy, rename, changed, new consent, unknown old consent, native consent, native renewal
	FakeNetwork network;
	CloudSyncManager manager(&network);
	OAuth2TokenSet token;
	token.accessToken = "test-token";
	token.refreshToken = "test-refresh";
	if (mode != 0 && mode != 4) token.accountId = mode >= 2 ? "previous-account" : "test-account";
	token.accountEmail = "previous@example.invalid";
	manager.tokens.insert(providerId, token);
	manager.setPrimaryProvider(providerId);
	manager.setAutoSyncEnabled(true);
	const QByteArray local = manager.serializeCurrentDiveLog();
	manager.saveLastSyncManifest(providerId, CloudSyncManifest::forPayload(local));
	const auto saved = CloudCredentialStore::load(manager.syncStateCredentialKey(providerId));
	QSettings().setValue("subsurface-neo/cloud/last-sync/" + providerId, "previous-sync");
	QSignalSpy changed(&manager, &CloudSyncManager::providerAccountChanged);
	QSignalSpy connected(&manager, &CloudSyncManager::providerConnected);
	if (mode == 5 || mode == 6) {
		if (mode == 5) manager.activeProviderId = providerId;
		else {
			manager.refreshProviderId = providerId;
			manager.pendingTokenContinuation = [](const QString &) { QFAIL("Changed native account must not continue sync"); };
		}
		manager.acceptNativeAccessToken(providerId, "native-token");
	} else if (mode == 3 || mode == 4) {
		manager.activeProviderId = providerId;
		OAuth2TokenSet next;
		next.accessToken = "new-consent-token";
		next.refreshToken = "new-consent-refresh";
		manager.tokenClient.tokenReceived(next);
	} else {
		manager.checkAccount(providerId);
	}
	QVERIFY(manager.accountCheckInProgress());
	QTRY_VERIFY(!manager.accountCheckInProgress());
	const auto current = manager.tokens.value(providerId);
	QCOMPARE(current.accountId, QString("test-account"));
	QCOMPARE(current.accountEmail, QString("test@example.invalid"));
	QCOMPARE(current.accountName, QString("Test Diver"));
	QCOMPARE(manager.verifiedAccountTokens.value(providerId), current.accessToken);
	QCOMPARE(network.accountRequests, 1);
	QCOMPARE(network.requests, 0); // Identity checks never read or write cloud files.
	QCOMPARE(manager.serializeCurrentDiveLog(), local);
	QCOMPARE(changed.count(), mode == 2 || mode == 3 || mode == 5 || mode == 6 ? 1 : 0);
	QCOMPARE(connected.count(), mode == 4 ? 1 : 0);
	const bool reset = mode >= 2;
	QCOMPARE(manager.primaryProviderId(), reset ? QString() : providerId);
	QCOMPARE(CloudCredentialStore::load(manager.syncStateCredentialKey(providerId)), reset ? QByteArray() : saved);
	QCOMPARE(QSettings().value("subsurface-neo/cloud/last-sync/" + providerId).toString(), reset ? QString() : QString("previous-sync"));
	if (reset) QVERIFY(!manager.syncPrimaryIfReady());
	for (const auto &item : manager.providers()) {
		const auto row = item.toMap();
		if (row.value("id").toString() != providerId) continue;
		QCOMPARE(row.value("accountEmail").toString(), current.accountEmail);
		QVERIFY(row.value("accountVerified").toBool());
	}
#if defined(Q_OS_WIN)
	CloudSyncManager restored(&network);
	QCOMPARE(restored.tokens.value(providerId).accountId, current.accountId);
	QCOMPARE(restored.tokens.value(providerId).accountEmail, current.accountEmail);
	QVERIFY(restored.verifiedAccountTokens.isEmpty()); // Restart checks the credential again.
#endif
	manager.disconnectProvider(providerId);
	QVERIFY(!manager.tokens.contains(providerId));
	QVERIFY(!manager.verifiedAccountTokens.contains(providerId));
	QVERIFY(CloudCredentialStore::load(providerId).isEmpty());
}

void TestCloudSyncManager::testAccountLookupFailure_data()
{
	QTest::addColumn<QString>("providerId");
	QTest::addColumn<int>("failure");
	for (const auto &provider : {QString("google-drive"), QString("dropbox")})
		for (int failure = 0; failure < 7; ++failure)
			QTest::newRow(qPrintable(provider + QString::number(failure))) << provider << failure;
}

void TestCloudSyncManager::testAccountLookupFailure()
{
	QFETCH(QString, providerId);
	QFETCH(int, failure);
	FakeNetwork network;
	network.accountStatus = failure == 0 ? 503 : failure == 1 || failure == 2 ? 401 : failure == 5 ? 302 : 200;
	if (failure == 3) network.accountBody = "malformed";
	if (failure == 4) network.accountBody = R"({"email":"no-id@example.invalid"})";
	if (failure == 6) network.accountBody = QByteArray(65537, ' ');
	CloudSyncManager manager(&network);
	OAuth2TokenSet token;
	token.accessToken = "test-token";
	token.accountId = "test-account";
	if (failure != 2) token.refreshToken = "test-refresh";
	manager.tokens.insert(providerId, token);
	manager.setPrimaryProvider(providerId);
	const QByteArray local = manager.serializeCurrentDiveLog();
	manager.saveLastSyncManifest(providerId, CloudSyncManifest::forPayload(local));
	const auto baseline = CloudCredentialStore::load(manager.syncStateCredentialKey(providerId));
	manager.backupDiveLog(providerId); // Even explicit backup stops before touching files.
	QVERIFY(manager.syncInProgress());
	QTRY_VERIFY(!manager.syncInProgress());
	QVERIFY(!manager.accountCheckInProgress());
	QVERIFY(!manager.lastError().isEmpty());
	QCOMPARE(network.requests, 0);
	QCOMPARE(network.accountRequests, 1);
	QCOMPARE(manager.serializeCurrentDiveLog(), local);
	QVERIFY(manager.verifiedAccountTokens.isEmpty());
	QCOMPARE(manager.primaryProviderId(), failure == 2 ? QString() : providerId);
	QCOMPARE(CloudCredentialStore::load(manager.syncStateCredentialKey(providerId)), failure == 2 ? QByteArray() : baseline);
	if (failure == 1 || failure == 2) {
		QVERIFY(!manager.tokens.value(providerId).hasAccessToken());
		QCOMPARE(manager.tokens.value(providerId).canRefresh(), failure == 1);
	} else {
		QCOMPARE(manager.tokens.value(providerId).accessToken, token.accessToken);
		network.accountStatus = 200;
		network.accountBody = accountResponse();
		manager.checkAccount(providerId);
		QTRY_VERIFY(!manager.accountCheckInProgress());
		QVERIFY(manager.lastError().isEmpty());
		QCOMPARE(manager.verifiedAccountTokens.value(providerId), token.accessToken);
	}
}

void TestCloudSyncManager::testAccountLookupCancellation()
{
	FakeNetwork network;
	network.accountDeferred = true;
	CloudSyncManager manager(&network);
	OAuth2TokenSet token;
	token.accessToken = "test-token";
	manager.tokens.insert("dropbox", token);
	manager.syncDiveLog("dropbox");
	QVERIFY(manager.accountCheckInProgress());
	QVERIFY(manager.syncInProgress());
	FakeReply *late = network.lastAccountReply;
	QCOMPARE(late->request().transferTimeout(), 30000);
	QCOMPARE(late->request().attribute(QNetworkRequest::RedirectPolicyAttribute).toInt(), int(QNetworkRequest::ManualRedirectPolicy));
	manager.beginAuthorization("dropbox");
	QVERIFY(!manager.authorizationInProgress());
	manager.disconnectProvider("dropbox");
	late->complete();
	QVERIFY(!manager.accountCheckInProgress());
	QVERIFY(!manager.syncInProgress());
	QCOMPARE(network.requests, 0);
	QVERIFY(manager.tokens.isEmpty());
	QVERIFY(CloudCredentialStore::load("dropbox").isEmpty());
}

void TestCloudSyncManager::testAccountChangeDuringRefresh()
{
	FakeNetwork network;
	network.body = R"({"access_token":"renewed-token","expires_in":3600})";
	CloudSyncManager manager(&network);
	OAuth2TokenSet token;
	token.refreshToken = "test-refresh";
	token.accountId = "previous-account";
	manager.tokens.insert("dropbox", token);
	manager.setBackupProvider("dropbox");
	const QByteArray local = manager.serializeCurrentDiveLog();
	manager.syncDiveLog("dropbox");
	QTRY_VERIFY(!manager.syncInProgress());
	QCOMPARE(network.requests, 1); // Refresh only; no file requests after the mismatch.
	QCOMPARE(network.accountRequests, 1);
	QVERIFY(manager.backupProviderId().isEmpty());
	QVERIFY(manager.lastError().contains("account changed"));
	QCOMPARE(manager.serializeCurrentDiveLog(), local);
	QCOMPARE(manager.tokens.value("dropbox").accountId, QString("test-account"));
	// An explicit retry can now compare the new account, with no old baseline.
	network.body = CloudSyncManifest::forPayload("different cloud log").toJson();
	QSignalSpy choice(&manager, &CloudSyncManager::diveLogInitialChoiceRequired);
	manager.syncDiveLog("dropbox");
	QTRY_COMPARE(choice.count(), 1);
	QCOMPARE(network.accountRequests, 1); // Same verified access token reused.
	QCOMPARE(manager.serializeCurrentDiveLog(), local);
}

void TestCloudSyncManager::testStorageReview_data()
{
	QTest::addColumn<QString>("providerId"); QTest::addColumn<int>("mode");
	for (const QString &provider : {QString("google-drive"), QString("dropbox")})
		for (int mode = 0; mode < 8; ++mode)
			QTest::newRow(qPrintable(provider + QString::number(mode))) << provider << mode;
}

void TestCloudSyncManager::testStorageReview()
{
	QFETCH(QString, providerId); QFETCH(int, mode);
	FakeNetwork network;
	network.body = providerId == "google-drive" ? QByteArray(R"({"files":[],"incompleteSearch":false})")
		: QByteArray(R"({"entries":[],"has_more":false,"cursor":"complete"})");
	CloudSyncManager manager(&network);
	OAuth2TokenSet token;
	token.accessToken = "synthetic"; token.refreshToken = "synthetic-refresh";
	token.accountId = mode == 1 ? "previous-account" : "test-account";
	manager.tokens.insert(providerId, token);
	manager.verifiedAccountTokens.insert(providerId, token.accessToken); // Review must verify afresh.
	manager.setPrimaryProvider(providerId); manager.setAutoSyncEnabled(true);
	const auto local = manager.serializeCurrentDiveLog();
	manager.saveLastSyncManifest(providerId, CloudSyncManifest::forPayload(local));
	const auto baseline = CloudCredentialStore::load(manager.syncStateCredentialKey(providerId));
	manager.storageReviews.insert(providerId, {{"reviewedAt", "old review"}});
	if (mode == 2) network.accountStatus = 503;
	if (mode == 3) network.accountDeferred = true;
	if (mode == 4 || mode == 5) network.deferred = true;
	if (mode == 6) network.status = 503;
	if (mode == 7) network.status = 401;
	QSignalSpy completed(&manager, &CloudSyncManager::storageReviewFinished);
	manager.reviewStorage(providerId);
	QVERIFY(manager.storageReviewInProgress());
	QVERIFY(manager.storageReviews.value(providerId).isEmpty());
	QVERIFY(!manager.syncPrimaryIfReady());
	QCOMPARE(network.accountRequests, 1);
	if (mode == 3) {
		auto *late = network.lastAccountReply;
		manager.cancelStorageReview(); late->complete();
	} else if (mode == 4 || mode == 5) {
		QTRY_COMPARE(network.requests, 1);
		auto *late = network.lastReply;
		if (mode == 4) manager.disconnectProvider(providerId); else manager.cancelStorageReview();
		late->complete();
	}
	QTRY_VERIFY(!manager.syncInProgress());
	QVERIFY(!manager.storageReviewInProgress());
	QVERIFY(!manager.accountCheckInProgress());
	QCOMPARE(completed.count(), mode == 0 ? 1 : 0);
	QCOMPARE(manager.serializeCurrentDiveLog(), local);
	if (mode != 1 && mode != 4) QCOMPARE(CloudCredentialStore::load(manager.syncStateCredentialKey(providerId)), baseline);
	if (mode >= 1 && mode <= 3) QCOMPARE(network.requests, 0);
	for (const auto &route : network.routes)
		QVERIFY(route == "/drive/v3/files" || route == "/2/files/list_folder");
	if (mode == 0) {
		QCOMPARE(manager.storageReviews.value(providerId).value("entryCount").toInt(), 0);
		QVERIFY(!manager.storageReviews.value(providerId).value("reviewedAt").toString().isEmpty());
		manager.disconnectProvider(providerId);
		QVERIFY(manager.storageReviews.value(providerId).isEmpty());
	} else {
		QVERIFY(manager.storageReviews.value(providerId).isEmpty());
	}
	if (mode == 6) {
		network.status = 200;
		manager.reviewStorage(providerId);
		QTRY_COMPARE(completed.count(), 1);
		QVERIFY(manager.lastError().isEmpty());
	} else if (mode == 7) {
		QVERIFY(manager.tokens.value(providerId).accessToken.isEmpty());
		QVERIFY(manager.tokens.value(providerId).canRefresh());
	}
}

void TestCloudSyncManager::testHistoryGuard_data()
{
	QTest::addColumn<int>("mode");
	for (int i = 0; i < 7; ++i) QTest::newRow(qPrintable(QString::number(i))) << i;
}

void TestCloudSyncManager::testHistoryGuard()
{
	QFETCH(int, mode);
	const auto previous = qgetenv("SUBSURFACE_NEO_HISTORY_DEV_EPOCH");
	const auto restore = qScopeGuard([&] { if (previous.isNull()) qunsetenv("SUBSURFACE_NEO_HISTORY_DEV_EPOCH"); else qputenv("SUBSURFACE_NEO_HISTORY_DEV_EPOCH", previous); });
	qputenv("SUBSURFACE_NEO_HISTORY_DEV_EPOCH", mode == 0 ? QByteArray() : QUuid::createUuid().toString(QUuid::WithoutBraces).toUtf8());
	FakeNetwork network; network.accountDeferred = true;
	if (mode == 5) network.accountBody = accountResponse("changed-account");
	if (mode == 6) network.accountStatus = 503;
	CloudSyncManager manager(&network);
	OAuth2TokenSet token; token.accessToken = "synthetic"; token.accountId = "test-account";
	manager.tokens.insert("dropbox", token);
	manager.verifiedAccountTokens.insert("dropbox", token.accessToken);
	if (mode == 1) manager.setPrimaryProvider("dropbox");
	if (mode == 2) manager.setAutoSyncEnabled(true);
	const auto local = manager.serializeCurrentDiveLog();
	manager.saveLastSyncManifest("dropbox", CloudSyncManifest::forPayload(local));
	const auto baseline = CloudCredentialStore::load(manager.syncStateCredentialKey("dropbox"));
	manager.runHistory("dropbox", "migrate");
	if (mode <= 2) { QCOMPARE(network.accountRequests, 0); QVERIFY(!manager.syncInProgress()); }
	else {
		QVERIFY(manager.historyInProgress()); QCOMPARE(network.accountRequests, 1);
		QVERIFY(!manager.syncPrimaryIfReady());
		manager.uploadBytes("dropbox", "unexpected.xml", "bad");
		manager.downloadBytes("dropbox", "unexpected.xml");
		QCOMPARE(network.requests, 0);
		auto *late = network.lastAccountReply;
		if (mode == 3) manager.cancelHistory();
		if (mode == 4) manager.disconnectProvider("dropbox");
		late->complete();
		QTRY_VERIFY(!manager.syncInProgress());
		QVERIFY(!manager.accountCheckInProgress()); QCOMPARE(network.requests, 0);
	}
	QCOMPARE(manager.serializeCurrentDiveLog(), local);
	if (mode != 4 && mode != 5) QCOMPARE(CloudCredentialStore::load(manager.syncStateCredentialKey("dropbox")), baseline);
}

void TestCloudSyncManager::testWorkspaceGuard_data()
{
	QTest::addColumn<bool>("google"); QTest::addColumn<int>("mode");
	for (bool g : {false, true}) for (int m = 0; m < 8; ++m) QTest::newRow(qPrintable(QString("%1-workspace-guard-%2").arg(g).arg(m))) << g << m;
}
void TestCloudSyncManager::testWorkspaceGuard()
{
	QFETCH(bool, google); QFETCH(int, mode);
	const auto previous = qgetenv("SUBSURFACE_NEO_HISTORY_DEV_EPOCH");
	const auto restore = qScopeGuard([&] { if (previous.isNull()) qunsetenv("SUBSURFACE_NEO_HISTORY_DEV_EPOCH"); else qputenv("SUBSURFACE_NEO_HISTORY_DEV_EPOCH", previous); });
	qputenv("SUBSURFACE_NEO_HISTORY_DEV_EPOCH", mode == 0 ? QByteArray() : QUuid::createUuid().toString(QUuid::WithoutBraces).toUtf8());
	const auto provider = QString(google ? "google-drive" : "dropbox");
	FakeNetwork network; network.accountDeferred = true;
	if (mode == 5) network.accountBody = accountResponse("changed-account"); if (mode == 6) network.accountStatus = 503;
	QTemporaryDir temporary; QVERIFY(temporary.isValid()); CloudSyncManager manager(&network); manager.workspaceDataRootOverride = temporary.path();
	OAuth2TokenSet token; token.accessToken = "synthetic"; token.accountId = "test-account"; manager.tokens.insert(provider, token); manager.verifiedAccountTokens.insert(provider, token.accessToken);
	if (mode == 1) manager.setPrimaryProvider(provider); if (mode == 2) manager.setAutoSyncEnabled(true);
	const auto local = manager.serializeCurrentDiveLog(); manager.saveLastSyncManifest(provider, CloudSyncManifest::forPayload(local));
	const auto baseline = CloudCredentialStore::load(manager.syncStateCredentialKey(provider));
	const auto ref = CloudWorkspaceReference::create(mode == 7 ? (google ? "dropbox" : "google-drive") : provider,
		CloudSyncManifest::sha256("test-account"), QUuid::createUuid().toString(QUuid::WithoutBraces), mode == 7 ? (google ? "id:root" : "root") : (google ? "root" : "id:root"));
	manager.runWorkspace(provider, "join", QString::fromUtf8(ref.json()));
	if (mode <= 2 || mode == 7) { QCOMPARE(network.accountRequests, 0); QVERIFY(!manager.syncInProgress()); }
	else {
		QVERIFY(manager.workspaceInProgress()); QCOMPARE(network.accountRequests, 1); QCOMPARE(network.requests, 0);
		manager.uploadBytes(provider, "unexpected.xml", "bad"); manager.downloadBytes(provider, "unexpected.xml"); QCOMPARE(network.requests, 0);
		auto *late = network.lastAccountReply; if (mode == 3) manager.cancelWorkspace(); if (mode == 4) manager.disconnectProvider(provider);
		late->complete(); QTRY_VERIFY(!manager.syncInProgress()); QVERIFY(!manager.accountCheckInProgress()); QCOMPARE(network.requests, 0);
	}
	QCOMPARE(manager.serializeCurrentDiveLog(), local);
	if (mode != 4 && mode != 5) QCOMPARE(CloudCredentialStore::load(manager.syncStateCredentialKey(provider)), baseline);
}
void TestCloudSyncManager::testWorkspaceSelection_data()
{
	QTest::addColumn<bool>("google"); QTest::addColumn<int>("mode");
	const QStringList cases = {"create-review-restart", "join-idempotence", "stale-during-identity", "stale-during-root", "missing-known-checkpoint",
		"checkpoint-disappears-during-read", "cancel", "disconnect", "local-edits", "assignments-change", "remote-rollback", "oversized-reference", "corrupt-choice", "bound-setup-missing-checkpoint", "resume-known-setup"};
	for (bool g : {false, true}) for (int m = 0; m < cases.size(); ++m) QTest::newRow(qPrintable(QString("%1-%2").arg(g).arg(cases[m]))) << g << m;
}
void TestCloudSyncManager::testWorkspaceSelection()
{
	QFETCH(bool, google); QFETCH(int, mode);
	const auto previous = qgetenv("SUBSURFACE_NEO_HISTORY_DEV_EPOCH");
	const auto restore = qScopeGuard([&] { if (previous.isNull()) qunsetenv("SUBSURFACE_NEO_HISTORY_DEV_EPOCH"); else qputenv("SUBSURFACE_NEO_HISTORY_DEV_EPOCH", previous); });
	qputenv("SUBSURFACE_NEO_HISTORY_DEV_EPOCH", QUuid::createUuid().toString(QUuid::WithoutBraces).toUtf8());
	QTemporaryDir temporary; QVERIFY(temporary.isValid()); MemoryCloud network; network.workspaceMode = true;
	CloudSyncManager manager(&network); manager.workspaceDataRootOverride = temporary.path(); const auto provider = QString(google ? "google-drive" : "dropbox"), account = CloudSyncManifest::sha256("test-account");
	OAuth2TokenSet token; token.accessToken = "synthetic"; token.accountId = "test-account"; token.expiresAt = QDateTime::currentDateTimeUtc().addSecs(3600); manager.tokens.insert(provider, token);
	const auto directory = manager.workspaceDirectory(provider, account), choiceFile = QDir(directory).filePath("selection.json");
	const auto local = manager.serializeCurrentDiveLog(); manager.saveLastSyncManifest(provider, CloudSyncManifest::forPayload(local)); const auto baseline = CloudCredentialStore::load(manager.syncStateCredentialKey(provider));
	const auto epoch = QUuid::createUuid().toString(QUuid::WithoutBraces), nonce = QUuid::createUuid().toString(QUuid::WithoutBraces), origin = CloudSyncManifest::sha256("origin");
	const auto ref = CloudWorkspaceReference::create(provider, account, epoch, google ? "1" : "id:1");
	const auto initial = CloudWorkspaceRecord::create(ref, origin);
	if (mode != 0) network.add(ref.rootName(), initial.json());
	const auto checkpointFile = QDir(directory).filePath("roots/" + ref.key() + ".json");
	const auto readFile = [](const QString &path) { QFile f(path); return f.open(QIODevice::ReadOnly) ? f.readAll() : QByteArray(); };
	if (mode == 12) { QVERIFY(QDir().mkpath(directory)); QFile bad(choiceFile); QVERIFY(bad.open(QIODevice::WriteOnly)); QCOMPARE(bad.write("invalid choice"), qint64(14)); bad.close(); }
	if (mode == 13 || mode == 14) {
		CloudWorkspaceCreationIntent intent(QDir(directory).filePath("creations/" + nonce + ".json"), provider, account, epoch, nonce, origin);
		QVERIFY(intent.prepare()); QVERIFY(intent.submit()); QVERIFY(intent.identify(ref.value["root_id"].toString()));
		if (mode == 13) { QVERIFY(intent.beginBinding()); QVERIFY(intent.bind()); } else network.files[0].data = intent.bootstrap();
	}
	CloudWorkspaceReference otherRef;
	const auto selectOther = [&] {
		otherRef = CloudWorkspaceReference::create(provider, account, QUuid::createUuid().toString(QUuid::WithoutBraces), google ? "other-root" : "id:other-root");
		CloudWorkspaceAuthority otherCheckpoint(QDir(directory).filePath("roots/" + otherRef.key() + ".json"), otherRef);
		QVERIFY(otherCheckpoint.observe(CloudWorkspaceRecord::create(otherRef, origin), "other-revision"));
		CloudWorkspaceSelection other(choiceFile, provider, account); QVERIFY(other.load()); QVERIFY(other.select(other.version(), otherRef));
	};
	bool injected = false;
	if (mode == 2) network.requested = [&](const QNetworkRequest &r) { if (isAccountRequest(r) && !injected) { injected = true; selectOther(); } };
	if (mode == 3 || mode == 6 || mode == 7 || mode == 8 || mode == 9) network.workspaceReadDeferred = true;
	QSignalSpy completed(&manager, &CloudSyncManager::workspaceFinished);
	const auto join = [&] { manager.runWorkspace(provider, "join", QString::fromUtf8(ref.json())); };
	if (mode == 0) manager.runWorkspace(provider, "create");
	else if (mode == 11) manager.runWorkspace(provider, "join", QString::fromUtf8(ref.json()) + QString(2049, ' '));
	else if (mode == 14) manager.runWorkspace(provider, "resume", nonce);
	else join();
	QByteArray expectedLocal = local;
	if (mode == 3 || mode == 6 || mode == 7 || mode == 8 || mode == 9) {
		QTRY_VERIFY(network.recoveryReply != nullptr); auto *late = network.recoveryReply;
		if (mode == 3) selectOther();
		if (mode == 6) manager.cancelWorkspace();
		if (mode == 7) manager.disconnectProvider(provider);
		if (mode == 8) { ::divelog.dives[0]->notes = "Edited during workspace verification."; expectedLocal = manager.serializeCurrentDiveLog(); }
		if (mode == 9) manager.setPrimaryProvider(provider);
		network.workspaceReadDeferred = false; late->complete();
	}
	QTRY_VERIFY(!manager.syncInProgress()); QVERIFY(!manager.accountCheckInProgress());
	const bool success = mode == 0 || mode == 1 || mode == 4 || mode == 5 || mode == 8 || mode == 10 || mode == 14;
	QCOMPARE(completed.size(), int(success));
	if (success) {
		QVERIFY(manager.lastError().isEmpty()); CloudWorkspaceSelection choice(choiceFile, provider, account); QVERIFY(choice.load()); QVERIFY(choice.selected().valid());
		QVERIFY(manager.workspaceReport(provider).value("verified").toBool()); const auto selected = choice.selected();
		const auto actualCheckpoint = QDir(directory).filePath("roots/" + selected.key() + ".json"); const auto choiceBytes = readFile(choiceFile); const auto puts = network.puts;
		if (mode == 4 || mode == 5 || mode == 10) {
			if (mode == 4) QVERIFY(QFile::remove(actualCheckpoint));
			if (mode == 10) { auto advanced = initial; QVERIFY(advanced.registerUpload(QUuid::createUuid().toString(QUuid::WithoutBraces), {}, CloudSyncManifest::sha256(local), local.size())); CloudWorkspaceAuthority checkpoint(actualCheckpoint, ref); QVERIFY(checkpoint.observe(advanced, "advanced-version")); }
			if (mode == 5) network.workspaceReadDeferred = true;
			const auto requests = network.workspaceRequests; manager.runWorkspace(provider, "review");
			if (mode == 5) { QTRY_VERIFY(network.recoveryReply); QVERIFY(QFile::remove(actualCheckpoint)); network.workspaceReadDeferred = false; network.recoveryReply->complete(); }
			QTRY_VERIFY(!manager.syncInProgress()); QVERIFY(!manager.lastError().isEmpty()); QCOMPARE(completed.size(), 1); QCOMPARE(readFile(choiceFile), choiceBytes);
			if (mode == 4) QCOMPARE(network.workspaceRequests, requests);
			if (mode == 4 || mode == 5) QVERIFY(!QFileInfo::exists(actualCheckpoint));
		} else {
			manager.runWorkspace(provider, "review"); QTRY_COMPARE(completed.size(), 2); QCOMPARE(network.puts, puts); QCOMPARE(readFile(choiceFile), choiceBytes);
			if (mode == 1) { join(); QTRY_COMPARE(completed.size(), 3); QCOMPARE(readFile(choiceFile), choiceBytes); QCOMPARE(network.puts, puts); }
			if (mode == 0) {
				CloudSyncManager restarted(&network); restarted.workspaceDataRootOverride = temporary.path();
#if defined(Q_OS_WIN) || defined(Q_OS_ANDROID)
				QCOMPARE(restarted.tokens.value(provider).accountId, token.accountId);
#else
				// These platforms deliberately have no credential persistence backend.
				// Restore a synthetic in-memory session to inspect the durable choice.
				QVERIFY(!restarted.tokens.contains(provider));
				restarted.tokens.insert(provider, manager.tokens.value(provider));
#endif
				QVERIFY(restarted.workspaceReport(provider).value("selected").toBool());
				QVERIFY(!restarted.workspaceReport(provider).value("verified").toBool());
			}
		}
		QCOMPARE(network.puts, puts);
	} else if (mode == 2 || mode == 3) {
		CloudWorkspaceSelection choice(choiceFile, provider, account); QVERIFY(choice.load()); QCOMPARE(choice.selected().key(), otherRef.key());
		if (mode == 2) QCOMPARE(network.workspaceRequests, 0); else QVERIFY(QFileInfo::exists(checkpointFile));
	} else if (mode == 11 || mode == 12 || mode == 13) { QCOMPARE(network.workspaceRequests, 0); }
	QCOMPARE(manager.serializeCurrentDiveLog(), expectedLocal);
	if (mode != 7) QCOMPARE(CloudCredentialStore::load(manager.syncStateCredentialKey(provider)), baseline);
	if (mode != 0 && mode != 14) QCOMPARE(network.files[0].data, initial.json());
}

void TestCloudSyncManager::testWorkspaceCopy_data()
{
	QTest::addColumn<bool>("google"); QTest::addColumn<int>("mode");
	const QStringList cases = {"copy-idempotent-restart", "unreviewed", "wrong-review", "edited-before-confirmation", "choice-away-back", "missing-checkpoint",
		"changed-source", "replaced-source", "cancel-copy", "disconnect-copy", "edit-during-copy", "choice-during-copy", "assignment-during-copy",
		"unknown-payload-ack", "resume-registered", "resume-identified", "submitted-no-replay", "corrupt-proof", "checkpoint-lost-during-copy", "restart-forgets-confirmation",
		"cancel-review", "edit-during-review", "assigned-before-confirmation", "stale-parent"};
	for (bool google : {false, true}) for (int mode = 0; mode < cases.size(); ++mode) QTest::newRow(qPrintable(QString("%1-%2").arg(google).arg(cases[mode]))) << google << mode;
}
void TestCloudSyncManager::testWorkspaceCopy()
{
	QFETCH(bool, google); QFETCH(int, mode);
	const auto previous = qgetenv("SUBSURFACE_NEO_HISTORY_DEV_EPOCH");
	const auto restore = qScopeGuard([&] { if (previous.isNull()) qunsetenv("SUBSURFACE_NEO_HISTORY_DEV_EPOCH"); else qputenv("SUBSURFACE_NEO_HISTORY_DEV_EPOCH", previous); });
	qputenv("SUBSURFACE_NEO_HISTORY_DEV_EPOCH", QUuid::createUuid().toString(QUuid::WithoutBraces).toUtf8());
	QTemporaryDir temporary; QVERIFY(temporary.isValid()); MemoryCloud network; network.workspaceMode = true;
	CloudSyncManager manager(&network); manager.workspaceDataRootOverride = temporary.path();
	const auto provider = QString(google ? "google-drive" : "dropbox"), account = CloudSyncManifest::sha256("test-account");
	OAuth2TokenSet token; token.accessToken = "synthetic"; token.accountId = "test-account"; token.expiresAt = QDateTime::currentDateTimeUtc().addSecs(3600); manager.tokens.insert(provider, token);
	const auto local = manager.serializeCurrentDiveLog(); auto expectedLocal = local;
	manager.saveLastSyncManifest(provider, CloudSyncManifest::forPayload(local)); const auto baseline = CloudCredentialStore::load(manager.syncStateCredentialKey(provider));
	const auto directory = manager.workspaceDirectory(provider, account), choiceFile = QDir(directory).filePath("selection.json");
	const auto epoch = QUuid::createUuid().toString(QUuid::WithoutBraces);
	const auto ref = CloudWorkspaceReference::create(provider, account, epoch, google ? "1" : "id:1");
	auto root = CloudWorkspaceRecord::create(ref, CloudSyncManifest::sha256("genesis")); network.add(ref.rootName(), root.json());
	auto manifest = CloudSyncManifest::forPayload(local); manifest.schemaVersion = 2; manifest.payloadFile = "subsurface-neo-" + manifest.revisionId + ".xml";
	network.add("subsurface-neo-sync.json", manifest.toJson()); network.add(manifest.payloadFileName(), local);
	const auto checkpointFile = QDir(directory).filePath("roots/" + ref.key() + ".json"); CloudWorkspaceAuthority checkpoint(checkpointFile, ref); QVERIFY(checkpoint.observe(root, network.files[0].version));
	CloudWorkspaceSelection choice(choiceFile, provider, account); QVERIFY(choice.load()); QVERIFY(choice.select(choice.version(), ref));
	QFile choiceRead(choiceFile); QVERIFY(choiceRead.open(QIODevice::ReadOnly)); const auto choiceBytes = choiceRead.readAll(); choiceRead.close();
	const auto edit = [&] { ::divelog.dives[0]->notes = "Edits must survive workspace copy."; expectedLocal = manager.serializeCurrentDiveLog(); };
	const auto changeChoice = [&] {
		const auto other = CloudWorkspaceReference::create(provider, account, QUuid::createUuid().toString(QUuid::WithoutBraces), google ? "another-root" : "id:another-root");
		CloudWorkspaceSelection changed(choiceFile, provider, account); QVERIFY(changed.load()); QVERIFY(changed.select(changed.version(), other));
		if (mode == 4) { QVERIFY(changed.load()); QVERIFY(changed.select(changed.version(), ref)); }
	};
	QSignalSpy done(&manager, &CloudSyncManager::workspaceFinished);
	if (mode == 1) {
		manager.runWorkspace(provider, "copy", QUuid::createUuid().toString(QUuid::WithoutBraces)); QCOMPARE(network.accountRequests, 0); QCOMPARE(network.puts, 0); QVERIFY(!manager.syncInProgress()); return;
	}
	if (mode == 20 || mode == 21) network.workspaceReadDeferred = true;
	manager.runWorkspace(provider, "inspect-copy");
	if (mode == 20 || mode == 21) {
		QTRY_VERIFY(network.recoveryReply); auto *late = network.recoveryReply;
		if (mode == 20) manager.cancelWorkspace(); else edit();
		network.workspaceReadDeferred = false; late->complete(); QTRY_VERIFY(!manager.syncInProgress());
		QCOMPARE(done.size(), 0); QVERIFY(manager.workspaceCopyReviews.value(provider).id.isEmpty()); QCOMPARE(network.puts, 0); QCOMPARE(manager.serializeCurrentDiveLog(), expectedLocal); return;
	}
	QTRY_COMPARE(done.size(), 1); QVERIFY(!manager.syncInProgress()); QCOMPARE(network.puts, 0);
	const auto review = manager.workspaceCopyReviews.value(provider); QVERIFY(CloudHistoryRecord::validEpoch(review.id)); QVERIFY(review.source.valid()); QCOMPARE(review.parent, QString());
	QVERIFY(manager.workspaceReport(provider).value("copyReview").toMap().value("id").toString() == review.id);
	if (mode == 2) { manager.runWorkspace(provider, "copy", QUuid::createUuid().toString(QUuid::WithoutBraces)); QCOMPARE(network.puts, 0); QCOMPARE(done.size(), 1); return; }
	if (mode == 3) edit(); if (mode == 4) changeChoice(); if (mode == 5) QVERIFY(QFile::remove(checkpointFile));
	if (mode == 6) { manifest.createdAtUtc = manifest.createdAtUtc.addSecs(1); network.files[1].data = manifest.toJson(); }
	if (mode == 7) { const auto body = network.files[1].data; network.files.removeAt(1); network.add("subsurface-neo-sync.json", body); }
	if (mode == 13) network.workspacePayloadAckLost = true;
	QString action = "copy", argument = review.id, savedTicket;
	if (mode >= 14 && mode <= 17) {
		action = "resume-copy"; savedTicket = QUuid::createUuid().toString(QUuid::WithoutBraces); argument = savedTicket;
		const auto copyPath = QDir(directory).filePath("copies/" + ref.key() + "/" + savedTicket);
		QVERIFY(review.source.preserve(QDir(copyPath).filePath("source.json")));
		CloudWorkspaceCopyIntent intent(QDir(copyPath).filePath("intent.json"), ref, review.source, savedTicket, {}); QVERIFY(intent.prepare()); QVERIFY(intent.beginRegistration());
		QVERIFY(root.registerCopy(savedTicket, {}, review.source)); network.files[0].data = root.json(); network.files[0].version = "registered-revision"; QVERIFY(intent.registerTicket());
		if (mode == 15 || mode == 16) QVERIFY(intent.submit());
		if (mode == 15) { network.add(ref.payloadName(savedTicket), local); QVERIFY(intent.identify(google ? network.files.last().id : "id:" + network.files.last().id)); }
		if (mode == 17) { QFile proof(QDir(copyPath).filePath("source.json")); QVERIFY(proof.open(QIODevice::WriteOnly)); proof.write("bad proof"); proof.close(); }
	}
	if (mode == 19) {
		CloudSyncManager restarted(&network); restarted.workspaceDataRootOverride = temporary.path(); restarted.tokens.insert(provider, manager.tokens.value(provider));
		const auto requests = network.accountRequests; restarted.runWorkspace(provider, "copy", review.id); QCOMPARE(network.accountRequests, requests); QVERIFY(!restarted.syncInProgress()); QCOMPARE(network.puts, 0); return;
	}
	if (mode == 22) manager.setPrimaryProvider(provider);
	if (mode == 23) {
		const auto ticket = QUuid::createUuid().toString(QUuid::WithoutBraces); QVERIFY(root.registerUpload(ticket, {}, CloudSyncManifest::sha256(local), local.size()));
		network.add(ref.payloadName(ticket), local); QVERIFY(root.completeUpload(ticket, google ? network.files.last().id : "id:" + network.files.last().id, network.files.last().version)); QVERIFY(root.publish(ticket, {}));
		network.files[0].data = root.json(); network.files[0].version = "competing-head";
	}
	if ((mode >= 8 && mode <= 12) || mode == 18) network.workspaceReadDeferred = true;
	manager.runWorkspace(provider, action, argument);
	if ((mode >= 8 && mode <= 12) || mode == 18) {
		QTRY_VERIFY(network.recoveryReply); auto *late = network.recoveryReply;
		if (mode == 8) manager.cancelWorkspace(); if (mode == 9) manager.disconnectProvider(provider); if (mode == 10) edit(); if (mode == 11) changeChoice();
		if (mode == 12) manager.setPrimaryProvider(provider); if (mode == 18) QVERIFY(QFile::remove(checkpointFile));
		network.workspaceReadDeferred = false; late->complete();
	}
	QTRY_VERIFY(!manager.syncInProgress());
	const bool success = mode == 0 || mode == 14 || mode == 15;
	QCOMPARE(done.size(), success ? 2 : 1);
	if (success) {
		QVERIFY(manager.lastError().isEmpty()); QVERIFY(manager.workspaceReport(provider).value("copied").toBool());
		const auto copied = CloudWorkspaceRecord::parse(network.files[0].data); QVERIFY(copied.valid()); QCOMPARE(copied.value["journal"].toObject()["history"].toArray().size(), 1);
		QCOMPARE(copied.copySource(copied.head()).key(), review.source.key()); if (!savedTicket.isEmpty()) QCOMPARE(copied.head(), savedTicket);
		const auto puts = network.puts; manager.runWorkspace(provider, "inspect-copy"); QTRY_COMPARE(done.size(), 3);
		manager.runWorkspace(provider, "copy", manager.workspaceCopyReviews.value(provider).id); QTRY_COMPARE(done.size(), 4); QCOMPARE(network.puts, puts);
	} else if (mode == 13) {
		const auto copies = manager.workspaceReport(provider).value("copies").toList(); QCOMPARE(copies.size(), 1); QVERIFY(copies[0].toMap().value("uncertain").toBool());
	} else { QCOMPARE(network.puts, 0); }
	if (mode == 16) { const auto copies = manager.workspaceReport(provider).value("copies").toList(); QCOMPARE(copies.size(), 1); QVERIFY(!copies[0].toMap().value("canResume").toBool()); }
	if (mode == 17) QVERIFY(manager.workspaceReport(provider).value("copyRecoveryRequired").toBool());
	QCOMPARE(manager.serializeCurrentDiveLog(), expectedLocal); if (mode != 9) QCOMPARE(CloudCredentialStore::load(manager.syncStateCredentialKey(provider)), baseline);
	if (mode != 4 && mode != 11) { QVERIFY(choiceRead.open(QIODevice::ReadOnly)); QCOMPARE(choiceRead.readAll(), choiceBytes); }
	if (mode == 5 || mode == 18) QVERIFY(!QFileInfo::exists(checkpointFile));
}

QTEST_GUILESS_MAIN(TestCloudSyncManager)
