// SPDX-License-Identifier: GPL-2.0
#include "testcloudsynchistory.h"
#include "core/cloudsynchistory.h"
#include "core/cloudsyncmanifest.h"
#include <QJsonArray>
#include <QJsonDocument>
#include <QFile>
#include <QDir>
#include <QLockFile>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QPointer>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QUrlQuery>
#include <QUuid>
#include <cstring>

namespace {
QString uid() { return QUuid::createUuid().toString(QUuid::WithoutBraces); }
const QByteArray A = "<divelog><dives><dive number='1'><notes>Original test log</notes></dive></dives></divelog>";
const QByteArray B = "<divelog><dives><dive number='2'><notes>Saved local changes</notes></dive></dives></divelog>";
const QString Account = CloudSyncManifest::sha256("synthetic-account");
const QString Legacy = "subsurface-neo-sync.json";
class Reply : public QNetworkReply {
public:
	Reply(const QNetworkRequest &r, QByteArray b, int status, QObject *parent, bool defer = false) : QNetworkReply(parent), body(std::move(b)) {
		setRequest(r); setUrl(r.url()); setAttribute(QNetworkRequest::HttpStatusCodeAttribute, status);
		if (status >= 400) setError(ContentAccessDenied, "synthetic fault");
		open(ReadOnly); if (!defer) QTimer::singleShot(0, this, [this] { complete(); });
	}
	void header(const QByteArray &v) { setRawHeader("Dropbox-API-Result", v); }
	void complete() { setFinished(true); emit finished(); }
	void abort() override { setError(OperationCanceledError, "cancelled"); complete(); }
	qint64 bytesAvailable() const override { return body.size() - offset + QNetworkReply::bytesAvailable(); }
protected:
	qint64 readData(char *target, qint64 max) override {
		const auto n = qMin(max, qint64(body.size()) - offset); if (!n) return -1;
		std::memcpy(target, body.constData() + offset, size_t(n)); offset += n; return n;
	}
private:
	QByteArray body; qint64 offset = 0;
};
class Network : public QNetworkAccessManager {
public:
	struct File { QString name, id, rev; QByteArray data; };
	QList<File> files;
	int serial = 0, fault = 0, mutations = 0, renames = 0;
	bool allowed = true, guard = true;
	QPointer<Reply> deferred;
	std::function<void(const QString &)> wrote;
	std::function<void(const QNetworkRequest &)> requested;
	std::function<QByteArray(const QNetworkRequest &, const QByteArray &)> response;
	bool deferReads = false;
	QByteArray json(const QJsonObject &o) { return QJsonDocument(o).toJson(QJsonDocument::Compact); }
	int find(const QString &name) const { for (int i = 0; i < files.size(); ++i) if (files[i].name == name) return i; return -1; }
	void add(const QString &name, const QByteArray &data) {
		const auto id = QString::number(++serial); files.append({name, "id:" + id, "rev-" + id, data});
	}
	QByteArray content(const QString &name) const { const int i = find(name); return i < 0 ? QByteArray() : files[i].data; }
	void seed(int schema = 2) {
		auto m = CloudSyncManifest::forPayload(A); m.schemaVersion = schema;
		if (schema == 2) m.payloadFile = "subsurface-neo-" + m.revisionId + ".xml";
		add(Legacy, m.toJson()); add(m.payloadFileName(), A);
	}
protected:
	QNetworkReply *createRequest(Operation op, const QNetworkRequest &r, QIODevice *outgoing) override {
		if (requested) requested(r);
		const QByteArray sent = outgoing ? outgoing->readAll() : QByteArray();
		const bool google = r.url().host() == "www.googleapis.com";
		const auto path = r.url().path(); const QUrlQuery query(r.url());
		QString name, expected; int index = -1, status = 200; bool write = false, create = false, defer = false, rename = false;
		QByteArray data = sent, body, meta;
		if (!google) {
			const auto arg = QJsonDocument::fromJson(r.rawHeader("Dropbox-API-Arg")).object();
			const QString target = arg.value("path").toString();
			if (target.startsWith("id:")) { for (int i = 0; i < files.size(); ++i) if (files[i].id == target) index = i; if (index >= 0) name = files[index].name; }
			else { name = target.mid(1); index = find(name); }
			write = path == "/2/files/upload"; create = arg.value("mode").toString() == "add"; expected = arg.value("mode").toObject().value("update").toString();
			allowed &= op == PostOperation && (write || path == "/2/files/download");
			if (write) allowed &= arg.value("strict_conflict").toBool() && !arg.value("autorename").toBool() && (create || !expected.isEmpty());
		} else if (op == GetOperation && path == "/drive/v3/files") {
			name = query.queryItemValue("q").section('\'', 1, 1); QJsonArray matches;
			for (const auto &f : files) if (f.name == name) matches.append(QJsonObject{{"id", f.id.mid(3)}, {"name", f.name},
				{"createdTime", fault == 17 ? "" : "2026-10-27T00:00:00.000Z"}, {"mimeType", "application/octet-stream"}});
			body = json({{"files", matches}, {"incompleteSearch", false}});
		} else if (op == PostOperation && path == "/upload/drive/v3/files") {
			create = write = true; const int begin = sent.indexOf("\r\n\r\n") + 4, end = sent.indexOf("\r\n--", begin);
			name = QJsonDocument::fromJson(sent.mid(begin, end - begin)).object().value("name").toString();
			const int start = sent.indexOf("\r\n\r\n", end) + 4; data = sent.mid(start, sent.lastIndexOf("\r\n--") - start);
		} else {
			const auto id = "id:" + path.section('/', -1); for (int i = 0; i < files.size(); ++i) if (files[i].id == id) index = i;
			if (index >= 0) name = files[index].name;
			rename = op == CustomOperation && r.attribute(QNetworkRequest::CustomVerbAttribute).toByteArray() == "PATCH";
			write = op == PutOperation || rename; expected = QString::fromUtf8(r.rawHeader("If-Match"));
			allowed &= op == GetOperation || (write && !expected.isEmpty() &&
				path.startsWith(rename ? "/drive/v2/files/" : "/upload/drive/v2/files/"));
		}
		if (rename) {
			++mutations; ++renames;
			const auto target = QJsonDocument::fromJson(sent).object().value("title").toString();
			const auto old = index < 0 ? CloudHistoryRecord() : CloudHistoryRecord::parse(files[index].data);
			allowed &= old.valid() && old.value["sequence"].toInt() == 0 && old.value["tickets"].toObject().isEmpty() &&
				target == CloudHistoryRecord::rootName(old.value["epoch"].toString()).chopped(5) + "-recovered-" + files[index].id.mid(3) + ".json";
			if (fault == 15 && index >= 0) {
				auto active = old; active.registerUpload(uid(), {}, CloudSyncManifest::sha256(A), A.size());
				files[index].data = active.json(); files[index].rev = "concurrent-registration"; fault = 0;
			}
			if (index < 0 || files[index].rev != expected) status = 412;
			else {
				files[index].name = target; files[index].rev = "rev-" + QString::number(++serial);
				body = json({{"id", files[index].id.mid(3)}, {"title", target}});
				if (fault == 14) { status = 503; fault = 0; }
				if (fault == 16) defer = true;
			}
		} else if (write) {
			++mutations; allowed &= name.startsWith("subsurface-neo-history-");
			const bool root = name.endsWith(".json");
			if (!root) {
				const QString epoch = name.mid(23, 36), ticket = name.mid(60, 36);
				const auto registered = CloudHistoryRecord::parse(content(CloudHistoryRecord::rootName(epoch))).ticket(ticket);
				allowed &= registered.value("state").toString() == "registered" && registered.value("name").toString() == name &&
					registered.value("sha256").toString() == CloudSyncManifest::sha256(data);
			}
			const auto candidate = CloudHistoryRecord::parse(data);
			const bool publish = root && candidate.valid() && !candidate.head().isEmpty() &&
				(index < 0 || candidate.head() != CloudHistoryRecord::parse(files[index].data).head());
			if ((fault == 1 && root && create) || (fault == 2 && !root)) status = 503;
			else if (fault == 4 && root && !create) { status = google ? 412 : 409; fault = 0; }
			else if ((!create && (index < 0 || files[index].rev != expected)) || (!google && create && index >= 0)) status = google ? 412 : 409;
			else {
				if (create) { add(name, data); index = files.size() - 1; }
				else { files[index].data = data; files[index].rev = "rev-" + QString::number(++serial); }
				body = json({{"id", google ? files[index].id.mid(3) : files[index].id}, {"rev", files[index].rev}});
				if (fault == 3 && publish) { status = 503; fault = 0; }
				if (fault == 7 && !root) { auto m = CloudSyncManifest::forPayload(B); files[find(Legacy)].data = m.toJson(); fault = 0; }
				if (fault == 8 && !root) guard = false;
				if (fault == 11 && !root) defer = true;
				if (wrote) wrote(name);
			}
			if (status == 409) body = R"({"error_summary":"path/conflict/file/"})";
		} else if (body.isEmpty()) {
			if (index < 0) { status = google ? 404 : 409; body = R"({"error":{".tag":"path","path":{".tag":"not_found"}}})"; }
			else if (google && path.startsWith("/drive/v2/files/")) body = json({{"id", files[index].id.mid(3)}, {"title", name}, {"etag", files[index].rev}});
			else {
				body = files[index].data;
				meta = json({{"id", files[index].id}, {"name", name}, {"rev", files[index].rev}});
				if (fault == 5 && name.startsWith("subsurface-neo-history-") && name.endsWith(".xml")) { if (google) files[index].rev += "changed"; else meta = "{}"; }
			}
		}
		if (response) body = response(r, body);
		defer |= deferReads;
		auto *reply = new Reply(r, body, status, this, defer); if (defer) deferred = reply;
		if (!meta.isEmpty()) reply->header(meta); return reply;
	}
};
}

