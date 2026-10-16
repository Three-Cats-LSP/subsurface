// SPDX-License-Identifier: GPL-2.0
#include "testcloudsynchistory.h"
#include "core/cloudsynchistory.h"
#include "core/cloudsyncmanifest.h"
#include <QJsonArray>
#include <QJsonDocument>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QPointer>
#include <QSignalSpy>
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
	int serial = 0, fault = 0, mutations = 0;
	bool allowed = true, guard = true;
	QPointer<Reply> deferred;
	std::function<void(const QString &)> wrote;
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
		const QByteArray sent = outgoing ? outgoing->readAll() : QByteArray();
		const bool google = r.url().host() == "www.googleapis.com";
		const auto path = r.url().path(); const QUrlQuery query(r.url());
		QString name, expected; int index = -1, status = 200; bool write = false, create = false, defer = false;
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
			for (const auto &f : files) if (f.name == name) matches.append(QJsonObject{{"id", f.id.mid(3)}, {"name", f.name}});
			body = json({{"files", matches}, {"incompleteSearch", false}});
		} else if (op == PostOperation && path == "/upload/drive/v3/files") {
			create = write = true; const int begin = sent.indexOf("\r\n\r\n") + 4, end = sent.indexOf("\r\n--", begin);
			name = QJsonDocument::fromJson(sent.mid(begin, end - begin)).object().value("name").toString();
			const int start = sent.indexOf("\r\n\r\n", end) + 4; data = sent.mid(start, sent.lastIndexOf("\r\n--") - start);
		} else {
			const auto id = "id:" + path.section('/', -1); for (int i = 0; i < files.size(); ++i) if (files[i].id == id) index = i;
			if (index >= 0) name = files[index].name;
			write = op == PutOperation; expected = QString::fromUtf8(r.rawHeader("If-Match"));
			allowed &= op == GetOperation || (write && !expected.isEmpty() && path.startsWith("/upload/drive/v2/files/"));
		}
		if (write) {
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
	else if (fault == 3) { QTRY_COMPARE(done.size(), 1); QCOMPARE(error.size(), 0); }
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
	QTest::addColumn<bool>("google"); QTest::newRow("google") << true; QTest::newRow("dropbox") << false;
}
void TestCloudSyncHistory::concurrentInitialization()
{
	QFETCH(bool, google); Network n; n.seed(); const auto before = n.content(Legacy); const auto epoch = uid();
	const auto provider = google ? CloudSyncProviderType::GoogleDrive : CloudSyncProviderType::Dropbox;
	CloudSyncHistory a(&n), b(&n); QSignalSpy ad(&a, &CloudSyncHistory::finished), bd(&b, &CloudSyncHistory::finished), ae(&a, &CloudSyncHistory::failed), be(&b, &CloudSyncHistory::failed);
	a.start(provider, "token", Account, epoch, "migrate", {}, A, [] { return true; });
	b.start(provider, "token", Account, epoch, "migrate", {}, A, [] { return true; });
	QTRY_COMPARE(ad.size() + bd.size() + ae.size() + be.size(), 2);
	QVERIFY(ad.size() + bd.size() <= 1); QCOMPARE(n.content(Legacy), before); QVERIFY(n.allowed);
	if (google) { QCOMPARE(ad.size() + bd.size(), 0); QCOMPARE(ae.size() + be.size(), 2); }
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
