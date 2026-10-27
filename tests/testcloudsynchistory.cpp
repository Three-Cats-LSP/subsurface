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
	int serial = 0, fault = 0, mutations = 0, renames = 0;
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
				{"createdTime", fault == 17 ? "" : "2026-10-27T00:00:00.000Z"}});
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