void TestCloudSyncHistory::stateTransitions()
{
	auto r = CloudHistoryRecord::create("google-drive", Account, uid(), CloudSyncManifest::sha256("legacy"));
	QVERIFY(r.valid()); const auto first = uid(), second = uid(), losing = uid(), reservation = uid(), restored = uid();
	QVERIFY(!r.publish(first, {}));
	QVERIFY(r.registerUpload(first, {}, CloudSyncManifest::sha256(A), A.size()));
	QVERIFY(!r.registerUpload(first, {}, CloudSyncManifest::sha256(A), A.size()));
	QVERIFY(!r.publish(first, {}));
	QVERIFY(r.completeUpload(first, "object-a", "v1")); QVERIFY(r.publish(first, {}));
	QVERIFY(r.registerUpload(second, first, CloudSyncManifest::sha256(B), B.size()));
	QVERIFY(r.registerUpload(losing, first, CloudSyncManifest::sha256(B), B.size()));
	QVERIFY(r.completeUpload(second, "object-b", "v1")); QVERIFY(r.publish(second, first));
	QVERIFY(r.completeUpload(losing, "object-c", "v1")); QVERIFY(!r.publish(losing, first));
	QVERIFY(r.reserve(reservation, first)); QVERIFY(!r.reserve(reservation, second));
	QVERIFY(r.registerUpload(restored, second, CloudSyncManifest::sha256(A), A.size(), first, reservation));
	QVERIFY(!r.release(reservation)); // An active upload still depends on its reservation.
	QVERIFY(r.completeUpload(restored, "object-restored", "v1")); QVERIFY(r.publish(restored, second));
	QVERIFY(r.release(reservation)); QVERIFY(!r.reserve(reservation, first));
	QVERIFY(!r.publish(first, restored)); QCOMPARE(r.head(), restored);
	QCOMPARE(r.value.value("history").toArray().size(), 3);
	QCOMPARE(CloudHistoryRecord::parse(r.json()).json(), r.json());
}
void TestCloudSyncHistory::invalidRecords_data()
{
	QTest::addColumn<int>("fault");
	for (int i = 0; i < 12; ++i) QTest::newRow(qPrintable(QString::number(i))) << i;
}
void TestCloudSyncHistory::invalidRecords()
{
	QFETCH(int, fault); auto r = CloudHistoryRecord::create("dropbox", Account, uid(), CloudSyncManifest::sha256("legacy"));
	const auto id = uid(); QVERIFY(r.registerUpload(id, {}, CloudSyncManifest::sha256(A), A.size()));
	QVERIFY(r.completeUpload(id, "id:original", "v1")); QVERIFY(r.publish(id, {}));
	if (fault == 0) r.value["schema"] = 99;
	if (fault == 1) r.value["future-field"] = true;
	if (fault == 2) r.value["head"] = uid();
	if (fault == 3) r.value["history"] = QJsonArray{id, id};
	if (fault == 4) r.value["sequence"] = 1.5;
	if (fault == 5) r.value["account"] = "unbound";
	if (fault == 6) r.value["epoch"] = "../../outside";
	if (fault >= 7) {
		auto tickets = r.value["tickets"].toObject(), t = tickets[id].toObject();
		if (fault == 7) t["state"] = "retired";
		if (fault == 8) t["name"] = "subsurface-neo.xml";
		if (fault == 9) t["bytes"] = -1;
		if (fault == 10) t["version"] = "";
		if (fault == 11) t["parent"] = id;
		tickets[id] = t; r.value["tickets"] = tickets;
	}
	QVERIFY(!r.valid()); QVERIFY(!CloudHistoryRecord::parse(r.json()).valid());
}
void TestCloudSyncHistory::workflow_data()
{
	QTest::addColumn<bool>("google"); QTest::addColumn<int>("schema");
	for (bool g : {false, true}) for (int s : {1, 2}) QTest::newRow(qPrintable(QString("%1-schema%2").arg(g).arg(s))) << g << s;
}
void TestCloudSyncHistory::workflow()
{
	QFETCH(bool, google); QFETCH(int, schema); Network n; n.seed(schema);
	const auto legacy = n.content(Legacy); const auto epoch = uid(); const auto provider = google ? CloudSyncProviderType::GoogleDrive : CloudSyncProviderType::Dropbox;
	CloudSyncHistory client(&n); QSignalSpy done(&client, &CloudSyncHistory::finished), errors(&client, &CloudSyncHistory::failed);
	auto run = [&](const QString &action, const QByteArray &local, const QString &id = QString()) { client.start(provider, "token", Account, epoch, action, id, local, [&] { return n.guard; }); };
	run("list", A); QTRY_COMPARE(done.size(), 1); QCOMPARE(n.mutations, 0);
	run("migrate", A); QTRY_COMPARE(done.size(), 2); QCOMPARE(errors.size(), 0);
	auto first = CloudHistoryRecord::parse(n.content(CloudHistoryRecord::rootName(epoch))); const auto initial = first.head(); QVERIFY(!initial.isEmpty());
	run("save", B); QTRY_COMPARE(done.size(), 3); QCOMPARE(errors.size(), 0);
	const auto saved = CloudHistoryRecord::parse(n.content(CloudHistoryRecord::rootName(epoch))).head(); QVERIFY(saved != initial);
	run("restore", A, initial); QTRY_COMPARE(errors.size(), 1); // Unsaved local state must survive.
	run("restore", B, initial); QTRY_COMPARE(done.size(), 4); QCOMPARE(done.last()[1].toByteArray(), A);
	const auto restored = CloudHistoryRecord::parse(n.content(CloudHistoryRecord::rootName(epoch)));
	QCOMPARE(restored.value["history"].toArray().size(), 3); QVERIFY(restored.head() != initial && restored.head() != saved);
	QVERIFY(restored.ticket(restored.head())["object"] != first.ticket(initial)["object"]);
	QCOMPARE(n.content(Legacy), legacy); QCOMPARE(n.content(CloudSyncManifest::fromJson(legacy).payloadFileName()), A); QVERIFY(n.allowed);
	CloudSyncHistory restarted(&n); QSignalSpy read(&restarted, &CloudSyncHistory::finished);
	restarted.start(provider, "token", Account, epoch, "list", {}, A, [] { return true; }); QTRY_COMPARE(read.size(), 1);
	QCOMPARE(read.last()[0].toMap()["versions"].toList().size(), 3);
}
void TestCloudSyncHistory::interruption_data()
{
	QTest::addColumn<bool>("google"); QTest::addColumn<int>("fault");
	for (bool g : {false, true}) for (int f : {1, 2, 3, 4, 5, 7, 8, 11}) QTest::newRow(qPrintable(QString("%1-fault%2").arg(g).arg(f))) << g << f;
}
void TestCloudSyncHistory::interruption()
{
	QFETCH(bool, google); QFETCH(int, fault); Network n; n.seed(); n.fault = fault;
	const auto before = n.content(Legacy); const auto epoch = uid(); const auto provider = google ? CloudSyncProviderType::GoogleDrive : CloudSyncProviderType::Dropbox;
	CloudSyncHistory client(&n); QSignalSpy done(&client, &CloudSyncHistory::finished), error(&client, &CloudSyncHistory::failed);
	client.start(provider, "token", Account, epoch, "migrate", {}, A, [&] { return n.guard; });
	if (fault == 11) { QTRY_VERIFY(n.deferred); client.cancel(); QTest::qWait(20); QCOMPARE(done.size(), 0); QCOMPARE(error.size(), 0); }
	else if (fault == 3 || fault == 4) { QTRY_COMPARE(done.size(), 1); QCOMPARE(error.size(), 0); }
	else { QTRY_COMPARE(error.size(), 1); QCOMPARE(done.size(), 0); }
	if (fault != 7) QCOMPARE(n.content(Legacy), before);
	QCOMPARE(n.content(CloudSyncManifest::fromJson(before).payloadFileName()), A); QVERIFY(n.allowed);
	n.fault = 0; n.guard = true;
	CloudSyncHistory restarted(&n); QSignalSpy retry(&restarted, &CloudSyncHistory::finished), failed(&restarted, &CloudSyncHistory::failed);
	restarted.start(provider, "token", Account, epoch, fault == 7 ? "list" : "migrate", {}, A, [] { return true; });
	QTRY_COMPARE(retry.size(), 1); QCOMPARE(failed.size(), 0);
	if (fault == 7) QVERIFY(retry.last()[0].toMap()["legacyChanged"].toBool());
	else QVERIFY(retry.last()[0].toMap()["initialized"].toBool());
}
void TestCloudSyncHistory::concurrentInitialization_data()
{
	QTest::addColumn<bool>("google"); QTest::addColumn<bool>("bound");
	for (bool g : {false, true}) for (bool b : {false, true})
		QTest::newRow(qPrintable(QString("%1-bound%2").arg(g).arg(b))) << g << b;
}
void TestCloudSyncHistory::concurrentInitialization()
{
	QFETCH(bool, google); QFETCH(bool, bound); Network n; n.seed(); const auto before = n.content(Legacy); const auto epoch = uid();
	QTemporaryDir directory; QVERIFY(directory.isValid());
	const auto provider = google ? CloudSyncProviderType::GoogleDrive : CloudSyncProviderType::Dropbox;
	CloudSyncHistory a(&n), b(&n); QSignalSpy ad(&a, &CloudSyncHistory::finished), bd(&b, &CloudSyncHistory::finished), ae(&a, &CloudSyncHistory::failed), be(&b, &CloudSyncHistory::failed);
	a.start(provider, "token", Account, epoch, "migrate", {}, A, [] { return true; }, bound ? directory.filePath("a.json") : QString());
	b.start(provider, "token", Account, epoch, "migrate", {}, A, [] { return true; }, bound ? directory.filePath("b.json") : QString());
	QTRY_COMPARE(ad.size() + bd.size() + ae.size() + be.size(), 2);
	QCOMPARE(ae.size() + be.size(), 0); QCOMPARE(ad.size() + bd.size(), 2);
	QCOMPARE(n.content(Legacy), before); QVERIFY(n.allowed);
	const auto root = CloudHistoryRecord::parse(n.content(CloudHistoryRecord::rootName(epoch)));
	QVERIFY(root.valid()); QCOMPARE(root.value["history"].toArray().size(), 1);
	QCOMPARE(ad.last()[0].toMap(), bd.last()[0].toMap());
	int roots = 0; for (const auto &f : n.files) if (f.name == CloudHistoryRecord::rootName(epoch)) ++roots;
	QCOMPARE(roots, 1);
	if (google) { QVERIFY(n.renames >= 1); QVERIFY(n.files.size() >= 5); } // Losing attempts remain protected too.
}
void TestCloudSyncHistory::initialRootRecovery_data()
{
	QTest::addColumn<int>("scenario");
	const char *names[] = {"pristine", "active-newer", "two-active", "unknown-metadata", "other-account", "other-epoch",
		"read-only-list", "lost-rename-reply", "registration-races-rename", "cancel-rename", "missing-creation-time", "legacy-duplicates"};
	for (int i = 0; i < 12; ++i) QTest::newRow(names[i]) << i;
}
void TestCloudSyncHistory::initialRootRecovery()
{
	QFETCH(int, scenario); Network n; n.seed(); const auto legacy = n.content(Legacy);
	const auto epoch = uid(), name = CloudHistoryRecord::rootName(epoch);
	const auto pristine = CloudHistoryRecord::create("google-drive", Account, epoch, CloudSyncManifest::sha256(legacy));
	auto other = pristine;
	if (scenario == 1 || scenario == 2) QVERIFY(other.registerUpload(uid(), {}, CloudSyncManifest::sha256(A), A.size()));
	if (scenario == 3) other.value["unknown-safety-metadata"] = true;
	if (scenario == 4) other.value["account"] = CloudSyncManifest::sha256("other-account");
	if (scenario == 5) other.value["epoch"] = uid();
	n.add(name, scenario == 2 ? other.json() : pristine.json()); n.add(name, other.json());
	const auto firstId = n.files[2].id, secondId = n.files[3].id;
	if (scenario == 7) n.fault = 14;
	if (scenario == 8) n.fault = 15;
	if (scenario == 9) n.fault = 16;
	if (scenario == 10) n.fault = 17;
	if (scenario == 11) n.add(Legacy, legacy);
	CloudSyncHistory client(&n); QSignalSpy done(&client, &CloudSyncHistory::finished), error(&client, &CloudSyncHistory::failed);
	client.start(CloudSyncProviderType::GoogleDrive, "token", Account, epoch, scenario == 6 ? "list" : "migrate", {}, A, [] { return true; });
	if (scenario == 9) { QTRY_VERIFY(n.deferred); client.cancel(); QTest::qWait(20); QCOMPARE(done.size(), 0); QCOMPARE(error.size(), 0); }
	else if (scenario == 0 || scenario == 1 || scenario == 8) { QTRY_COMPARE(done.size(), 1); QCOMPARE(error.size(), 0); }
	else { QTRY_COMPARE(error.size(), 1); QCOMPARE(done.size(), 0); }
	QCOMPARE(n.content(Legacy), legacy); QCOMPARE(n.content(CloudSyncManifest::fromJson(legacy).payloadFileName()), A); QVERIFY(n.allowed);
	if (scenario == 2 || scenario == 3 || scenario == 4 || scenario == 5 || scenario == 6 || scenario == 10 || scenario == 11) {
		QCOMPARE(n.mutations, 0); QCOMPARE(n.files[2].data, scenario == 2 ? other.json() : pristine.json()); QCOMPARE(n.files[3].data, other.json());
		return;
	}
	n.fault = 0;
	CloudSyncHistory restarted(&n); QSignalSpy retry(&restarted, &CloudSyncHistory::finished), failed(&restarted, &CloudSyncHistory::failed);
	restarted.start(CloudSyncProviderType::GoogleDrive, "token", Account, epoch, "migrate", {}, A, [] { return true; });
	QTRY_COMPARE(retry.size(), 1); QCOMPARE(failed.size(), 0); QVERIFY(n.allowed);
	const auto r = CloudHistoryRecord::parse(n.content(name)); QVERIFY(r.valid()); QCOMPARE(r.value["history"].toArray().size(), 1);
	QCOMPARE(n.files[n.find(name)].id, scenario == 1 || scenario == 8 ? secondId : firstId);
	const auto archived = scenario == 1 || scenario == 8 ? 2 : 3;
	QCOMPARE(n.files[archived].data, pristine.json()); // Rename retains exact bytes and object identity.
	QCOMPARE(n.files[2].id, firstId); QCOMPARE(n.files[3].id, secondId);
	if (scenario == 1 || scenario == 8) QCOMPARE(r.value["tickets"].toObject().size(), 2); // Pending work stays protected.
}
void TestCloudSyncHistory::initializationRetryGuard_data()
{
	QTest::addColumn<bool>("google"); QTest::addColumn<int>("scenario");
	for (bool g : {false, true}) for (int s = 0; s < 3; ++s)
		QTest::newRow(qPrintable(QString("%1-retry-guard%2").arg(g).arg(s))) << g << s;
}
void TestCloudSyncHistory::initializationRetryGuard()
{
	QFETCH(bool, google); QFETCH(int, scenario); Network n; n.seed(); n.fault = 4;
	const auto legacy = n.content(Legacy); const auto epoch = uid();
	const auto provider = google ? CloudSyncProviderType::GoogleDrive : CloudSyncProviderType::Dropbox;
	CloudSyncHistory client(&n); QSignalSpy done(&client, &CloudSyncHistory::finished), errors(&client, &CloudSyncHistory::failed);
	client.start(provider, "token", Account, epoch, "migrate", {}, A, [&] { return n.guard; });
	QTRY_COMPARE(n.mutations, 2); // Root creation succeeded, registration conflicted; retry is waiting.
	if (scenario == 0) client.cancel();
	if (scenario == 1) n.guard = false;
	if (scenario == 2) client.start(provider, "token", Account, epoch, "list", {}, A, [] { return true; });
	QTest::qWait(650); // Beyond the first retry's maximum delay; no stale timer may write.
	QCOMPARE(n.mutations, 2); QCOMPARE(done.size(), scenario == 2 ? 1 : 0); QCOMPARE(errors.size(), scenario == 1 ? 1 : 0);
	n.guard = true;
	client.start(provider, "token", Account, epoch, "migrate", {}, A, [] { return true; });
	QTRY_COMPARE(done.size(), scenario == 2 ? 2 : 1);
	QCOMPARE(n.content(Legacy), legacy); QVERIFY(n.allowed);
}
void TestCloudSyncHistory::legacyProvenance()
{
	const auto firstHash = CloudSyncManifest::sha256("initial legacy"), nextHash = CloudSyncManifest::sha256("changed legacy");
	auto r = CloudHistoryRecord::create("dropbox", Account, uid(), firstHash);
	QVERIFY(!r.upgradeLegacyProvenance());
	const auto first = uid(), imported = uid(), pending = uid();
	QVERIFY(r.registerUpload(first, {}, CloudSyncManifest::sha256(A), A.size()));
	QVERIFY(r.completeUpload(first, "object-a", "v1")); QVERIFY(r.publish(first, {}));
	QVERIFY(!r.registerUpload(imported, first, CloudSyncManifest::sha256(B), B.size(), {}, {}, nextHash));
	QVERIFY(r.upgradeLegacyProvenance()); const auto upgraded = r.json(); QVERIFY(r.upgradeLegacyProvenance()); QCOMPARE(r.json(), upgraded);
	QCOMPARE(r.ticket(first)["legacy_source"].toString(), firstHash);
	QVERIFY(r.registerUpload(imported, first, CloudSyncManifest::sha256(B), B.size(), {}, {}, nextHash));
	QVERIFY(r.completeUpload(imported, "object-b", "v1"));
	QCOMPARE(r.value["legacy_sha256"].toString(), firstHash); // Accepted source changes only with publication.
	QVERIFY(r.publish(imported, first)); QCOMPARE(r.value["legacy_sha256"].toString(), nextHash);
	QVERIFY(r.registerUpload(pending, imported, CloudSyncManifest::sha256(A), A.size(), {}, {}, firstHash));
	QCOMPARE(CloudHistoryRecord::parse(r.json()).json(), r.json());
	for (int fault = 0; fault < 6; ++fault) {
		auto invalid = r;
		if (fault == 0) invalid.value["legacy_sha256"] = firstHash;
		if (fault == 1) invalid.value["format"] = "neo-history-development-1"; // Old readers must reject the new metadata.
		auto tickets = invalid.value["tickets"].toObject(), t = tickets[imported].toObject();
		if (fault == 2) t["legacy_source"] = "invalid";
		if (fault == 3) t.remove("legacy_source");
		if (fault == 4) t["legacy_source"] = QJsonValue::Null;
		if (fault == 5) { auto initial = tickets[first].toObject(); initial["legacy_source"] = ""; tickets[first] = initial; }
		tickets[imported] = t; invalid.value["tickets"] = tickets; QVERIFY(!invalid.valid());
	}
}
void TestCloudSyncHistory::legacyBranch_data()
{
	QTest::addColumn<bool>("google"); QTest::addColumn<int>("fault");
	for (bool g : {false, true}) for (int f : {0, 2, 3, 4, 7, 8, 11, 20})
		QTest::newRow(qPrintable(QString("%1-legacy%2").arg(g).arg(f))) << g << f;
}
void TestCloudSyncHistory::legacyBranch()
{
	QFETCH(bool, google); QFETCH(int, fault); Network n; n.seed();
	const auto epoch = uid(), name = CloudHistoryRecord::rootName(epoch);
	const auto initialLegacy = n.content(Legacy);
	const auto provider = google ? CloudSyncProviderType::GoogleDrive : CloudSyncProviderType::Dropbox;
	CloudSyncHistory client(&n); QSignalSpy done(&client, &CloudSyncHistory::finished), errors(&client, &CloudSyncHistory::failed);
	auto run = [&](const QString &action, const QByteArray &local = A, const QString &revision = QString()) {
		client.start(provider, "token", Account, epoch, action, revision, local, [&] { return n.guard; });
	};
	run("migrate"); QTRY_COMPARE(done.size(), 1);
	const auto initial = CloudHistoryRecord::parse(n.content(name)); const auto first = initial.head();
	const auto oldPayloadName = initial.ticket(first)["name"].toString(); const auto oldPayload = n.content(oldPayloadName);
	// Simulate an old client publishing a separate schema 2 branch.
	auto branch = CloudSyncManifest::forPayload(B); branch.schemaVersion = 2; branch.payloadFile = "subsurface-neo-" + branch.revisionId + ".xml";
	n.add(branch.payloadFileName(), B); n.files[n.find(Legacy)].data = branch.toJson(); n.files[n.find(Legacy)].rev += "new-branch";
	run("list"); QTRY_COMPARE(done.size(), 2); QVERIFY(done.last()[0].toMap()["legacyChanged"].toBool());
	const int before = n.mutations;
	run("save"); QTRY_COMPARE(errors.size(), 1); QCOMPARE(n.mutations, before);
	n.fault = fault == 20 ? 0 : fault;
	if (fault == 20) n.files[n.find(branch.payloadFileName())].data = "<divelog>truncated";
	run("import-legacy");
	if (fault == 11) { QTRY_VERIFY(n.deferred); client.cancel(); QTest::qWait(20); QCOMPARE(done.size(), 2); }
	else if (fault == 0 || fault == 3) { QTRY_COMPARE(done.size(), 3); QCOMPARE(errors.size(), 1); QVERIFY(done.last()[1].toByteArray().isEmpty()); }
	else { QTRY_COMPARE(errors.size(), 2); QCOMPARE(done.size(), 2); }
	QCOMPARE(n.content(oldPayloadName), oldPayload); QCOMPARE(n.content(CloudSyncManifest::fromJson(initialLegacy).payloadFileName()), A);
	QVERIFY(n.allowed);
	const auto attempt = CloudHistoryRecord::parse(n.content(name)); QVERIFY(attempt.valid());
	if (fault != 0 && fault != 3) { QCOMPARE(attempt.head(), first); QCOMPARE(attempt.value["legacy_sha256"], initial.value["legacy_sha256"]); }
	// Restore only the synthetic old-client input after the injected fault, then
	// retry with a fresh service. Any failed upload/ticket remains protected.
	n.fault = 0; n.guard = true; n.files[n.find(Legacy)].data = branch.toJson(); n.files[n.find(branch.payloadFileName())].data = B;
	CloudSyncHistory restarted(&n); QSignalSpy retry(&restarted, &CloudSyncHistory::finished), failed(&restarted, &CloudSyncHistory::failed);
	restarted.start(provider, "token", Account, epoch, "import-legacy", {}, A, [] { return true; });
	QTRY_COMPARE(retry.size(), 1); QCOMPARE(failed.size(), 0); QVERIFY(retry.last()[1].toByteArray().isEmpty());
	auto imported = CloudHistoryRecord::parse(n.content(name)); QVERIFY(imported.valid());
	QCOMPARE(imported.value["format"].toString(), QString("neo-history-development-2"));
	QCOMPARE(imported.value["history"].toArray().size(), 2); QCOMPARE(imported.value["history"].toArray().first().toString(), first);
	QCOMPARE(imported.value["legacy_sha256"].toString(), CloudSyncManifest::sha256(branch.toJson()));
	const auto copy = imported.head(); QCOMPARE(n.content(imported.ticket(copy)["name"].toString()), B);
	const auto oldTicket = initial.ticket(first), preserved = imported.ticket(first);
	for (auto it = oldTicket.begin(); it != oldTicket.end(); ++it) QCOMPARE(preserved[it.key()], it.value());
	for (const auto &key : attempt.value["tickets"].toObject().keys()) QVERIFY(imported.value["tickets"].toObject().contains(key));
	const int settled = n.mutations;
	restarted.start(provider, "token", Account, epoch, "import-legacy", {}, A, [] { return true; });
	QTRY_COMPARE(retry.size(), 2); QCOMPARE(n.mutations, settled); // A lost confirmation cannot duplicate the branch.
	run("save"); QTRY_COMPARE(done.size(), fault == 0 || fault == 3 ? 4 : 3);
	run("restore", A, copy); QTRY_COMPARE(done.size(), fault == 0 || fault == 3 ? 5 : 4); QCOMPARE(done.last()[1].toByteArray(), B);
	QCOMPARE(n.content(Legacy), branch.toJson()); QCOMPARE(n.content(branch.payloadFileName()), B); QVERIFY(n.allowed);
}
void TestCloudSyncHistory::authorityCheckpoint()
{
	QTemporaryDir directory; QVERIFY(directory.isValid()); const auto path = directory.filePath("authority.json");
	auto read = [&] { QFile f(path); if (!f.open(QIODevice::ReadOnly)) return QByteArray(); return f.readAll(); };
	CloudHistoryAuthority store(path); QVERIFY(store.load()); QVERIFY(store.id().isEmpty()); QVERIFY(!QFile::exists(path));
	auto r = CloudHistoryRecord::create("google-drive", Account, uid(), CloudSyncManifest::sha256("legacy"));
	const auto first = uid(), pending = uid(), reservation = uid();
	QVERIFY(store.observe("root-a", r)); const auto pristine = r;
	QVERIFY(r.registerUpload(first, {}, CloudSyncManifest::sha256(A), A.size())); QVERIFY(store.observe("root-a", r));
	QVERIFY(r.completeUpload(first, "payload-a", "v1")); QVERIFY(store.observe("root-a", r));
	QVERIFY(r.publish(first, {})); QVERIFY(store.observe("root-a", r));
	QVERIFY(r.registerUpload(pending, first, CloudSyncManifest::sha256(B), B.size()));
	QVERIFY(r.completeUpload(pending, "payload-b", "v1"));
	QVERIFY(r.reserve(reservation, first)); QVERIFY(r.release(reservation)); QVERIFY(store.observe("root-a", r));
	const auto bytes = read(); QVERIFY(!bytes.contains(A));
	CloudHistoryAuthority restarted(path); QVERIFY(restarted.load()); QCOMPARE(restarted.id(), QString("root-a")); QCOMPARE(restarted.record().json(), r.json());
	QVERIFY(!restarted.observe("replacement", r)); QVERIFY(!restarted.observe("root-a", pristine)); QCOMPARE(read(), bytes);
	for (int fault = 0; fault < 7; ++fault) {
		auto candidate = r; auto tickets = candidate.value["tickets"].toObject(), t = tickets[pending].toObject();
		if (fault == 0) { t["created_at"] = "2026-01-01T00:00:00.000Z"; tickets[pending] = t; }
		if (fault == 1) { tickets.remove(pending); }
		if (fault == 2) { t["state"] = "registered"; t["object"] = ""; t["version"] = ""; tickets[pending] = t; }
		if (fault == 3) { t["sha256"] = CloudSyncManifest::sha256("rewritten pending data"); tickets[pending] = t; }
		if (fault == 4) { t["object"] = "replacement-object"; tickets[pending] = t; }
		if (fault == 5) { t["version"] = "replacement-version"; tickets[pending] = t; }
		candidate.value["tickets"] = tickets;
		if (fault == 6) { auto reservations = candidate.value["reservations"].toObject(), v = reservations[reservation].toObject(); v["active"] = true; reservations[reservation] = v; candidate.value["reservations"] = reservations; }
		if (fault != 0) candidate.value["sequence"] = r.value["sequence"].toInt() + 1;
		QVERIFY(candidate.valid()); QVERIFY(!candidate.follows(r)); QVERIFY(!restarted.observe("root-a", candidate)); QCOMPARE(read(), bytes);
	}
	QLockFile lock(path + ".lock"); QVERIFY(lock.tryLock(0)); QVERIFY(!restarted.load()); QVERIFY(!restarted.observe("root-a", r)); lock.unlock();
	QVERIFY(restarted.load()); QCOMPARE(read(), bytes);
	const auto beforeUpgrade = r; QVERIFY(r.upgradeLegacyProvenance()); QVERIFY(r.follows(beforeUpgrade)); QVERIFY(restarted.observe("root-a", r));
	const auto imported = uid(), legacy = CloudSyncManifest::sha256("new-legacy");
	QVERIFY(r.registerUpload(imported, first, CloudSyncManifest::sha256(B), B.size(), {}, {}, legacy));
	QVERIFY(r.completeUpload(imported, "payload-import", "v1")); QVERIFY(r.publish(imported, first));
	QVERIFY(restarted.observe("root-a", r)); QVERIFY(!beforeUpgrade.follows(r));
	QFile corrupt(path); QVERIFY(corrupt.open(QIODevice::WriteOnly)); QCOMPARE(corrupt.write("broken checkpoint"), qint64(17)); corrupt.close();
	QVERIFY(!restarted.load()); QVERIFY(!restarted.observe("root-a", r)); QCOMPARE(read(), QByteArray("broken checkpoint"));
}
void TestCloudSyncHistory::authorityPristineAlias()
{
	QTemporaryDir directory; QVERIFY(directory.isValid()); const auto path = directory.filePath("authority.json");
	const auto pristine = CloudHistoryRecord::create("google-drive", Account, uid(), CloudSyncManifest::sha256("legacy"));
	auto next = pristine; const auto ticket = uid(); QVERIFY(next.registerUpload(ticket, {}, CloudSyncManifest::sha256(A), A.size()));
	CloudHistoryAuthority store(path); QVERIFY(store.observe("old-root", pristine));
	QVERIFY(!store.rebindPristine("wrong-root", pristine, "new-root", next));
	QVERIFY(!store.rebindPristine("old-root", next, "new-root", next)); // The archived object must still be exactly pristine.
	QVERIFY(store.rebindPristine("old-root", pristine, "new-root", next));
	CloudHistoryAuthority restarted(path); QVERIFY(restarted.load()); QCOMPARE(restarted.id(), QString("new-root"));
	QVERIFY(!restarted.observe("old-root", next)); QVERIFY(!restarted.rebindPristine("new-root", pristine, "third-root", next));
}
void TestCloudSyncHistory::boundAuthority_data()
{
	QTest::addColumn<bool>("google"); QTest::addColumn<int>("fault");
	for (bool g : {false, true}) for (int f = 0; f < 12; ++f)
		if (g || f < 9) QTest::newRow(qPrintable(QString("%1-bound%2").arg(g).arg(f))) << g << f;
}
void TestCloudSyncHistory::boundAuthority()
{
	QFETCH(bool, google); QFETCH(int, fault); Network n; n.seed();
	QTemporaryDir directory; QVERIFY(directory.isValid()); const auto path = directory.filePath("authority.json");
	const auto epoch = uid(), name = CloudHistoryRecord::rootName(epoch); const auto legacy = n.content(Legacy);
	const auto provider = google ? CloudSyncProviderType::GoogleDrive : CloudSyncProviderType::Dropbox;
	const auto pristine = CloudHistoryRecord::create(google ? "google-drive" : "dropbox", Account, epoch, CloudSyncManifest::sha256(legacy));
	CloudSyncHistory initial(&n); QSignalSpy initialized(&initial, &CloudSyncHistory::finished), initialError(&initial, &CloudSyncHistory::failed);
	if (fault >= 10) {
		n.add(name, pristine.json());
		CloudHistoryAuthority store(path); QVERIFY(store.observe(n.files[n.find(name)].id.mid(google ? 3 : 0), pristine));
	} else {
		initial.start(provider, "token", Account, epoch, "migrate", {}, A, [] { return true; }, path);
		QTRY_COMPARE(initialized.size(), 1); QCOMPARE(initialError.size(), 0);
	}
	const int index = n.find(name); QVERIFY(index >= 0); const auto originalFile = n.files[index];
	const auto original = CloudHistoryRecord::parse(originalFile.data);
	QFile file(path); QVERIFY(file.open(QIODevice::ReadOnly)); const auto checkpoint = file.readAll(); file.close();
	if (fault == 0) n.files[index].rev += "metadata-only-change";
	if (fault == 1) n.files[index].name = "kept-but-renamed.json";
	if (fault == 2) { n.files[index].name = "kept-original-root.json"; n.add(name, originalFile.data); }
	if (fault == 3) n.files[index].data = pristine.json();
	if (fault == 4) {
		auto changed = original; auto tickets = changed.value["tickets"].toObject(), t = tickets[changed.head()].toObject();
		t["created_at"] = "2026-01-01T00:00:00.000Z"; tickets[changed.head()] = t; changed.value["tickets"] = tickets;
		QVERIFY(changed.valid()); n.files[index].data = changed.json();
	}
	if (fault == 5) { QVERIFY(file.open(QIODevice::WriteOnly)); file.write("unsupported checkpoint"); file.close(); }
	if (fault == 6) { auto changed = pristine; changed.value["sequence"] = original.value["sequence"].toInt() + 10; QVERIFY(changed.valid()); n.files[index].data = changed.json(); }
	if (fault == 7) { QVERIFY(file.remove()); QVERIFY(QDir().mkdir(path)); }
	if (fault == 9) { n.files[index].name = "kept-original-root.json"; n.add(name, pristine.json()); n.add(name, pristine.json()); }
	if (fault >= 10) {
		const QString oldId = originalFile.id.mid(3);
		n.files[index].name = name.chopped(5) + "-recovered-" + oldId + ".json";
		if (fault == 11) { auto changed = pristine; QVERIFY(changed.registerUpload(uid(), {}, CloudSyncManifest::sha256(A), A.size())); n.files[index].data = changed.json(); }
		n.add(name, pristine.json());
	}
	const int before = n.mutations;
	CloudSyncHistory restarted(&n); QSignalSpy done(&restarted, &CloudSyncHistory::finished), errors(&restarted, &CloudSyncHistory::failed);
	restarted.start(provider, "token", fault == 8 ? CloudSyncManifest::sha256("other-account") : Account, epoch,
		fault == 9 ? "migrate" : "list", {}, A, [] { return true; }, path);
	if (fault == 0 || fault == 10) {
		QTRY_COMPARE(done.size(), 1); QCOMPARE(errors.size(), 0);
		if (fault == 10) { CloudHistoryAuthority check(path); QVERIFY(check.load()); QCOMPARE(check.id(), n.files[n.find(name)].id.mid(3)); }
	} else { QTRY_COMPARE(errors.size(), 1); QCOMPARE(done.size(), 0); }
	QCOMPARE(n.mutations, before); QCOMPARE(n.content(Legacy), legacy); QVERIFY(n.allowed);
	if (fault != 5 && fault != 7 && fault != 10) { QVERIFY(file.open(QIODevice::ReadOnly)); QCOMPARE(file.readAll(), checkpoint); }
}
void TestCloudSyncHistory::authorityStorageFailure()
{
	for (bool google : {false, true}) {
		Network n; n.seed(); const auto legacy = n.content(Legacy); const auto epoch = uid(), name = CloudHistoryRecord::rootName(epoch);
		QTemporaryDir directory; QVERIFY(directory.isValid()); const auto blocked = directory.filePath("checkpoint-dir"), path = blocked + "/authority.json";
		bool blockedOnce = false;
		n.wrote = [&](const QString &written) { if (written == name && !blockedOnce) { QFile blocker(blocked); QVERIFY(blocker.open(QIODevice::WriteOnly)); blocker.write("synthetic storage failure"); blockedOnce = true; } };
		const auto provider = google ? CloudSyncProviderType::GoogleDrive : CloudSyncProviderType::Dropbox;
		CloudSyncHistory client(&n); QSignalSpy done(&client, &CloudSyncHistory::finished), errors(&client, &CloudSyncHistory::failed);
		client.start(provider, "token", Account, epoch, "migrate", {}, A, [] { return true; }, path);
		QTRY_COMPARE(errors.size(), 1); QCOMPARE(done.size(), 0); QCOMPARE(n.mutations, 1);
		QVERIFY(CloudHistoryRecord::parse(n.content(name)).pristine()); QCOMPARE(n.content(Legacy), legacy); QVERIFY(n.allowed);
		QVERIFY(QFile::remove(blocked));
		client.start(provider, "token", Account, epoch, "migrate", {}, A, [] { return true; }, path);
		QTRY_COMPARE(done.size(), 1); CloudHistoryAuthority check(path); QVERIFY(check.load()); QVERIFY(!check.record().head().isEmpty()); QVERIFY(n.allowed);
	}
}
void TestCloudSyncHistory::rootReview_data()
{
	QTest::addColumn<bool>("google"); QTest::addColumn<int>("scenario");
	for (bool g : {false, true}) for (int s = 0; s < (g ? 24 : 12); ++s)
		QTest::newRow(qPrintable(QString("%1-review%2").arg(g).arg(s))) << g << s;
}
void TestCloudSyncHistory::rootReview()
{
	QFETCH(bool, google); QFETCH(int, scenario);
	Network n; n.seed(); const auto epoch = uid(), name = CloudHistoryRecord::rootName(epoch);
	const auto provider = google ? CloudSyncProviderType::GoogleDrive : CloudSyncProviderType::Dropbox;
	auto record = CloudHistoryRecord::create(google ? "google-drive" : "dropbox", Account, epoch, CloudSyncManifest::sha256(n.content(Legacy)));
	const auto committed = uid();
	QVERIFY(record.registerUpload(committed, {}, CloudSyncManifest::sha256(A), A.size()));
	QVERIFY(record.completeUpload(committed, "payload-object", "payload-rev")); QVERIFY(record.publish(committed, {}));
	if (scenario == 2) {
		QVERIFY(record.registerUpload(uid(), committed, CloudSyncManifest::sha256(B), B.size()));
		QVERIFY(record.reserve(uid(), committed));
	}
	if (scenario != 0) n.add(name, record.json());
	if (scenario == 3) n.files.last().data = "{\"unsupported\":true}";
	if (scenario == 4) { auto other = record; other.value["account"] = CloudSyncManifest::sha256("other"); n.files.last().data = other.json(); }
	if (scenario == 5) { auto other = record; other.value["epoch"] = uid(); n.files.last().data = other.json(); }
	if (scenario == 12 || scenario == 14 || scenario == 15) n.add(name, record.json());
	if (scenario == 13) n.add(name, CloudHistoryRecord::create("google-drive", Account, epoch, record.value["legacy_sha256"].toString()).json());
	if (scenario == 21) for (int i = 0; i < 16; ++i) n.add(name, record.json());
	QTemporaryDir directory; const auto path = directory.filePath("authority.json");
	if (scenario >= 7 && scenario <= 9) {
		auto checkpoint = record;
		if (scenario == 8) QVERIFY(checkpoint.registerUpload(uid(), committed, CloudSyncManifest::sha256(B), B.size()));
		const QString id = scenario == 9 ? (google ? "missing-root" : "id:missing-root") : (google ? n.files.last().id.mid(3) : n.files.last().id);
		CloudHistoryAuthority authority(path); QVERIFY(authority.observe(id, checkpoint));
	}
	const auto savedCheckpoint = [&] { QFile f(path); if (!f.open(QIODevice::ReadOnly)) return QByteArray(); return f.readAll(); }();
	int mediaReads = 0, listReads = 0;
	n.requested = [&](const QNetworkRequest &request) {
		const auto p = request.url().path();
		const bool media = google ? (p.startsWith("/drive/v3/files/") && QUrlQuery(request.url()).queryItemValue("alt") == "media") : p == "/2/files/download";
		if (media && ++mediaReads == 2) {
			if (scenario == 6) n.files[n.find(name)].rev += "changed";
			if (scenario == 11) n.guard = false;
		}
	};
	n.response = [&](const QNetworkRequest &request, const QByteArray &body) -> QByteArray {
		if (request.url().path() != "/drive/v3/files") return body;
		++listReads; auto page = QJsonDocument::fromJson(body).object(); auto entries = page["files"].toArray();
		if (scenario == 14 || scenario == 15) {
			const bool second = !QUrlQuery(request.url()).queryItemValue("pageToken").isEmpty();
			page["files"] = QJsonArray{entries[second ? 1 : 0]};
			if (!second || scenario == 15) page["nextPageToken"] = "next";
		}
		if (scenario == 16) { entries.append(entries.first()); page["files"] = entries; }
		if (scenario == 17) page["incompleteSearch"] = true;
		if (scenario == 18 || scenario == 19) {
			auto entry = entries.first().toObject(); entry[scenario == 18 ? "mimeType" : "name"] = scenario == 18 ? "application/vnd.google-apps.folder" : "unrelated.json";
			page["files"] = QJsonArray{entry};
		}
		if (scenario == 20) return "invalid JSON";
		if (scenario == 22 && listReads == 2) page["files"] = QJsonArray();
		if (scenario == 23) page["nextPageToken"] = 17;
		return QJsonDocument(page).toJson(QJsonDocument::Compact);
	};
	if (scenario == 10) n.deferReads = true;
	CloudSyncHistory client(&n); QSignalSpy done(&client, &CloudSyncHistory::finished), errors(&client, &CloudSyncHistory::failed);
	client.start(provider, "token", Account, epoch, "review-roots", {}, A, [&] { return n.guard; }, path);
	if (scenario == 10) {
		QTRY_VERIFY(n.deferred); client.cancel(); QTest::qWait(20); QCOMPARE(done.size(), 0); QCOMPARE(errors.size(), 0);
	} else if ((scenario >= 3 && scenario <= 6) || scenario == 11 || scenario >= 15) {
		QTRY_COMPARE(errors.size(), 1); QCOMPARE(done.size(), 0);
	} else {
		QTRY_COMPARE(done.size(), 1); QCOMPARE(errors.size(), 0);
		const auto report = done.first()[0].toMap(); QVERIFY(report["branchesReviewed"].toBool());
		QVERIFY(!report["initialized"].toBool()); QVERIFY(done.first()[1].toByteArray().isEmpty());
		const auto branches = report["branches"].toList();
		QCOMPARE(branches.size(), scenario == 0 ? 0 : (scenario >= 12 ? 2 : 1));
		if (!branches.isEmpty()) {
			const auto branch = branches.first().toMap(); QCOMPARE(branch["versions"].toInt(), 1);
			QCOMPARE(branch["pendingUploads"].toInt(), scenario == 2 ? 1 : 0);
			QCOMPARE(branch["activeReservations"].toInt(), scenario == 2 ? 1 : 0);
			QCOMPARE(branch["matchesCheckpoint"].toBool(), scenario == 7);
		}
		QCOMPARE(report["knownRootMissing"].toBool(), scenario == 9);
		QCOMPARE(report["checkpointConflict"].toBool(), scenario == 8);
		if (scenario == 12) QCOMPARE(report["activeBranches"].toInt(), 2);
		if (scenario == 14) QCOMPARE(listReads, 4);
	}
	QCOMPARE(n.mutations, 0); QCOMPARE(n.renames, 0); QVERIFY(n.allowed);
	QFile checkpoint(path); if (savedCheckpoint.isEmpty()) QVERIFY(!checkpoint.exists());
	else { QVERIFY(checkpoint.open(QIODevice::ReadOnly)); QCOMPARE(checkpoint.readAll(), savedCheckpoint); }
}
void TestCloudSyncHistory::restoreRejection_data()
{
	QTest::addColumn<bool>("google"); QTest::addColumn<int>("fault");
	for (bool g : {false, true}) for (int f = 0; f < 7; ++f) QTest::newRow(qPrintable(QString("%1-restore%2").arg(g).arg(f))) << g << f;
}
void TestCloudSyncHistory::restoreRejection()
{
	QFETCH(bool, google); QFETCH(int, fault); Network n; n.seed(); const auto epoch = uid();
	const auto provider = google ? CloudSyncProviderType::GoogleDrive : CloudSyncProviderType::Dropbox;
	CloudSyncHistory client(&n); QSignalSpy done(&client, &CloudSyncHistory::finished), error(&client, &CloudSyncHistory::failed);
	client.start(provider, "token", Account, epoch, "migrate", {}, A, [&] { return n.guard; }); QTRY_COMPARE(done.size(), 1);
	const auto rootName = CloudHistoryRecord::rootName(epoch);
	auto record = CloudHistoryRecord::parse(n.content(rootName)); const auto id = record.head();
	const auto snapshotName = record.ticket(id).value("name").toString();
	const auto originalLegacy = n.content(Legacy);
	if (fault == 0) n.files[n.find(snapshotName)].data = "<divelog>truncated";
	if (fault == 1) n.files[n.find(snapshotName)].rev += "changed";
	if (fault == 2) { record.value["account"] = CloudSyncManifest::sha256("other-account"); n.files[n.find(rootName)].data = record.json(); }
	if (fault == 3) { record.value["unknown_safety_metadata"] = true; n.files[n.find(rootName)].data = record.json(); }
	if (fault == 4) n.wrote = [&](const QString &) { n.guard = false; }; // Local edit while acquiring the reservation.
	if (fault == 5) { n.files[n.find(snapshotName)].name = "preserved-but-renamed.xml"; }
	if (fault == 6) { record.value["epoch"] = uid(); n.files[n.find(rootName)].data = record.json(); }
	client.start(provider, "token", Account, epoch, "restore", id, A, [&] { return n.guard; });
	QTRY_COMPARE(error.size(), 1); QCOMPARE(done.size(), 1); QCOMPARE(n.content(Legacy), originalLegacy);
	const auto after = QJsonDocument::fromJson(n.content(rootName)).object();
	QCOMPARE(after.value("head").toString(), id); QCOMPARE(after.value("history").toArray().size(), 1);
	QVERIFY(n.allowed);
}
QTEST_GUILESS_MAIN(TestCloudSyncHistory)
