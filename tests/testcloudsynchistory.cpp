// SPDX-License-Identifier: GPL-2.0
#include "testcloudsynchistory.h"
#include "core/cloudsynchistory.h"
#include "core/cloudsyncworkspace.h"
#include "core/cloudsyncworkspacesource.h"
#include "core/cloudsyncworkspacecopy.h"
#include "core/cloudsyncmanifest.h"
#include <QJsonArray>
#include <QJsonDocument>
#include <QFile>
#include <QFileInfo>
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
CloudHistoryRecord fencedBranch(const QString &provider, const QString &epoch)
{
	auto root = CloudHistoryRecord::create(provider, Account, epoch, CloudSyncManifest::sha256("legacy"));
	const auto first = uid(), pending = uid(), uploaded = uid(), reservation = uid(), released = uid();
	if (!root.registerUpload(first, {}, CloudSyncManifest::sha256(A), A.size()) ||
	    !root.completeUpload(first, "original-payload", "payload-rev") || !root.publish(first, {}) ||
	    !root.reserve(reservation, first) || !root.reserve(released, first) || !root.release(released) ||
	    !root.registerUpload(pending, first, CloudSyncManifest::sha256(A), A.size(), first, reservation) ||
	    !root.registerUpload(uploaded, first, CloudSyncManifest::sha256(B), B.size()) ||
	    !root.completeUpload(uploaded, "unpublished-payload", "unpublished-rev")) return {};
	return root;
}
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
class QueuedUploadNetwork : public QNetworkAccessManager {
public:
	QPointer<Reply> last;
	QByteArray sent;
protected:
	QNetworkReply *createRequest(Operation, const QNetworkRequest &request, QIODevice *outgoing) override {
		sent = outgoing ? outgoing->readAll() : QByteArray();
		last = new Reply(request, "{}", 200, this, true); return last;
	}
};
class Network : public QNetworkAccessManager {
public:
	struct File { QString name, id, rev; QByteArray data; };
	QList<File> files;
	int serial = 0, fault = 0, mutations = 0, renames = 0;
	int writeFailureStatus = 0;
	int readFailureStatus = 0;
	QString archiveEpoch;
	int archiveFailureStatus = 0;
	bool loseArchiveAck = false, deferArchive = false;
	bool allowed = true, guard = true;
	QPointer<Reply> deferred;
	std::function<void(const QString &)> wrote;
	std::function<void(const QNetworkRequest &)> requested;
	std::function<QByteArray(const QNetworkRequest &, const QByteArray &)> response;
	std::function<QByteArray(const QByteArray &)> metadataResponse;
	bool deferReads = false;
	bool loseWriteAck = false;
	bool workspaceWrites = false;
	bool workspaceCopies = false;
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
			if (archiveEpoch.isEmpty()) allowed &= old.valid() && old.value["sequence"].toInt() == 0 && old.value["tickets"].toObject().isEmpty() &&
				target == CloudHistoryRecord::rootName(old.value["epoch"].toString()).chopped(5) + "-recovered-" + files[index].id.mid(3) + ".json";
			else allowed &= target == CloudHistoryFence::archiveName(archiveEpoch, path.section('/', -1));
			if (fault == 15 && index >= 0) {
				auto active = old; active.registerUpload(uid(), {}, CloudSyncManifest::sha256(A), A.size());
				files[index].data = active.json(); files[index].rev = "concurrent-registration"; fault = 0;
			}
			if (archiveFailureStatus) status = archiveFailureStatus;
			else if (index < 0 || files[index].rev != expected) status = 412;
			else {
				files[index].name = target; files[index].rev = "rev-" + QString::number(++serial);
				body = json({{"id", files[index].id.mid(3)}, {"title", target}, {"etag", files[index].rev}});
				if (loseArchiveAck) { status = 503; loseArchiveAck = false; }
				if (deferArchive) defer = true;
				if (fault == 14) { status = 503; fault = 0; }
				if (fault == 16) defer = true;
			}
		} else if (write) {
			++mutations; allowed &= name.startsWith("subsurface-neo-history-") || (workspaceWrites && name.startsWith("subsurface-neo-workspace-") && name.endsWith(".json")) ||
				(workspaceCopies && name.startsWith("subsurface-neo-workspace-") && name.endsWith(".xml")) || (!create && index < 0);
			const bool root = name.endsWith(".json");
			if (!root && (create || index >= 0)) {
				if (workspaceCopies && name.startsWith("subsurface-neo-workspace-")) {
					const auto ticket = name.chopped(4).right(36); bool registered = false;
					for (const auto &f : files) {
						const auto control = CloudWorkspaceRecord::parse(f.data); if (!control.valid() || control.reference().payloadName(ticket) != name) continue;
						const auto t = control.ticket(ticket); registered |= t["state"] == QJsonValue("registered") && t["sha256"] == QJsonValue(CloudSyncManifest::sha256(data)) && control.copySource(ticket).valid();
					}
					allowed &= registered;
				} else {
				const QString epoch = name.mid(23, 36), ticket = name.mid(60, 36);
				auto control = CloudHistoryRecord::parse(content(CloudHistoryRecord::rootName(epoch)));
				if (!control.valid()) control = CloudHistoryFence::parse(content(CloudHistoryRecord::rootName(epoch))).record();
				const auto registered = control.ticket(ticket);
				allowed &= registered.value("state").toString() == "registered" && registered.value("name").toString() == name &&
					registered.value("sha256").toString() == CloudSyncManifest::sha256(data);
				}
			}
			const auto candidate = CloudHistoryRecord::parse(data);
			const bool publish = root && candidate.valid() && !candidate.head().isEmpty() &&
				(index < 0 || candidate.head() != CloudHistoryRecord::parse(files[index].data).head());
			if (writeFailureStatus) status = writeFailureStatus;
			else if ((fault == 1 && root && create) || (fault == 2 && !root)) status = 503;
			else if (fault == 4 && root && !create) { status = google ? 412 : 409; fault = 0; }
			else if ((!create && (index < 0 || files[index].rev != expected)) || (!google && create && index >= 0)) status = google ? 412 : 409;
			else {
				if (create) { add(name, data); index = files.size() - 1; }
				else { files[index].data = data; files[index].rev = "rev-" + QString::number(++serial); }
				body = json({{"id", google ? files[index].id.mid(3) : files[index].id}, {"rev", files[index].rev}, {"name", files[index].name}});
				if (fault == 3 && publish) { status = 503; fault = 0; }
				if (loseWriteAck) { status = 503; loseWriteAck = false; }
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
		if (!write && readFailureStatus) status = readFailureStatus;
		if (response) body = response(r, body);
		if (metadataResponse) {
			if (google && path.startsWith("/drive/v2/files/") && op == GetOperation) body = metadataResponse(body);
			else if (!meta.isEmpty()) meta = metadataResponse(meta);
		}
		defer |= deferReads;
		auto *reply = new Reply(r, body, status, this, defer); if (defer) deferred = reply;
		if (!meta.isEmpty()) reply->header(meta); return reply;
	}
};
CloudWorkspaceCopySource copySource(const Network &n, bool google)
{
	const auto &m = n.files[0], &p = n.files[1]; CloudWorkspaceCopySource source;
	source.value = {{"schema", 1}, {"format", "neo-workspace-copy-source-development-1"}, {"provider", google ? "google-drive" : "dropbox"}, {"account", Account},
		{"manifest", QJsonObject{{"name", m.name}, {"id", google ? m.id.mid(3) : m.id}, {"version", m.rev}, {"sha256", CloudSyncManifest::sha256(m.data)}, {"body_base64", QString::fromLatin1(m.data.toBase64())}}},
		{"payload", QJsonObject{{"name", p.name}, {"id", google ? p.id.mid(3) : p.id}, {"version", p.rev}, {"sha256", CloudSyncManifest::sha256(p.data)}, {"bytes", p.data.size()}}}};
	return source;
}
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
void TestCloudSyncHistory::fenceTransitions_data()
{
	QTest::addColumn<bool>("google"); QTest::addColumn<bool>("provenance");
	for (bool g : {false, true}) for (bool p : {false, true})
		QTest::newRow(qPrintable(QString("%1-provenance%2").arg(g).arg(p))) << g << p;
}
void TestCloudSyncHistory::fenceTransitions()
{
	QFETCH(bool, google); QFETCH(bool, provenance);
	const auto epoch = uid(), provider = QString(google ? "google-drive" : "dropbox");
	const auto targetId = QString(google ? "target" : "id:target"), sourceId = QString(google ? "source" : "id:source");
	auto root = fencedBranch(provider, epoch); QVERIFY(root.valid());
	if (provenance) QVERIFY(root.upgradeLegacyProvenance());
	auto target = CloudHistoryFence::freeze(targetId, root); QVERIFY(target.valid());
	const auto frozen = target;
	QCOMPARE(target.record().json(), root.json());
	QVERIFY(!CloudHistoryRecord::parse(target.json()).valid()); // Old writer formats cannot accept the fence.
	QVERIFY(target.intend(sourceId)); QVERIFY(target.follows(frozen));
	const auto intent = target;
	QVERIFY(!target.intend(sourceId)); QCOMPARE(target.json(), intent.json());
	const auto source = CloudHistoryFence::seal(sourceId, root, "source-before-seal", target); QVERIFY(source.valid());
	QCOMPARE(source.record().json(), root.json());
	QVERIFY(!CloudHistoryRecord::parse(source.json()).valid());
	QCOMPARE(source.value["nonce"], target.intent(sourceId)["nonce"]);
	const auto restartedSource = CloudHistoryFence::parse(source.json()); QVERIFY(restartedSource.valid());
	auto restartedTarget = CloudHistoryFence::parse(target.json()); QVERIFY(restartedTarget.valid());
	QVERIFY(restartedTarget.acknowledge(sourceId, restartedSource, "archived-version", CloudHistoryFence::archiveName(epoch, sourceId)));
	QVERIFY(restartedTarget.follows(frozen)); QVERIFY(restartedTarget.follows(intent));
	QCOMPARE(restartedTarget.record().json(), root.json());
	QCOMPARE(restartedTarget.intent(sourceId)["sha256"].toString(), CloudSyncManifest::sha256(source.json()));
	QCOMPARE(restartedTarget.intent(sourceId)["version"].toString(), QString("archived-version"));
	const auto acknowledged = restartedTarget.json();
	QVERIFY(!restartedTarget.acknowledge(sourceId, source, "archived-version", CloudHistoryFence::archiveName(epoch, sourceId)));
	QCOMPARE(restartedTarget.json(), acknowledged);
	QVERIFY(!intent.follows(restartedTarget)); // A lost-ack retry cannot roll back the acknowledged source.
	for (int i = 1; i < 16; ++i) QVERIFY(restartedTarget.intend(google ? "source-" + QString::number(i) : "id:source-" + QString::number(i)));
	const auto full = restartedTarget.json();
	QVERIFY(!restartedTarget.intend(google ? "overflow" : "id:overflow")); QCOMPARE(restartedTarget.json(), full);
	QVERIFY(CloudHistoryFence::parse(full).follows(frozen));
	// Every ticket, intended name, uploaded revision, reservation and provenance
	// stays in its original branch; no ticket tables are merged or dropped.
	QCOMPARE(restartedTarget.record().value, root.value); QCOMPARE(restartedSource.record().value, root.value);
}
void TestCloudSyncHistory::fenceRejection_data()
{
	QTest::addColumn<bool>("google"); QTest::addColumn<int>("scenario");
	const QStringList cases = {"own-intent", "bad-id", "pristine", "unknown-format", "unknown-field", "foreign-account",
		"foreign-epoch", "foreign-provider", "no-intent", "wrong-session", "wrong-nonce", "wrong-target",
		"unconfirmed-version", "wrong-archive", "removed-intent", "changed-intent", "changed-record", "rollback",
		"duplicate-source", "duplicate-nonce", "ack-rewrite", "sealed-change", "nested-coordinator", "record-cap"};
	for (bool g : {false, true}) for (int s = 0; s < cases.size(); ++s)
		QTest::newRow(qPrintable(QString("%1-%2").arg(g).arg(cases[s]))) << g << s;
}
void TestCloudSyncHistory::fenceRejection()
{
	QFETCH(bool, google); QFETCH(int, scenario);
	const auto epoch = uid(), provider = QString(google ? "google-drive" : "dropbox");
	const auto targetId = QString(google ? "target" : "id:target"), sourceId = QString(google ? "source" : "id:source");
	auto root = fencedBranch(provider, epoch); QVERIFY(root.valid());
	auto target = CloudHistoryFence::freeze(targetId, root); QVERIFY(target.intend(sourceId));
	auto source = CloudHistoryFence::seal(sourceId, root, "before-seal", target); QVERIFY(source.valid());
	const auto before = target.json(); const auto name = CloudHistoryFence::archiveName(epoch, sourceId);
	if (scenario == 0 || scenario == 1) {
		QVERIFY(!target.intend(scenario == 0 ? targetId : "invalid/id")); QCOMPARE(target.json(), before);
	} else if (scenario == 2) {
		auto pristine = CloudHistoryRecord::create(provider, Account, epoch, CloudSyncManifest::sha256("legacy"));
		QVERIFY(!CloudHistoryFence::freeze(targetId, pristine).valid());
		QVERIFY(!CloudHistoryFence::seal(sourceId, pristine, "v", target).valid());
	} else if (scenario == 3 || scenario == 4) {
		if (scenario == 3) source.value["format"] = "neo-history-fence-future";
		else source.value["unknown_safety_field"] = true;
		QVERIFY(!CloudHistoryFence::parse(source.json()).valid());
		QVERIFY(!target.acknowledge(sourceId, source, "archived", name)); QCOMPARE(target.json(), before);
	} else if (scenario >= 5 && scenario <= 8) {
		if (scenario == 5) root.value["account"] = CloudSyncManifest::sha256("other-account");
		if (scenario == 6) root = fencedBranch(provider, uid());
		if (scenario == 7) root.value["provider"] = google ? "dropbox" : "google-drive";
		if (scenario == 8) target = CloudHistoryFence::freeze(targetId, root);
		QVERIFY(!CloudHistoryFence::seal(sourceId, root, "v", target).valid());
	} else if (scenario >= 9 && scenario <= 13) {
		if (scenario == 9) source.value["session"] = uid();
		if (scenario == 10) source.value["nonce"] = uid();
		if (scenario == 11) source.value["destination"] = google ? "other-target" : "id:other-target";
		QVERIFY(!target.acknowledge(sourceId, source, scenario == 12 ? "before-seal" : "archived", scenario == 13 ? "wrong-name.json" : name));
		QCOMPARE(target.json(), before);
	} else if (scenario >= 14 && scenario <= 20) {
		const auto previous = target;
		auto entries = target.value["sources"].toArray(); auto entry = entries.first().toObject();
		if (scenario == 14) entries = {};
		if (scenario == 15) { entry["nonce"] = uid(); entries[0] = entry; }
		if (scenario == 16) { QVERIFY(root.registerUpload(uid(), root.head(), CloudSyncManifest::sha256(B), B.size())); target.value["record"] = root.value; }
		if (scenario == 17) target.value["sequence"] = 0;
		if (scenario == 18) { entry["nonce"] = uid(); entries.append(entry); }
		if (scenario == 19) { entry["id"] = google ? "other-source" : "id:other-source"; entries.append(entry); }
		if (scenario == 20) {
			QVERIFY(target.acknowledge(sourceId, source, "archived", name)); const auto acknowledged = target;
			entries = target.value["sources"].toArray(); entry = entries.first().toObject(); entry["version"] = "rewritten"; entries[0] = entry;
			target.value["sources"] = entries; target.value["sequence"] = 3;
			QVERIFY(target.valid()); QVERIFY(!target.follows(acknowledged)); return;
		}
		target.value["sources"] = entries;
		if (scenario != 17) target.value["sequence"] = 10;
		QVERIFY(!target.follows(previous));
	} else if (scenario == 21) {
		const auto sealed = source; source.value["base_version"] = "changed";
		QVERIFY(source.valid()); QVERIFY(!source.follows(sealed)); QVERIFY(!source.intend(targetId));
		source.value["sequence"] = 1; QVERIFY(!source.valid());
	} else if (scenario == 22) {
		CloudHistoryRecord nested; nested.value = target.value;
		QVERIFY(!CloudHistoryFence::seal(sourceId, nested, "v", target).valid());
		QVERIFY(!CloudHistoryFence::freeze(sourceId, nested).valid());
	} else {
		// The entry/byte bounds are independent. Large valid tables are kept
		// whole; oversized serialized input is rejected, never truncated.
		auto tickets = root.value["tickets"].toObject();
		auto t = root.ticket(root.head()); t["state"] = "uploaded"; t["parent"] = root.head();
		for (int i = 0; i < 505; ++i) {
			const auto ticket = uid();
			t["name"] = CloudHistoryRecord::payloadName(epoch, ticket); t["object"] = QString::number(i) + QString(490, 'x'); t["version"] = QString(512, 'v');
			tickets[ticket] = t;
		}
		root.value["tickets"] = tickets; QVERIFY(root.valid());
		const auto large = CloudHistoryFence::freeze(targetId, root); QVERIFY(large.valid());
		const auto bytes = large.json();
		QVERIFY(!CloudHistoryFence::parse(bytes + QByteArray(1024 * 1024, ' ')).valid());
		QCOMPARE(root.json(), large.record().json());
	}
}

void TestCloudSyncHistory::fenceCas_data()
{
	QTest::addColumn<bool>("google"); QTest::addColumn<int>("scenario");
	const QStringList cases = {"writer-before-freeze", "freeze-before-writer", "source-advance", "late-payload",
		"competing-coordinators", "delayed-root", "lost-freeze-ack", "lost-intent-ack", "lost-seal-ack", "lost-acknowledgement-ack",
		"identity-wrong-name", "identity-replaced-path", "identity-wrong-id", "identity-no-version", "identity-no-name"};
	for (bool g : {false, true}) for (int s = 0; s < cases.size(); ++s)
		QTest::newRow(qPrintable(QString("%1-%2").arg(g).arg(cases[s]))) << g << s;
}
void TestCloudSyncHistory::fenceCas()
{
	QFETCH(bool, google); QFETCH(int, scenario);
	Network n; n.seed(); const auto original = n.files;
	const auto epoch = uid(), name = CloudHistoryRecord::rootName(epoch);
	const auto provider = google ? CloudSyncProviderType::GoogleDrive : CloudSyncProviderType::Dropbox;
	auto root = fencedBranch(google ? "google-drive" : "dropbox", epoch); QVERIFY(root.valid());
	n.add(name, root.json());
	// Dropbox has unique paths; Drive deliberately has two equal names.
	n.add(google ? name : name.chopped(5) + "-branch.json", root.json());
	const auto id = [&](int i) { return google ? n.files[i].id.mid(3) : n.files[i].id; };
	const auto targetId = id(2), sourceId = id(3), initialVersion = n.files[2].rev;
	CloudSyncFileStore files(&n); files.setStrictReads(true);
	QSignalSpy written(&files, &CloudSyncFileStore::uploadFinished), errors(&files, &CloudSyncFileStore::operationError),
		conflicts(&files, &CloudSyncFileStore::writeConflict), read(&files, &CloudSyncFileStore::versionedDownloadFinished);
	int stage = 0;
	const auto write = [&](int index, const QByteArray &bytes, const QString &version) {
		files.uploadIdentity(provider, "token", n.files[index].name, bytes, version, id(index));
	};
	const auto persist = [&](int index, const QByteArray &bytes) {
		const int writes = written.size(), failures = errors.size(), reads = read.size();
		const bool lost = scenario == 6 + stage++; n.loseWriteAck = lost;
		write(index, bytes, n.files[index].rev);
		if (!QTest::qWaitFor([&] { return written.size() > writes || errors.size() > failures; }, 2000)) return false;
		if (written.size() != writes + int(!lost) || errors.size() != failures + int(lost)) return false;
		// Recover by exact ID even though the committed write returned an error.
		// No replacement is selected by the shared Drive name.
		files.downloadIdentity(provider, "token", n.files[index].name, id(index));
		if (!QTest::qWaitFor([&] { return read.size() > reads; }, 2000)) return false;
		return read.last()[2].toByteArray() == bytes && read.last()[3].toString() == n.files[index].rev && read.last()[4].toString() == id(index);
	};
	if (scenario >= 10) {
		const auto expected = n.files[2].name;
		if (scenario <= 11) n.files[2].name = name.chopped(5) + "-moved.json";
		if (scenario == 11) n.add(expected, root.json()); // A replacement path is never adopted.
		if (scenario >= 12) n.metadataResponse = [&](const QByteArray &bytes) {
			auto metadata = QJsonDocument::fromJson(bytes).object();
			if (scenario == 12) metadata["id"] = google ? "different-id" : "id:different-id";
			if (scenario == 13) metadata[google ? "etag" : "rev"] = "";
			if (scenario == 14) metadata.remove(google ? "title" : "name");
			return QJsonDocument(metadata).toJson(QJsonDocument::Compact);
		};
		files.setStrictReads(false); // Exact reads still enforce the name without this opt-in.
		files.downloadIdentity(provider, "token", expected, targetId);
		if (google && (scenario <= 11 || scenario == 14)) { QTRY_COMPARE(conflicts.size(), 1); QCOMPARE(errors.size(), 0); }
		else { QTRY_COMPARE(errors.size(), 1); QCOMPARE(conflicts.size(), 0); }
		QCOMPARE(read.size(), 0); QCOMPARE(n.mutations, 0); QVERIFY(n.allowed); return;
	}
	auto target = CloudHistoryFence::freeze(targetId, root); QVERIFY(target.valid());
	if (scenario == 0) {
		auto writer = root; QVERIFY(writer.registerUpload(uid(), writer.head(), CloudSyncManifest::sha256(B), B.size()));
		write(2, writer.json(), initialVersion); QTRY_COMPARE(written.size(), 1);
		write(2, target.json(), initialVersion); QTRY_COMPARE(conflicts.size(), 1);
		QCOMPARE(n.files[2].data, writer.json());
		target = CloudHistoryFence::freeze(targetId, writer); QVERIFY(persist(2, target.json()));
		QCOMPARE(CloudHistoryFence::parse(n.files[2].data).record().json(), writer.json()); return;
	}
	QVERIFY(persist(2, target.json()));
	if (scenario == 1) {
		auto writer = root; QVERIFY(writer.registerUpload(uid(), writer.head(), CloudSyncManifest::sha256(B), B.size()));
		write(2, writer.json(), initialVersion); QTRY_COMPARE(conflicts.size(), 1); QCOMPARE(n.files[2].data, target.json());
	}
	QVERIFY(target.intend(sourceId)); QVERIFY(persist(2, target.json()));
	// Read durable intent instead of assuming a previous process received it.
	target = CloudHistoryFence::parse(n.files[2].data); QVERIFY(target.valid());
	if (scenario == 4) {
		const auto competing = CloudHistoryFence::freeze(sourceId, root); QVERIFY(persist(3, competing.json()));
		QVERIFY(!CloudHistoryFence::seal(sourceId, CloudHistoryRecord::parse(n.files[3].data), n.files[3].rev, target).valid());
		QCOMPARE(n.files[2].data, target.json()); QCOMPARE(n.files[3].data, competing.json());
		// Both coordinators remain frozen. There is no unproved redirect,
		// winner election or unfreeze transition in this foundation.
	} else {
		auto sourceRoot = root;
		auto source = CloudHistoryFence::seal(sourceId, sourceRoot, n.files[3].rev, target); QVERIFY(source.valid());
		if (scenario == 2) {
			const auto staleVersion = n.files[3].rev;
			const auto tickets = sourceRoot.value["tickets"].toObject();
			QString uploaded;
			for (auto it = tickets.begin(); it != tickets.end(); ++it) if (it.value().toObject()["state"] == QJsonValue("uploaded")) uploaded = it.key();
			QVERIFY(!uploaded.isEmpty()); QVERIFY(sourceRoot.publish(uploaded, sourceRoot.head()));
			write(3, sourceRoot.json(), staleVersion); QTRY_COMPARE(written.size(), 3);
			write(3, source.json(), staleVersion); QTRY_COMPARE(conflicts.size(), 1); QCOMPARE(n.files[3].data, sourceRoot.json());
			source = CloudHistoryFence::seal(sourceId, CloudHistoryRecord::parse(n.files[3].data), n.files[3].rev, target); QVERIFY(source.valid());
		}
		const auto staleSourceVersion = n.files[3].rev;
		QVERIFY(persist(3, source.json()));
		if (scenario == 3) {
			const auto tickets = sourceRoot.value["tickets"].toObject(); QString pending;
			for (auto it = tickets.begin(); it != tickets.end(); ++it) if (it.value().toObject()["state"] == QJsonValue("registered")) pending = it.key();
			QVERIFY(!pending.isEmpty()); const auto t = sourceRoot.ticket(pending);
			const int count = written.size(); files.uploadConditional(provider, "token", t["name"].toString(), A, {});
			QTRY_COMPARE(written.size(), count + 1); QCOMPARE(n.files.last().data, A);
			QVERIFY(sourceRoot.completeUpload(pending, id(n.files.size() - 1), n.files.last().rev));
			write(3, sourceRoot.json(), staleSourceVersion); QTRY_COMPARE(conflicts.size(), 1);
			QCOMPARE(n.files[3].data, source.json());
			QCOMPARE(source.record().ticket(pending)["state"].toString(), QString("registered"));
			QCOMPARE(source.record().ticket(pending)["name"], t["name"]); // Late payload stays protected by intended name.
		}
		// Model the external conditional archival step. No native active-root
		// rename action exists yet; verify identity/name/bytes after it here.
		const auto archived = CloudHistoryFence::archiveName(epoch, sourceId);
		n.files[3].name = archived; n.files[3].rev += "-archived";
		const int reads = read.size(); files.downloadIdentity(provider, "token", archived, sourceId); QTRY_COMPARE(read.size(), reads + 1);
		QCOMPARE(read.last()[2].toByteArray(), source.json()); QCOMPARE(read.last()[4].toString(), sourceId);
		QVERIFY(target.acknowledge(read.last()[4].toString(), CloudHistoryFence::parse(read.last()[2].toByteArray()), read.last()[3].toString(), archived));
		QVERIFY(persist(2, target.json()));
		QCOMPARE(target.record().json(), root.json()); QCOMPARE(source.record().json(), scenario == 2 ? sourceRoot.json() : root.json());
		if (scenario == 5) n.add(google ? name : name.chopped(5) + "-delayed.json", root.json());
	}
	// Frozen/unknown formats stop every existing mutation and restore path.
	// Local bytes are never delivered for replacement, and checkpoints remain
	// at the old identity until a separate proven rebinding protocol exists.
	const int mutations = n.mutations;
	CloudSyncHistory client(&n); QSignalSpy done(&client, &CloudSyncHistory::finished), failed(&client, &CloudSyncHistory::failed);
	client.start(provider, "token", Account, epoch, "save", {}, B, [] { return true; });
	QTRY_COMPARE(failed.size(), 1); QCOMPARE(done.size(), 0); QCOMPARE(n.mutations, mutations); QCOMPARE(n.renames, 0);
	for (int i = 0; i < original.size(); ++i) {
		QCOMPARE(n.files[i].id, original[i].id); QCOMPARE(n.files[i].rev, original[i].rev); QCOMPARE(n.files[i].data, original[i].data);
	}
	QVERIFY(n.allowed);
}

void TestCloudSyncHistory::identityWrite_data()
{
	QTest::addColumn<bool>("google"); QTest::addColumn<int>("scenario");
	const QStringList cases = {"exact-id", "moved-with-replacement", "stale-revision", "missing-id", "lost-response", "name-mismatch",
		"invalid-id", "empty-revision", "empty-id", "legacy-provider", "unsafe-name", "empty-token", "newline-revision",
		"long-id", "long-revision", "empty-name", "null-revision", "cancel-after-commit", "throttle", "unauthorized", "server-error",
		"wrong-ack-id", "wrong-ack-name", "missing-ack-revision"};
	for (bool g : {false, true}) for (int s = 0; s < (g ? 21 : cases.size()); ++s)
		QTest::newRow(qPrintable(QString("%1-%2").arg(g).arg(cases[s]))) << g << s;
}
void TestCloudSyncHistory::identityWrite()
{
	QFETCH(bool, google); QFETCH(int, scenario);
	Network n; n.seed(); const auto name = CloudHistoryRecord::rootName(uid()); n.add(name, A);
	const auto targetId = google ? n.files[2].id.mid(3) : n.files[2].id;
	auto provider = google ? CloudSyncProviderType::GoogleDrive : CloudSyncProviderType::Dropbox;
	QString expectedName = name, expectedId = targetId, expectedVersion = n.files[2].rev, token = "token";
	if (scenario == 1 || scenario == 5) {
		n.files[2].name = name.chopped(5) + "-moved.json";
		n.add(name, "preserved replacement");
		if (scenario == 1) expectedName = n.files[2].name;
	}
	if (scenario == 2) n.files[2].rev += "-racing-writer";
	if (scenario == 3) expectedId = google ? "missing-id" : "id:missing-id";
	if (scenario == 4) n.loseWriteAck = true;
	if (scenario == 6) expectedId = "invalid/id";
	if (scenario == 7) expectedVersion.clear();
	if (scenario == 8) expectedId.clear();
	if (scenario == 9) provider = CloudSyncProviderType::SubsurfaceCloud;
	if (scenario == 10) expectedName = "unsafe/path.json";
	if (scenario == 11) token.clear();
	if (scenario == 12) expectedVersion += '\n';
	if (scenario == 13) expectedId += QString(513, 'x');
	if (scenario == 14) expectedVersion = QString(513, 'x');
	if (scenario == 15) expectedName.clear();
	if (scenario == 16) expectedVersion += QChar::Null;
	if (scenario == 17) n.deferReads = true;
	if (scenario >= 18 && scenario <= 20) n.writeFailureStatus = scenario == 18 ? 429 : scenario == 19 ? 401 : 503;
	if (scenario >= 21) n.response = [&](const QNetworkRequest &request, const QByteArray &bytes) {
		if (!request.url().path().endsWith("/upload")) return bytes;
		auto ack = QJsonDocument::fromJson(bytes).object();
		if (scenario == 21) ack["id"] = "id:different-id";
		if (scenario == 22) ack["name"] = "wrong-name.json";
		if (scenario == 23) ack.remove("rev");
		return QJsonDocument(ack).toJson(QJsonDocument::Compact);
	};
	const auto before = n.files; QStringList uploadTargets; int nameLookups = 0;
	n.requested = [&](const QNetworkRequest &request) {
		if (request.url().path() == "/drive/v3/files") ++nameLookups;
		if (request.url().path() == "/2/files/upload") uploadTargets.append(QJsonDocument::fromJson(request.rawHeader("Dropbox-API-Arg")).object()["path"].toString());
		else if (request.url().path().startsWith("/upload/drive/v2/files/")) uploadTargets.append(request.url().path().section('/', -1));
	};
	CloudSyncFileStore files(&n);
	QSignalSpy written(&files, &CloudSyncFileStore::uploadFinished), errors(&files, &CloudSyncFileStore::operationError),
		conflicts(&files, &CloudSyncFileStore::writeConflict), read(&files, &CloudSyncFileStore::versionedDownloadFinished);
	files.uploadIdentity(provider, token, expectedName, B, expectedVersion, expectedId);
	if (scenario >= 6 && scenario <= 16) {
		QCOMPARE(errors.size(), 1); QCOMPARE(written.size(), 0); QCOMPARE(n.mutations, 0); QVERIFY(uploadTargets.isEmpty()); return;
	}
	if (scenario == 17) {
		QTRY_VERIFY(n.deferred); files.cancel(provider); n.deferReads = false; QTest::qWait(20);
		QCOMPARE(written.size(), 0); QCOMPARE(errors.size(), 0); QCOMPARE(conflicts.size(), 0);
	} else {
		QTRY_COMPARE(written.size() + errors.size() + conflicts.size(), 1);
		const bool rejected = scenario == 2 || scenario == 3 || scenario == 4 || scenario >= 18 || (!google && scenario == 5);
		QCOMPARE(written.size(), int(!rejected));
		if (scenario == 19) { QCOMPARE(errors.size(), 1); QVERIFY(errors.first()[3].toBool()); }
	}
	QCOMPARE(n.mutations, 1); QCOMPARE(uploadTargets, QStringList{expectedId}); QCOMPARE(nameLookups, 0); QVERIFY(n.allowed);
	const bool unchanged = scenario == 2 || scenario == 3 || (scenario >= 18 && scenario <= 20);
	QCOMPARE(n.files[2].data, unchanged ? A : B);
	for (int i = 0; i < before.size(); ++i) {
		QCOMPARE(n.files[i].id, before[i].id); QCOMPARE(n.files[i].name, before[i].name);
		if (i != 2 || unchanged) { QCOMPARE(n.files[i].data, before[i].data); QCOMPARE(n.files[i].rev, before[i].rev); }
	}
	if (scenario == 5) {
		const int failures = errors.size() + conflicts.size();
		files.downloadIdentity(provider, "token", expectedName, targetId);
		QTRY_COMPARE(errors.size() + conflicts.size(), failures + 1); QCOMPARE(read.size(), 0);
	} else {
		files.downloadIdentity(provider, "token", n.files[2].name, targetId); QTRY_COMPARE(read.size(), 1);
		QCOMPARE(read.first()[2].toByteArray(), unchanged ? A : B); QCOMPARE(read.first()[4].toString(), targetId);
	}
	QCOMPARE(n.mutations, 1); QCOMPARE(n.files.size(), before.size());
}

void TestCloudSyncHistory::handoffReceipt_data()
{
	QTest::addColumn<bool>("google"); QTest::addColumn<int>("scenario");
	const QStringList cases = {"verified", "wrong-observed-id", "changed-source-body", "wrong-revision", "wrong-name", "unsealed-revision",
		"intent-without-receipt", "wrong-destination", "wrong-nonce", "wrong-session", "foreign-account", "wrong-hash", "unknown-format", "newline-revision"};
	for (bool g : {false, true}) for (int s = 0; s < cases.size(); ++s)
		QTest::newRow(qPrintable(QString("%1-%2").arg(g).arg(cases[s]))) << g << s;
}
void TestCloudSyncHistory::handoffReceipt()
{
	QFETCH(bool, google); QFETCH(int, scenario);
	const auto epoch = uid(), provider = QString(google ? "google-drive" : "dropbox");
	const auto sourceId = QString(google ? "source" : "id:source"), targetId = QString(google ? "target" : "id:target");
	auto root = fencedBranch(provider, epoch); QVERIFY(root.valid());
	auto target = CloudHistoryFence::freeze(targetId, root); QVERIFY(target.intend(sourceId));
	QVERIFY(target.intend(google ? "different-id" : "id:different-id"));
	const auto intended = target;
	auto source = CloudHistoryFence::seal(sourceId, root, "before-seal", target); QVERIFY(source.valid());
	QString actualId = sourceId, version = "sealed-revision", name = CloudHistoryFence::archiveName(epoch, sourceId);
	QVERIFY(!target.acknowledge(google ? "different-id" : "id:different-id", source, version, name));
	QCOMPARE(target.json(), intended.json()); // A body cannot attest another physical identity.
	QVERIFY(target.acknowledge(actualId, source, version, name));
	if (scenario == 1) actualId = google ? "different-id" : "id:different-id";
	if (scenario == 2) { QVERIFY(root.registerUpload(uid(), root.head(), CloudSyncManifest::sha256(B), B.size())); source.value["record"] = root.value; QVERIFY(source.valid()); }
	if (scenario == 3) version += "changed";
	if (scenario == 4) name = "unverified-name.json";
	if (scenario == 5) version = "before-seal";
	if (scenario == 6) target = intended;
	if (scenario == 7) target.value["id"] = google ? "another-target" : "id:another-target";
	if (scenario == 8) source.value["nonce"] = uid();
	if (scenario == 9) target.value["session"] = uid();
	if (scenario == 10) { auto record = source.record(); record.value["account"] = CloudSyncManifest::sha256("other-account"); source.value["record"] = record.value; QVERIFY(source.valid()); }
	if (scenario == 5 || scenario == 11 || scenario == 13) {
		auto entries = target.value["sources"].toArray(); auto entry = entries.first().toObject();
		if (scenario == 11) entry["sha256"] = CloudSyncManifest::sha256("other-body");
		else { if (scenario == 13) version += '\n'; entry["version"] = version; }
		entries[0] = entry; target.value["sources"] = entries;
	}
	if (scenario == 12) source.value["format"] = "unknown-future-format";
	const auto before = target.json();
	QCOMPARE(target.acknowledges(actualId, source, version, name), scenario == 0);
	QCOMPARE(target.json(), before); // Verification never rewrites safety metadata.
	if (scenario == 0) {
		const auto restartedSource = CloudHistoryFence::parse(source.json());
		auto restartedTarget = CloudHistoryFence::parse(target.json());
		QVERIFY(restartedTarget.acknowledges(actualId, restartedSource, version, name));
		QVERIFY(restartedTarget.intend(google ? "later-source" : "id:later-source"));
		QVERIFY(restartedTarget.acknowledges(actualId, restartedSource, version, name));
	}
}
void TestCloudSyncHistory::handoffProof_data()
{
	QTest::addColumn<bool>("google"); QTest::addColumn<int>("scenario");
	const QStringList cases = {"restart-and-idempotence", "coordinator-advance", "source-advance", "reload-new-checkpoint", "source-rollback",
		"wrong-observed-source", "wrong-observed-target", "missing-target-revision", "wrong-source-revision", "wrong-source-name", "checkpoint-lock",
		"corrupt-checkpoint", "directory-is-file", "corrupt-proof", "proof-is-directory", "competing-proof", "receipt-rollback", "pristine-anchor",
		"no-local-anchor", "oversized-proof", "unknown-proof-format", "foreign-source-account", "large-proof", "changed-checkpoint-bytes", "changed-sealed-body"};
	for (bool g : {false, true}) for (int s = 0; s < cases.size(); ++s)
		QTest::newRow(qPrintable(QString("%1-%2").arg(g).arg(cases[s]))) << g << s;
}
void TestCloudSyncHistory::handoffProof()
{
	QFETCH(bool, google); QFETCH(int, scenario);
	const auto epoch = uid(), provider = QString(google ? "google-drive" : "dropbox");
	const auto sourceId = QString(google ? "source" : "id:source"), targetId = QString(google ? "target" : "id:target");
	auto baseline = fencedBranch(provider, epoch); QVERIFY(baseline.valid());
	if (scenario == 22) {
		auto tickets = baseline.value["tickets"].toObject(); auto t = baseline.ticket(baseline.head());
		t["state"] = "uploaded"; t["parent"] = baseline.head();
		for (int i = 0; i < 505; ++i) {
			const auto id = uid(); t["name"] = CloudHistoryRecord::payloadName(epoch, id);
			t["object"] = QString::number(i) + QString(490, 'x'); t["version"] = QString(512, 'v'); tickets[id] = t;
		}
		baseline.value["tickets"] = tickets; QVERIFY(baseline.valid());
	}
	auto advanced = baseline;
	QVERIFY(advanced.registerUpload(uid(), advanced.head(), CloudSyncManifest::sha256(B), B.size()));
	QVERIFY(advanced.upgradeLegacyProvenance());
	const auto sourceRoot = scenario == 2 || scenario == 3 ? advanced : baseline;
	auto target = CloudHistoryFence::freeze(targetId, baseline); QVERIFY(target.intend(sourceId)); const auto intent = target;
	auto source = CloudHistoryFence::seal(sourceId, sourceRoot, "before-seal", target); QVERIFY(source.valid());
	const auto archived = CloudHistoryFence::archiveName(epoch, sourceId);
	QVERIFY(target.acknowledge(sourceId, source, "sealed-revision", archived));
	QTemporaryDir temporary; QVERIFY(temporary.isValid()); const auto path = temporary.filePath("authority.json");
	CloudHistoryAuthority authority(path);
	const auto anchor = scenario == 17 ? CloudHistoryRecord::create(provider, Account, epoch, baseline.value["legacy_sha256"].toString()) : baseline;
	if (scenario != 18) QVERIFY(authority.observe(sourceId, anchor));
	if (scenario == 3 || scenario == 4) { CloudHistoryAuthority other(path); QVERIFY(other.observe(sourceId, advanced)); }
	const auto readBytes = [](const QString &name) { QFile file(name); return QFileInfo(name).isFile() && file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray(); };
	const auto writeBytes = [](const QString &name, const QByteArray &bytes) { QFile file(name); return file.open(QIODevice::WriteOnly) && file.write(bytes) == bytes.size(); };
	const auto directory = path + ".handoffs", expectedPath = directory + "/" + CloudSyncManifest::sha256(sourceId.toUtf8()) + ".json";
	std::unique_ptr<QLockFile> held;
	if (scenario == 10) { held = std::make_unique<QLockFile>(path + ".lock"); QVERIFY(held->tryLock(0)); }
	if (scenario == 11) QVERIFY(writeBytes(path, "invalid checkpoint"));
	if (scenario == 12) QVERIFY(writeBytes(directory, "unrelated directory blocker"));
	if (scenario == 13 || scenario == 14 || scenario == 19) {
		QVERIFY(QDir().mkpath(directory));
		if (scenario == 13) QVERIFY(writeBytes(expectedPath, "unrelated proof blocker"));
		if (scenario == 14) QVERIFY(QDir().mkpath(expectedPath));
		if (scenario == 19) QVERIFY(writeBytes(expectedPath, QByteArray(4 * 1024 * 1024 + 4097, 'x')));
	}
	const bool priorReceipt = scenario == 1 || scenario == 15 || scenario == 16 || scenario == 20 || scenario == 23;
	if (priorReceipt) QCOMPARE(authority.preserveHandoff(sourceId, source, "sealed-revision", archived, targetId, target, "target-revision"), expectedPath);
	if (scenario == 1) QVERIFY(target.intend(google ? "later-source" : "id:later-source"));
	if (scenario == 15) {
		target = CloudHistoryFence::freeze(targetId, baseline); QVERIFY(target.intend(sourceId));
		source = CloudHistoryFence::seal(sourceId, baseline, "another-before-seal", target);
		QVERIFY(target.acknowledge(sourceId, source, "sealed-revision", archived));
	}
	if (scenario == 16) target = intent;
	if (scenario == 20) {
		auto unsupported = QJsonDocument::fromJson(readBytes(expectedPath)).object(); unsupported["format"] = "unknown-future-proof";
		QVERIFY(writeBytes(expectedPath, QJsonDocument(unsupported).toJson(QJsonDocument::Compact)));
	}
	if (scenario == 21) { auto foreign = source.record(); foreign.value["account"] = CloudSyncManifest::sha256("other-account"); source.value["record"] = foreign.value; QVERIFY(source.valid()); }
	if (scenario == 23) QVERIFY(writeBytes(path, readBytes(path) + "\n"));
	if (scenario == 24) { source.value["record"] = advanced.value; QVERIFY(source.valid()); }
	const auto checkpointBytes = readBytes(path), proofBefore = readBytes(expectedPath), blockerBefore = scenario == 12 ? readBytes(directory) : QByteArray();
	const auto observedSource = scenario == 5 ? (google ? QString("another-source") : QString("id:another-source")) : sourceId;
	const auto observedTarget = scenario == 6 ? (google ? QString("another-target") : QString("id:another-target")) : targetId;
	const auto result = authority.preserveHandoff(observedSource, source, scenario == 8 ? "wrong-revision" : "sealed-revision",
		scenario == 9 ? "wrong-name.json" : archived, observedTarget, target, scenario == 7 ? QString() : "new-target-revision");
	const bool accepted = scenario <= 3 || scenario == 17 || scenario == 22;
	QCOMPARE(!result.isEmpty(), accepted);
	QCOMPARE(readBytes(path), checkpointBytes); // Never replace/normalize the active checkpoint.
	if (scenario == 12) QCOMPARE(readBytes(directory), blockerBefore);
	if (!proofBefore.isEmpty()) QCOMPARE(readBytes(expectedPath), proofBefore); // Existing receipts/blockers are immutable.
	if (!accepted) {
		if (proofBefore.isEmpty() && scenario != 14) QVERIFY(!QFileInfo::exists(expectedPath));
		return;
	}
	QCOMPARE(result, expectedPath);
	const auto proofBytes = readBytes(result); const auto proof = QJsonDocument::fromJson(proofBytes).object();
	QCOMPARE(proof["format"].toString(), QString("neo-history-handoff-proof-development-1"));
	QCOMPARE(QByteArray::fromBase64(proof["checkpoint_base64"].toString().toLatin1()), checkpointBytes);
	QCOMPARE(proof["checkpoint_sha256"].toString(), CloudSyncManifest::sha256(checkpointBytes));
	QCOMPARE(proof["source"].toObject(), source.value); QCOMPARE(proof["source_id"].toString(), sourceId);
	if (scenario == 22) QVERIFY(proofBytes.size() > 1024 * 1024);
	CloudHistoryAuthority restarted(path); QVERIFY(restarted.load()); QCOMPARE(restarted.id(), sourceId);
	QVERIFY(!restarted.rebindPristine(sourceId, source.record(), targetId, target.record()));
	QCOMPARE(readBytes(path), checkpointBytes);
	QCOMPARE(restarted.preserveHandoff(sourceId, source, "sealed-revision", archived, targetId, target, "another-target-revision"), expectedPath);
	QCOMPARE(readBytes(expectedPath), proofBytes);
}

void TestCloudSyncHistory::archiveTransport_data()
{
	QTest::addColumn<int>("scenario");
	const QStringList cases = {"rename", "legacy-name", "bad-epoch", "empty-token", "missing-id", "unsafe-id", "empty-revision", "newline-revision", "long-revision", "stale-revision", "lost-ack", "bad-ack", "missing-object"};
	for (int i = 0; i < cases.size(); ++i) QTest::newRow(qPrintable(cases[i])) << i;
}
void TestCloudSyncHistory::archiveTransport()
{
	QFETCH(int, scenario); Network n; n.seed(); const auto epoch = uid(), name = CloudHistoryRecord::rootName(epoch);
	n.archiveEpoch = epoch; n.add(name, A); const auto before = n.files;
	QString token = "token", id = n.files[2].id.mid(3), version = n.files[2].rev, expectedName = name;
	if (scenario == 1) expectedName = Legacy;
	if (scenario == 2) expectedName = "subsurface-neo-history-invalid.json";
	if (scenario == 3) token.clear();
	if (scenario == 4) id.clear();
	if (scenario == 5) id = "unsafe/id";
	if (scenario == 6) version.clear();
	if (scenario == 7) version += '\n';
	if (scenario == 8) version = QString(513, 'v');
	if (scenario == 9) version = "stale";
	if (scenario == 10) n.loseArchiveAck = true;
	if (scenario == 11) n.response = [](const QNetworkRequest &, const QByteArray &) { return QByteArray("{}"); };
	if (scenario == 12) id = "missing-object";
	CloudSyncFileStore files(&n); QSignalSpy done(&files, &CloudSyncFileStore::historyRootArchived), errors(&files, &CloudSyncFileStore::operationError), conflicts(&files, &CloudSyncFileStore::writeConflict);
	files.archiveGoogleHistory(token, expectedName, id, version);
	QTRY_COMPARE(done.size() + errors.size() + conflicts.size(), 1);
	const bool validRequest = scenario == 0 || scenario >= 9;
	QCOMPARE(n.renames, int(validRequest)); QCOMPARE(n.mutations, int(validRequest));
	QCOMPARE(done.size(), int(scenario == 0));
	for (int i = 0; i < before.size(); ++i) {
		QCOMPARE(n.files[i].id, before[i].id); QCOMPARE(n.files[i].data, before[i].data);
		if (i != 2 || (scenario != 0 && scenario != 10 && scenario != 11)) {
			QCOMPARE(n.files[i].name, before[i].name); QCOMPARE(n.files[i].rev, before[i].rev);
		}
	}
	if (scenario == 0 || scenario == 10 || scenario == 11) QCOMPARE(n.files[2].name, CloudHistoryFence::archiveName(epoch, id));
	QVERIFY(n.allowed);
}
void TestCloudSyncHistory::archiveWorkflow_data()
{
	QTest::addColumn<int>("scenario");
	const QStringList cases = {"archive", "already-archived", "lost-rename-response", "stale-source-body", "stale-source-revision",
		"source-cas-race", "concurrent-archiver", "target-before-lost-intent", "target-after-lost-intent", "target-advances", "bad-source-scope",
		"bad-source-identity", "bad-target-identity", "scope-changes", "cancel-read", "cancel-committed-rename", "throttle-retry", "server-error-retry",
		"rejected-token", "bad-ack-recovery", "missing-source", "source-name-replaced", "missing-target", "unknown-source", "unknown-target",
		"changed-archived-body", "acknowledged-source", "mismatched-receipt", "unknown-target-after", "bad-account"};
	for (int i = 0; i < cases.size(); ++i) QTest::newRow(qPrintable(cases[i])) << i;
}
void TestCloudSyncHistory::archiveWorkflow()
{
	QFETCH(int, scenario); Network n; n.seed(); const auto legacy = n.files;
	const auto epoch = uid(), name = CloudHistoryRecord::rootName(epoch);
	n.archiveEpoch = epoch;
	const auto root = fencedBranch("google-drive", epoch); QVERIFY(root.valid());
	n.add(name, root.json()); n.add(name, root.json());
	const auto targetId = n.files[2].id.mid(3), sourceId = n.files[3].id.mid(3), archived = CloudHistoryFence::archiveName(epoch, sourceId);
	auto target = CloudHistoryFence::freeze(targetId, root); QVERIFY(target.intend(sourceId));
	auto source = CloudHistoryFence::seal(sourceId, root, "before-seal", target); QVERIFY(source.valid());
	n.files[2].data = target.json(); n.files[3].data = source.json(); const auto sealedVersion = n.files[3].rev;
	if (scenario == 1 || scenario == 26 || scenario == 27) { n.files[3].name = archived; n.files[3].rev += "-archived"; }
	if (scenario == 26 || scenario == 27) {
		QVERIFY(target.acknowledge(sourceId, source, n.files[3].rev, archived));
		if (scenario == 27) { auto entries = target.value["sources"].toArray(); auto entry = entries.first().toObject(); entry["version"] = "wrong-version"; entries[0] = entry; target.value["sources"] = entries; }
		n.files[2].data = target.json();
	}
	if (scenario == 2) n.loseArchiveAck = true;
	if (scenario == 3) { auto changed = root; QVERIFY(changed.registerUpload(uid(), changed.head(), CloudSyncManifest::sha256(B), B.size())); auto sealed = source; sealed.value["record"] = changed.value; n.files[3].data = sealed.json(); n.files[3].rev += "-changed"; }
	if (scenario == 4) n.files[3].rev += "-metadata-change";
	if (scenario == 7) { auto changed = target; changed.value["sources"] = QJsonArray(); changed.value["sequence"] = 2; n.files[2].data = changed.json(); }
	if (scenario == 10) { auto changed = source.record(); changed.value["account"] = CloudSyncManifest::sha256("other"); source.value["record"] = changed.value; }
	if (scenario == 14) n.deferReads = true;
	if (scenario == 15) n.deferArchive = true;
	if (scenario == 16 || scenario == 17 || scenario == 18) n.archiveFailureStatus = scenario == 16 ? 429 : scenario == 17 ? 503 : 401;
	if (scenario == 19) n.response = [](const QNetworkRequest &request, const QByteArray &bytes) { return request.attribute(QNetworkRequest::CustomVerbAttribute).toByteArray() == "PATCH" ? QByteArray("{}") : bytes; };
	if (scenario == 21) { n.files[3].name = "preserved-elsewhere.json"; n.add(name, source.json()); }
	if (scenario == 23) source.value["unknown_safety_field"] = true;
	if (scenario == 24) { auto future = target; future.value["format"] = "unknown-future"; n.files[2].data = future.json(); }
	if (scenario == 20) n.files.removeAt(3);
	if (scenario == 22) n.files.removeAt(2);
	int targetMetadataReads = 0, nameLookups = 0;
	n.requested = [&](const QNetworkRequest &request) {
		const auto path = request.url().path();
		if (path == "/drive/v3/files") ++nameLookups;
		if (path == "/drive/v2/files/" + targetId && request.attribute(QNetworkRequest::CustomVerbAttribute).toByteArray().isEmpty() && ++targetMetadataReads == 3) {
			if (scenario == 8 || scenario == 9 || scenario == 28) {
				auto changed = target;
				if (scenario == 9) QVERIFY(changed.intend("another-source"));
				else if (scenario == 8) { changed.value["sources"] = QJsonArray(); changed.value["sequence"] = 2; }
				else changed.value["format"] = "unknown-future";
				n.files[2].data = changed.json(); n.files[2].rev += "-target-advanced";
			}
		}
		if (path == "/drive/v2/files/" + sourceId && request.attribute(QNetworkRequest::CustomVerbAttribute).toByteArray() == "PATCH") {
			if (scenario == 5) { auto changed = root; QVERIFY(changed.registerUpload(uid(), changed.head(), CloudSyncManifest::sha256(B), B.size())); auto newer = source; newer.value["record"] = changed.value; n.files[3].data = newer.json(); n.files[3].rev += "-source-advanced"; }
			if (scenario == 6) { n.files[3].name = archived; n.files[3].rev += "-another-archiver"; }
		}
		if (scenario == 13 && path == "/drive/v3/files/" + sourceId) n.guard = false;
		if (scenario == 25 && n.renames && path == "/drive/v2/files/" + sourceId) { auto unknown = source; unknown.value["unknown_safety_field"] = true; n.files[3].data = unknown.json(); n.files[3].rev += "-changed-after-rename"; }
	};
	const auto originalSource = scenario == 20 ? QByteArray() : n.files[scenario == 22 ? 2 : 3].data;
	const auto allBefore = n.files;
	CloudHistoryArchive client(&n); QSignalSpy done(&client, &CloudHistoryArchive::finished), errors(&client, &CloudHistoryArchive::failed);
	const auto observedSource = scenario == 11 ? QString("wrong-source") : sourceId, observedTarget = scenario == 12 ? QString("wrong-target") : targetId;
	const auto start = [&] { client.start("token", scenario == 29 ? CloudSyncManifest::sha256("wrong-account") : Account, observedSource, source, sealedVersion, observedTarget, target, [&] { return n.guard; }); };
	start();
	if (scenario == 14 || scenario == 15) {
		QTRY_VERIFY(n.deferred); client.cancel(); if (n.deferred) n.deferred->complete(); QTest::qWait(20);
		QCOMPARE(done.size(), 0); QCOMPARE(errors.size(), 0);
		if (scenario == 15) { n.deferArchive = false; start(); QTRY_COMPARE(done.size(), 1); }
	} else {
		const bool success = scenario == 0 || scenario == 1 || scenario == 2 || scenario == 6 || scenario == 9 || scenario == 19 || scenario == 26;
		QTRY_COMPARE(done.size() + errors.size(), 1); QCOMPARE(done.size(), int(success));
		if (scenario == 18) QVERIFY(errors.first()[1].toBool());
		if (scenario == 16 || scenario == 17) { n.archiveFailureStatus = 0; start(); QTRY_COMPARE(done.size(), 1); QCOMPARE(errors.size(), 1); }
	}
	QCOMPARE(nameLookups, 0); QCOMPARE(n.mutations, n.renames);
	const QList<int> attempted = {0, 2, 5, 6, 8, 9, 15, 16, 17, 18, 19, 25, 28};
	QCOMPARE(n.renames, scenario == 16 || scenario == 17 ? 2 : int(attempted.contains(scenario)));
	QCOMPARE(n.files.size(), allBefore.size());
	if (scenario != 20) {
		const auto &file = n.files[scenario == 22 ? 2 : 3]; QCOMPARE(file.id.mid(3), sourceId);
		if (scenario != 5 && scenario != 25) QCOMPARE(file.data, originalSource);
	}
	if (!done.isEmpty()) {
		QCOMPARE(n.files[3].name, archived); QCOMPARE(done.last()[2].toString(), n.files[3].rev);
		QCOMPARE(done.last()[0].toJsonObject(), QJsonDocument::fromJson(n.files[2].data).object()); QCOMPARE(done.last()[1].toString(), n.files[2].rev);
	}
	for (int i = 0; i < legacy.size(); ++i) { QCOMPARE(n.files[i].id, legacy[i].id); QCOMPARE(n.files[i].rev, legacy[i].rev); QCOMPARE(n.files[i].data, legacy[i].data); }
	if (scenario == 21) { QCOMPARE(n.files.last().id, allBefore.last().id); QCOMPARE(n.files.last().rev, allBefore.last().rev); QCOMPARE(n.files.last().data, allBefore.last().data); }
	if (scenario != 22 && scenario != 8 && scenario != 9 && scenario != 28) { QCOMPARE(n.files[2].data, allBefore[2].data); QCOMPARE(n.files[2].rev, allBefore[2].rev); }
	QVERIFY(n.allowed);
}

void TestCloudSyncHistory::workspaceReference_data()
{
	QTest::addColumn<bool>("google"); QTest::addColumn<int>("scenario");
	for (bool g : {false, true}) for (int s = 0; s < 10; ++s) QTest::newRow(qPrintable(QString("%1-reference%2").arg(g).arg(s))) << g << s;
}
void TestCloudSyncHistory::workspaceReference()
{
	QFETCH(bool, google); QFETCH(int, scenario);
	const auto provider = QString(google ? "google-drive" : "dropbox"), id = QString(google ? "root-a" : "id:root-a"), epoch = uid(), ticket = uid();
	auto reference = CloudWorkspaceReference::create(provider, Account, epoch, id); QVERIFY(reference.valid());
	const auto original = reference;
	if (scenario == 1) reference.value["account"] = "not-a-hash";
	if (scenario == 2) reference.value["root_id"] = "unsafe/id";
	if (scenario == 3) reference.value["epoch"] = "not-an-epoch";
	if (scenario == 4) reference.value["provider"] = "subsurface-cloud";
	if (scenario == 5) reference.value["format"] = "unknown-future-reference";
	if (scenario == 6) reference.value["unknown_safety_field"] = true;
	if (scenario == 7) reference.value["root_id"] = id + QString(513, 'x');
	if (scenario == 8) reference.value["schema"] = 2;
	if (scenario == 9) { QVERIFY(!CloudWorkspaceReference::parse(reference.json() + QByteArray(2049, ' ')).valid()); return; }
	if (scenario) {
		QVERIFY(!reference.valid()); QVERIFY(!CloudWorkspaceReference::parse(reference.json()).valid());
		QVERIFY(reference.key().isEmpty()); QVERIFY(reference.rootName().isEmpty()); QVERIFY(reference.payloadName(ticket).isEmpty()); return;
	}
	QCOMPARE(CloudWorkspaceReference::parse(reference.json()).key(), reference.key()); QVERIFY(reference.matches(provider, Account));
	QVERIFY(!reference.matches(provider, CloudSyncManifest::sha256("other-account"))); QVERIFY(reference.payloadName("invalid").isEmpty());
	const auto second = CloudWorkspaceReference::create(provider, Account, epoch, google ? "root-b" : "id:root-b"); QVERIFY(second.valid());
	QCOMPARE(second.rootName(), reference.rootName()); // Equal names are not equal workspaces.
	QVERIFY(second.key() != reference.key()); QVERIFY(second.payloadName(ticket) != reference.payloadName(ticket));
	QVERIFY(reference.payloadName(ticket).contains(reference.key())); QVERIFY(reference.payloadName(ticket).size() < 255);
	QVERIFY(!reference.rootName().startsWith("subsurface-neo-history-")); QCOMPARE(reference.json(), original.json());
}
void TestCloudSyncHistory::workspaceSelection_data()
{
	QTest::addColumn<bool>("google"); QTest::addColumn<int>("scenario");
	const QStringList cases = {"switch-and-restart", "stale-process", "foreign-account", "corrupt-file", "future-format", "lock", "blocked-parent", "oversized-file", "tampered-reference", "retain-bound", "new-parent", "invalid-reference", "away-and-back-race", "old-format", "bad-choice-version", "bad-expected-version"};
	for (bool g : {false, true}) for (int s = 0; s < cases.size(); ++s) QTest::newRow(qPrintable(QString("%1-%2").arg(g).arg(cases[s]))) << g << s;
}
void TestCloudSyncHistory::workspaceSelection()
{
	QFETCH(bool, google); QFETCH(int, scenario);
	const auto provider = QString(google ? "google-drive" : "dropbox"), epoch = uid();
	const auto first = CloudWorkspaceReference::create(provider, Account, epoch, google ? "root-a" : "id:root-a");
	auto second = CloudWorkspaceReference::create(provider, Account, epoch, google ? "root-b" : "id:root-b");
	const auto third = CloudWorkspaceReference::create(provider, Account, uid(), google ? "root-c" : "id:root-c");
	QTemporaryDir temporary; QVERIFY(temporary.isValid());
	const auto path = temporary.filePath(scenario == 10 ? "new/parent/choice.json" : "choice.json");
	const auto read = [](const QString &name) { QFile f(name); return f.open(QIODevice::ReadOnly) ? f.readAll() : QByteArray(); };
	const auto write = [](const QString &name, const QByteArray &bytes) { QFile f(name); return f.open(QIODevice::WriteOnly) && f.write(bytes) == bytes.size(); };
	const auto oldCheckpoint = temporary.filePath("old-history-checkpoint.json"); QVERIFY(write(oldCheckpoint, "preserve old checkpoint"));
	CloudWorkspaceSelection selection(path, provider, Account); QVERIFY(selection.load()); QVERIFY(!selection.selected().valid());
	if (scenario == 10) QVERIFY(!QFileInfo::exists(QFileInfo(path).absolutePath()));
	QVERIFY(selection.select({}, first)); const auto originalBytes = read(path); const auto firstVersion = selection.version(); QVERIFY(CloudHistoryRecord::validEpoch(firstVersion));
	if (scenario == 0 || scenario == 10) {
		QVERIFY(selection.select({}, first)); QCOMPARE(read(path), originalBytes); // Lost local acknowledgement, no replay.
		QVERIFY(selection.select(selection.version(), second)); QVERIFY(selection.select(selection.version(), first)); QVERIFY(selection.version() != firstVersion);
		CloudWorkspaceSelection restarted(path, provider, Account); QVERIFY(restarted.load()); QCOMPARE(restarted.selected().key(), first.key());
		QCOMPARE(restarted.version(), selection.version());
		QCOMPARE(restarted.knownReferences().size(), 2); QCOMPARE(restarted.knownReferences()[second.key()].toObject(), second.value);
	} else if (scenario == 1) {
		CloudWorkspaceSelection other(path, provider, Account); QVERIFY(other.load()); QVERIFY(other.select(other.version(), second)); const auto changed = read(path);
		QVERIFY(!selection.select(firstVersion, third)); QCOMPARE(selection.selected().key(), second.key()); QCOMPARE(read(path), changed);
		QVERIFY(selection.select(selection.version(), third)); QCOMPARE(selection.knownReferences().size(), 3);
	} else if (scenario == 6) {
		const auto blocker = temporary.filePath("blocked"); QVERIFY(write(blocker, "unrelated blocker"));
		CloudWorkspaceSelection blocked(blocker + "/choice.json", provider, Account); QVERIFY(!blocked.select({}, second)); QCOMPARE(read(blocker), QByteArray("unrelated blocker"));
	} else if (scenario == 9) {
		for (int i = 1; i < 32; ++i) {
			const auto next = CloudWorkspaceReference::create(provider, Account, uid(), google ? "root-" + QString::number(i) : "id:root-" + QString::number(i));
			QVERIFY(selection.select(selection.version(), next));
		}
		const auto full = read(path); QVERIFY(!selection.select(selection.version(), second)); QCOMPARE(read(path), full);
		QVERIFY(selection.select(selection.version(), first)); QCOMPARE(selection.knownReferences().size(), 32);
		QCOMPARE(selection.knownReferences()[first.key()].toObject(), first.value);
	} else if (scenario == 12) {
		CloudWorkspaceSelection other(path, provider, Account); QVERIFY(other.load()); QVERIFY(other.select(other.version(), second)); QVERIFY(other.select(other.version(), first));
		const auto changed = read(path); QCOMPARE(other.selected().key(), first.key()); QVERIFY(other.version() != firstVersion);
		QVERIFY(!selection.select(firstVersion, third)); QCOMPARE(read(path), changed); QCOMPARE(selection.version(), other.version());
		QVERIFY(selection.select(firstVersion, first)); QCOMPARE(read(path), changed); // Same current choice needs no write.
	} else {
		std::unique_ptr<QLockFile> held;
		if (scenario == 2) second.value["account"] = CloudSyncManifest::sha256("other-account");
		if (scenario == 3) QVERIFY(write(path, "invalid choice"));
		if (scenario == 4 || scenario == 8) {
			auto value = QJsonDocument::fromJson(read(path)).object();
			if (scenario == 4) value["format"] = "unknown-future-choice";
			else { auto refs = value["references"].toObject(); auto changed = first; changed.value["root_id"] = google ? "tampered-root" : "id:tampered-root"; refs[first.key()] = changed.value; value["references"] = refs; }
			QVERIFY(write(path, QJsonDocument(value).toJson(QJsonDocument::Compact)));
		}
		if (scenario == 5) { held = std::make_unique<QLockFile>(path + ".lock"); QVERIFY(held->tryLock(0)); }
		if (scenario == 7) QVERIFY(write(path, QByteArray(64 * 1024 + 1, ' ')));
		if (scenario == 11) second.value["unknown_field"] = true;
		if (scenario == 13 || scenario == 14) {
			auto value = QJsonDocument::fromJson(read(path)).object();
			if (scenario == 13) { value["format"] = "neo-workspace-selection-development-1"; value.remove("version"); }
			else value["version"] = "invalid-choice-version";
			QVERIFY(write(path, QJsonDocument(value).toJson(QJsonDocument::Compact)));
		}
		const auto before = read(path); QVERIFY(!selection.select(scenario == 15 ? QString("invalid-expected-version") : firstVersion, second)); QCOMPARE(read(path), before);
	}
	QCOMPARE(read(oldCheckpoint), QByteArray("preserve old checkpoint"));
}

void TestCloudSyncHistory::workspaceRecord_data()
{
	QTest::addColumn<bool>("google"); QTest::addColumn<int>("scenario");
	const QStringList cases = {"transitions", "future-envelope", "unknown-field", "foreign-account", "foreign-epoch", "bad-schema", "tickets-array",
		"legacy-payload-name", "other-root-payload-name", "unsafe-object", "root-as-payload", "unsafe-revision", "rollback", "rebound-root",
		"foreign-provider", "unknown-ticket-field", "oversized-record", "distinct-pristine-roots", "lost-pending-ticket", "advanced-sequence", "future-journal", "bad-reference"};
	for (bool g : {false, true}) for (int s = 0; s < cases.size(); ++s) QTest::newRow(qPrintable(QString("%1-%2").arg(g).arg(cases[s]))) << g << s;
}
void TestCloudSyncHistory::workspaceRecord()
{
	QFETCH(bool, google); QFETCH(int, scenario);
	const auto provider = QString(google ? "google-drive" : "dropbox"), epoch = uid(), first = uid(), pending = uid(), reservation = uid();
	const auto ref = CloudWorkspaceReference::create(provider, Account, epoch, google ? "root-a" : "id:root-a");
	auto root = CloudWorkspaceRecord::create(ref, CloudSyncManifest::sha256("legacy")); QVERIFY(root.valid()); const auto pristine = root;
	QVERIFY(root.registerUpload(first, {}, CloudSyncManifest::sha256(A), A.size()));
	QVERIFY(root.completeUpload(first, google ? "payload-a" : "id:payload-a", "payload-revision")); QVERIFY(root.publish(first, {}));
	const auto committed = root;
	QVERIFY(root.registerUpload(pending, first, CloudSyncManifest::sha256(B), B.size())); QVERIFY(root.reserve(reservation, first));
	const auto original = root;
	const auto changeJournal = [&](const char *field, const QJsonValue &value) { auto j = root.value["journal"].toObject(); j[QLatin1String(field)] = value; root.value["journal"] = j; };
	const auto changeTicket = [&](const char *field, const QJsonValue &value) {
		auto j = root.value["journal"].toObject(); auto tickets = j["tickets"].toObject(); auto t = tickets[first].toObject();
		t[QLatin1String(field)] = value; tickets[first] = t; j["tickets"] = tickets; root.value["journal"] = j;
	};
	if (scenario == 0) {
		QVERIFY(root.follows(pristine)); QVERIFY(root.follows(committed));
		QCOMPARE(root.ticket(first)["name"].toString(), ref.payloadName(first));
		const auto restored = uid(); QVERIFY(root.registerUpload(restored, first, CloudSyncManifest::sha256(A), A.size(), first, reservation));
		QVERIFY(!root.release(reservation)); // An in-flight restore still requires its reservation.
		QVERIFY(root.completeUpload(restored, google ? "restored-a" : "id:restored-a", "restore-revision")); QVERIFY(root.publish(restored, first));
		QVERIFY(root.release(reservation)); QVERIFY(!root.publish(pending, first));
		QCOMPARE(root.ticket(pending), original.ticket(pending)); QVERIFY(root.follows(original));
		QCOMPARE(CloudWorkspaceRecord::parse(root.json()).json(), root.json());
		QVERIFY(!CloudHistoryRecord::parse(root.json()).valid()); // Old history cannot write this format.
		const auto before = root.json(); QVERIFY(!root.registerUpload(uid(), {}, CloudSyncManifest::sha256(B), B.size())); QCOMPARE(root.json(), before); return;
	}
	if (scenario == 12 || scenario == 17 || scenario == 18 || scenario == 19) {
		if (scenario == 12) root = committed;
		if (scenario == 17) root = CloudWorkspaceRecord::create(CloudWorkspaceReference::create(provider, Account, epoch, google ? "root-b" : "id:root-b"), CloudSyncManifest::sha256("legacy"));
		if (scenario == 18) { auto tickets = root.value["journal"].toObject()["tickets"].toObject(); tickets.remove(pending); changeJournal("tickets", tickets); }
		if (scenario == 19) changeJournal("sequence", root.value["journal"].toObject()["sequence"].toInt() + 1);
		QVERIFY(root.valid());
		if (scenario == 19) { QVERIFY(root.follows(original)); QVERIFY(!original.follows(root)); }
		else QVERIFY(!root.follows(original));
		if (scenario == 17) QVERIFY(!root.follows(pristine)); return;
	}
	if (scenario == 1) root.value["format"] = "unknown-future-workspace";
	if (scenario == 2) root.value["unknown_safety_field"] = true;
	if (scenario == 3) changeJournal("account", CloudSyncManifest::sha256("other-account"));
	if (scenario == 4) changeJournal("epoch", uid());
	if (scenario == 5) root.value["schema"] = 3;
	if (scenario == 6) changeJournal("tickets", QJsonArray());
	if (scenario == 7) changeTicket("name", CloudHistoryRecord::payloadName(epoch, first));
	if (scenario == 8) changeTicket("name", CloudWorkspaceReference::create(provider, Account, epoch, google ? "root-b" : "id:root-b").payloadName(first));
	if (scenario == 9) changeTicket("object", "unsafe/id");
	if (scenario == 10) changeTicket("object", ref.value["root_id"]);
	if (scenario == 11) changeTicket("version", "unsafe\nrevision");
	if (scenario == 13) { auto changed = ref.value; changed["root_id"] = google ? "root-b" : "id:root-b"; root.value["workspace"] = changed; }
	if (scenario == 14) changeJournal("provider", google ? "dropbox" : "google-drive");
	if (scenario == 15) changeTicket("unknown_safety_field", true);
	if (scenario == 16) {
		QVERIFY(!CloudWorkspaceRecord::parse(root.json() + QByteArray(1024 * 1024 + 3073, ' ')).valid()); return;
	}
	if (scenario == 20) changeJournal("format", "unknown-future-journal");
	if (scenario == 21) { auto changed = ref.value; changed["unknown_safety_field"] = true; root.value["workspace"] = changed; }
	QVERIFY(!root.valid()); QVERIFY(!CloudWorkspaceRecord::parse(root.json()).valid()); QVERIFY(!root.follows(original));
	const auto invalid = root.json(); QVERIFY(!root.registerUpload(uid(), first, CloudSyncManifest::sha256(B), B.size())); QCOMPARE(root.json(), invalid);
}
void TestCloudSyncHistory::workspaceOpen_data()
{
	QTest::addColumn<bool>("google"); QTest::addColumn<int>("scenario");
	const QStringList cases = {"verified", "foreign-account", "foreign-provider", "wrong-self-id", "wrong-epoch", "missing-root", "former-name-replaced", "future-format",
		"server-error", "token-rejected", "cancel", "scope-changes", "guard-refused", "rollback", "foreign-checkpoint", "invalid-checkpoint", "pending-advance",
		"wrong-metadata-id", "missing-revision", "same-name-root", "malformed-json", "oversized-root", "new-device", "empty-token", "missing-scope-guard"};
	for (bool g : {false, true}) for (int s = 0; s < cases.size(); ++s) QTest::newRow(qPrintable(QString("%1-open-%2").arg(g).arg(cases[s]))) << g << s;
}
void TestCloudSyncHistory::workspaceOpen()
{
	QFETCH(bool, google); QFETCH(int, scenario); Network n; n.seed();
	const auto provider = QString(google ? "google-drive" : "dropbox"), epoch = uid(), first = uid();
	n.add(CloudWorkspaceReference::create(provider, Account, epoch, google ? "temporary" : "id:temporary").rootName(), {});
	const auto id = google ? n.files[2].id.mid(3) : n.files[2].id;
	const auto ref = CloudWorkspaceReference::create(provider, Account, epoch, id);
	auto root = CloudWorkspaceRecord::create(ref, CloudSyncManifest::sha256("legacy"));
	QVERIFY(root.registerUpload(first, {}, CloudSyncManifest::sha256(A), A.size()));
	QVERIFY(root.completeUpload(first, google ? "payload-a" : "id:payload-a", "v1")); QVERIFY(root.publish(first, {}));
	auto checkpoint = root;
	if (scenario == 3 || scenario == 4) root = CloudWorkspaceRecord::create(CloudWorkspaceReference::create(provider, Account, scenario == 4 ? uid() : epoch,
		scenario == 3 ? (google ? "wrong-root" : "id:wrong-root") : id), CloudSyncManifest::sha256("legacy"));
	if (scenario == 7) root.value["format"] = "unknown-future-root";
	if (scenario == 13) QVERIFY(checkpoint.registerUpload(uid(), first, CloudSyncManifest::sha256(B), B.size()));
	if (scenario == 14) checkpoint = CloudWorkspaceRecord::create(CloudWorkspaceReference::create(provider, Account, epoch, google ? "other-root" : "id:other-root"), CloudSyncManifest::sha256("legacy"));
	if (scenario == 15) checkpoint.value["unknown_safety_field"] = true;
	if (scenario == 16) QVERIFY(root.registerUpload(uid(), first, CloudSyncManifest::sha256(B), B.size()));
	n.files[2].data = root.json();
	if (scenario == 5) n.files.removeAt(2);
	if (scenario == 6) { n.files[2].name = "preserved-elsewhere.json"; n.add(ref.rootName(), root.json()); }
	if (scenario == 8 || scenario == 9) n.readFailureStatus = scenario == 8 ? 503 : 401;
	if (scenario == 10) n.deferReads = true;
	if (scenario == 12) n.guard = false;
	if (scenario == 17 || scenario == 18) n.metadataResponse = [&](const QByteArray &bytes) {
		auto m = QJsonDocument::fromJson(bytes).object(); m[scenario == 17 ? "id" : (google ? "etag" : "rev")] = scenario == 17 ? "wrong-id" : ""; return n.json(m);
	};
	if (scenario == 19) n.add(ref.rootName(), CloudWorkspaceRecord::create(CloudWorkspaceReference::create(provider, Account, epoch, google ? "other-root" : "id:other-root"), CloudSyncManifest::sha256("legacy")).json());
	if (scenario == 20) n.files[2].data = "invalid-json";
	if (scenario == 21) n.files[2].data = QByteArray(1024 * 1024 + 3073, ' ');
	int requests = 0, lookups = 0;
	n.requested = [&](const QNetworkRequest &request) {
		++requests; if (request.url().path() == "/drive/v3/files") ++lookups;
		if (scenario == 11) n.guard = false;
	};
	const auto before = n.files;
	CloudWorkspaceOpen client(&n); QSignalSpy done(&client, &CloudWorkspaceOpen::finished), errors(&client, &CloudWorkspaceOpen::failed);
	if (scenario == 22) checkpoint.value = {};
	const std::function<bool()> guard = scenario == 24 ? std::function<bool()>() : [&] { return n.guard; };
	client.start(scenario == 23 ? "" : "token", scenario == 2 ? (google ? "dropbox" : "google-drive") : provider,
		scenario == 1 ? CloudSyncManifest::sha256("other-account") : Account, ref, guard, checkpoint);
	if (scenario == 10) {
		QTRY_VERIFY(n.deferred); client.cancel(); if (n.deferred) n.deferred->complete(); QTest::qWait(20); QCOMPARE(done.size() + errors.size(), 0);
	} else {
		QTRY_COMPARE(done.size() + errors.size(), 1); const bool success = scenario == 0 || scenario == 16 || scenario == 19 || scenario == 22;
		QCOMPARE(done.size(), int(success));
		if (success) { QCOMPARE(done.first()[0].toJsonObject(), root.value); QCOMPARE(done.first()[1].toString(), n.files[2].rev); }
		if (scenario == 9) QVERIFY(errors.first()[1].toBool());
	}
	if (scenario == 1 || scenario == 2 || scenario == 12 || scenario == 14 || scenario == 15 || scenario == 23 || scenario == 24) QCOMPARE(requests, 0);
	QCOMPARE(lookups, 0); QCOMPARE(n.mutations, 0); QCOMPARE(n.files.size(), before.size());
	for (int i = 0; i < before.size(); ++i) { QCOMPARE(n.files[i].data, before[i].data); QCOMPARE(n.files[i].name, before[i].name); QCOMPARE(n.files[i].rev, before[i].rev); }
	QVERIFY(n.allowed);
}

void TestCloudSyncHistory::workspaceAuthority_data()
{
	QTest::addColumn<bool>("google"); QTest::addColumn<int>("scenario");
	const QStringList cases = {"restart-and-lost-ack", "advance-and-reservation", "rollback", "same-name-foreign-id", "foreign-account", "corrupt-file",
		"future-format", "held-lock", "blocked-parent", "missing-parent", "oversized-file", "empty-revision", "unsafe-revision", "stale-process",
		"same-body-new-revision", "tampered-binding", "invalid-reference", "required-checkpoint-missing", "required-checkpoint-existing"};
	for (bool g : {false, true}) for (int s = 0; s < cases.size(); ++s) QTest::newRow(qPrintable(QString("%1-checkpoint-%2").arg(g).arg(cases[s]))) << g << s;
}
void TestCloudSyncHistory::workspaceAuthority()
{
	QFETCH(bool, google); QFETCH(int, scenario);
	const auto provider = QString(google ? "google-drive" : "dropbox"), epoch = uid(), first = uid(), pending = uid(), reservation = uid();
	const auto ref = CloudWorkspaceReference::create(provider, Account, epoch, google ? "root-a" : "id:root-a");
	auto root = CloudWorkspaceRecord::create(ref, CloudSyncManifest::sha256("legacy"));
	QVERIFY(root.registerUpload(first, {}, CloudSyncManifest::sha256(A), A.size()));
	QVERIFY(root.completeUpload(first, google ? "payload-a" : "id:payload-a", "v1")); QVERIFY(root.publish(first, {}));
	const auto original = root;
	QVERIFY(root.registerUpload(pending, first, CloudSyncManifest::sha256(B), B.size())); QVERIFY(root.reserve(reservation, first));
	QTemporaryDir temporary; QVERIFY(temporary.isValid());
	const auto path = temporary.filePath(scenario == 9 ? "new/parent/checkpoint.json" : "checkpoint.json");
	const auto read = [](const QString &name) { QFile f(name); return f.open(QIODevice::ReadOnly) ? f.readAll() : QByteArray(); };
	const auto write = [](const QString &name, const QByteArray &bytes) { QFile f(name); return f.open(QIODevice::WriteOnly) && f.write(bytes) == bytes.size(); };
	const auto selection = temporary.filePath("selection.json"), baseline = temporary.filePath("sync-baseline.json"), local = temporary.filePath("divelog.xml");
	QVERIFY(write(selection, "preserve selected workspace")); QVERIFY(write(baseline, "preserve schema 2 baseline")); QVERIFY(write(local, A));
	CloudWorkspaceAuthority authority(path, ref); QVERIFY(authority.load()); QVERIFY(!authority.record().valid());
	if (scenario == 9) QVERIFY(!QFileInfo::exists(QFileInfo(path).absolutePath()));
	QVERIFY(authority.observe(original, "root-v1")); const auto initialBytes = read(path);
	if (scenario == 17 || scenario == 18) {
		if (scenario == 17) { QVERIFY(QFile::remove(path)); QVERIFY(!authority.observe(original, "root-v1", true)); QVERIFY(!QFileInfo::exists(path)); }
		else { QVERIFY(authority.observe(original, "root-v1", true)); QCOMPARE(read(path), initialBytes); }
	} else if (scenario == 0 || scenario == 9) {
		QVERIFY(authority.observe(original, "root-v1")); QCOMPARE(read(path), initialBytes);
		CloudWorkspaceAuthority restarted(path, ref); QVERIFY(restarted.load()); QCOMPARE(restarted.record().json(), original.json()); QCOMPARE(restarted.version(), QString("root-v1"));
	} else if (scenario == 1) {
		QVERIFY(authority.observe(root, "root-v2")); CloudWorkspaceAuthority restarted(path, ref); QVERIFY(restarted.load()); QCOMPARE(restarted.record().json(), root.json());
		QCOMPARE(restarted.record().ticket(pending), root.ticket(pending)); QVERIFY(!restarted.observe(original, "root-v1"));
		QCOMPARE(restarted.record().value["journal"].toObject()["reservations"], root.value["journal"].toObject()["reservations"]);
	} else if (scenario == 2) {
		QVERIFY(authority.observe(root, "root-v2")); const auto advanced = read(path); QVERIFY(!authority.observe(original, "different-new-provider-revision")); QCOMPARE(read(path), advanced);
	} else if (scenario == 3 || scenario == 4 || scenario == 16) {
		auto foreign = CloudWorkspaceReference::create(provider, scenario == 4 ? CloudSyncManifest::sha256("other-account") : Account, epoch,
			scenario == 3 ? (google ? "root-b" : "id:root-b") : ref.value["root_id"].toString());
		if (scenario == 16) foreign.value["unknown_safety_field"] = true;
		CloudWorkspaceAuthority other(path, foreign); QVERIFY(!other.load()); QVERIFY(!other.observe(CloudWorkspaceRecord::create(foreign, CloudSyncManifest::sha256("legacy")), "foreign-revision"));
		QCOMPARE(read(path), initialBytes);
	} else if (scenario == 8) {
		const auto blocker = temporary.filePath("unrelated-blocker"); QVERIFY(write(blocker, "preserve blocker"));
		CloudWorkspaceAuthority blocked(blocker + "/checkpoint.json", ref); QVERIFY(!blocked.observe(root, "root-v2")); QCOMPARE(read(blocker), QByteArray("preserve blocker"));
	} else if (scenario == 13) {
		CloudWorkspaceAuthority other(path, ref); QVERIFY(other.load()); QVERIFY(other.observe(root, "root-v2")); const auto advanced = read(path);
		QVERIFY(!authority.observe(original, "root-v1")); QCOMPARE(authority.record().json(), root.json()); QCOMPARE(read(path), advanced);
		QVERIFY(authority.observe(root, "root-v2")); QCOMPARE(read(path), advanced);
	} else if (scenario == 14) {
		QVERIFY(authority.observe(original, "root-metadata-v2")); QCOMPARE(authority.record().json(), original.json()); QCOMPARE(authority.version(), QString("root-metadata-v2"));
		CloudWorkspaceAuthority restarted(path, ref); QVERIFY(restarted.load()); QCOMPARE(restarted.version(), authority.version());
	} else {
		std::unique_ptr<QLockFile> held; QString revision = "root-v2";
		if (scenario == 5) QVERIFY(write(path, "invalid checkpoint"));
		if (scenario == 6 || scenario == 15) {
			auto stored = QJsonDocument::fromJson(read(path)).object();
			if (scenario == 6) stored["format"] = "unknown-future-checkpoint"; else stored["workspace_key"] = CloudSyncManifest::sha256("other-root");
			QVERIFY(write(path, QJsonDocument(stored).toJson(QJsonDocument::Compact)));
		}
		if (scenario == 7) { held = std::make_unique<QLockFile>(path + ".lock"); QVERIFY(held->tryLock(0)); QVERIFY(!authority.load()); QVERIFY(!authority.record().valid()); }
		if (scenario == 10) QVERIFY(write(path, QByteArray(1024 * 1024 + 5121, ' ')));
		if (scenario == 11) revision.clear();
		if (scenario == 12) revision = QString("unsafe") + QChar::Null + "revision";
		const auto before = read(path); QVERIFY(!authority.observe(root, revision)); QCOMPARE(read(path), before);
	}
	QCOMPARE(read(selection), QByteArray("preserve selected workspace")); QCOMPARE(read(baseline), QByteArray("preserve schema 2 baseline")); QCOMPARE(read(local), A);
}

void TestCloudSyncHistory::workspaceCreationIntent_data()
{
	QTest::addColumn<bool>("google"); QTest::addColumn<int>("scenario");
	const QStringList cases = {"transitions-and-restart", "submitted-never-resubmits", "competing-submit", "different-nonce", "different-account", "different-origin",
		"corrupt", "future-format", "lock", "oversized", "blocked-parent", "unsafe-id", "immutable-id", "cannot-bind-before-id", "invalid-scope", "unknown-state", "unknown-field", "new-parent"};
	for (bool g : {false, true}) for (int s = 0; s < cases.size(); ++s) QTest::newRow(qPrintable(QString("%1-intent-%2").arg(g).arg(cases[s]))) << g << s;
}
void TestCloudSyncHistory::workspaceCreationIntent()
{
	QFETCH(bool, google); QFETCH(int, scenario);
	const auto provider = QString(google ? "google-drive" : "dropbox"), epoch = uid(), nonce = uid(), origin = CloudSyncManifest::sha256("origin"), id = QString(google ? "root-a" : "id:root-a");
	QTemporaryDir temporary; QVERIFY(temporary.isValid()); const auto path = temporary.filePath(scenario == 17 ? "new/parent/intent.json" : "intent.json");
	const auto read = [&] { QFile f(path); return f.open(QIODevice::ReadOnly) ? f.readAll() : QByteArray(); };
	const auto write = [&](const QByteArray &bytes) { QFile f(path); return f.open(QIODevice::WriteOnly) && f.write(bytes) == bytes.size(); };
	CloudWorkspaceCreationIntent intent(path, provider, Account, epoch, nonce, origin); QVERIFY(intent.prepare()); const auto initial = read();
	QCOMPARE(intent.state(), QString("prepared")); QVERIFY(intent.id().isEmpty()); QVERIFY(!CloudWorkspaceRecord::parse(intent.bootstrap()).valid());
	QVERIFY(intent.bootstrap().contains(nonce.toUtf8())); QVERIFY(!intent.bootstrap().contains("tickets"));
	if (scenario == 0 || scenario == 17) {
		QVERIFY(intent.prepare()); QCOMPARE(read(), initial); QVERIFY(intent.submit()); QVERIFY(intent.identify(id)); const auto identified = read();
		QVERIFY(intent.identify(id)); QCOMPARE(read(), identified); QVERIFY(!intent.bind()); QVERIFY(intent.beginBinding()); QVERIFY(intent.bind()); const auto bound = read(); QVERIFY(intent.bind()); QCOMPARE(read(), bound);
		CloudWorkspaceCreationIntent restarted(path, provider, Account, epoch, nonce, origin); QVERIFY(restarted.prepare()); QCOMPARE(restarted.state(), QString("bound")); QCOMPARE(restarted.reference().value, intent.reference().value);
	} else if (scenario == 1 || scenario == 2) {
		CloudWorkspaceCreationIntent other(path, provider, Account, epoch, nonce, origin); QVERIFY(other.prepare()); QVERIFY(other.submit()); const auto submitted = read();
		QVERIFY(!intent.submit()); QCOMPARE(read(), submitted); QVERIFY(intent.prepare()); QCOMPARE(intent.state(), QString("submitted")); QVERIFY(!intent.submit()); QCOMPARE(read(), submitted);
	} else if (scenario == 3 || scenario == 4 || scenario == 5 || scenario == 14) {
		CloudWorkspaceCreationIntent other(path, provider, scenario == 4 ? CloudSyncManifest::sha256("other-account") : Account, scenario == 14 ? QString("invalid-epoch") : epoch,
			scenario == 3 ? uid() : nonce, scenario == 5 ? CloudSyncManifest::sha256("other-origin") : origin);
		QVERIFY(!other.prepare()); QCOMPARE(read(), initial);
	} else if (scenario == 10) {
		const auto blocker = temporary.filePath("blocker"); QFile f(blocker); QVERIFY(f.open(QIODevice::WriteOnly)); QCOMPARE(f.write("preserve"), qint64(8)); f.close();
		CloudWorkspaceCreationIntent blocked(blocker + "/intent.json", provider, Account, epoch, nonce, origin); QVERIFY(!blocked.prepare());
		QVERIFY(f.open(QIODevice::ReadOnly)); QCOMPARE(f.readAll(), QByteArray("preserve"));
	} else if (scenario == 11 || scenario == 12 || scenario == 13) {
		if (scenario == 13) { QVERIFY(!intent.bind()); QCOMPARE(read(), initial); }
		else {
			QVERIFY(intent.submit()); if (scenario == 12) QVERIFY(intent.identify(id)); const auto before = read();
			QVERIFY(!intent.identify(scenario == 11 ? QString("unsafe/id") : (google ? QString("other-id") : QString("id:other-id")))); QCOMPARE(read(), before);
		}
	} else {
		std::unique_ptr<QLockFile> held;
		if (scenario == 6) QVERIFY(write("invalid intent"));
		if (scenario == 9) QVERIFY(write(QByteArray(8193, ' ')));
		if (scenario == 7 || scenario == 15 || scenario == 16) {
			auto value = QJsonDocument::fromJson(read()).object();
			if (scenario == 7) value["format"] = "future-intent"; if (scenario == 15) value["state"] = "unknown"; if (scenario == 16) value["unknown_field"] = true;
			QVERIFY(write(QJsonDocument(value).toJson(QJsonDocument::Compact)));
		}
		if (scenario == 8) { held = std::make_unique<QLockFile>(path + ".lock"); QVERIFY(held->tryLock(0)); }
		const auto before = read(); QVERIFY(!intent.prepare()); QVERIFY(!intent.submit()); QCOMPARE(read(), before);
	}
}
void TestCloudSyncHistory::workspaceCreateTransport_data()
{
	QTest::addColumn<bool>("google"); QTest::addColumn<int>("scenario");
	const QStringList cases = {"create", "ordinary-name", "bad-epoch", "empty-token", "empty-body", "oversized-body", "bad-provider", "lost-ack",
		"missing-id", "wrong-name", "unsafe-id", "same-name", "rejected-token", "cancel"};
	for (bool g : {false, true}) for (int s = 0; s < cases.size(); ++s) QTest::newRow(qPrintable(QString("%1-create-transport-%2").arg(g).arg(cases[s]))) << g << s;
}
void TestCloudSyncHistory::workspaceCreateTransport()
{
	QFETCH(bool, google); QFETCH(int, scenario); Network n; n.seed(); n.workspaceWrites = true; const auto old = n.files;
	const auto type = google ? CloudSyncProviderType::GoogleDrive : CloudSyncProviderType::Dropbox;
	const auto epoch = uid(), name = "subsurface-neo-workspace-" + epoch + ".json"; QString target = name, token = "token"; QByteArray body = "synthetic-bootstrap";
	if (scenario == 1) target = Legacy; if (scenario == 2) target = "subsurface-neo-workspace-invalid.json"; if (scenario == 3) token.clear();
	if (scenario == 4) body.clear(); if (scenario == 5) body = QByteArray(8193, ' '); if (scenario == 7) n.loseWriteAck = true;
	if (scenario == 8 || scenario == 9 || scenario == 10) n.response = [&](const QNetworkRequest &, const QByteArray &bytes) {
		auto m = QJsonDocument::fromJson(bytes).object(); m[scenario == 9 ? "name" : "id"] = scenario == 8 ? "" : scenario == 9 ? "wrong-name" : "unsafe/id"; return n.json(m);
	};
	if (scenario == 11) n.add(name, "preserve competing root"); if (scenario == 12) n.writeFailureStatus = 401; if (scenario == 13) n.deferReads = true;
	int lookups = 0; n.requested = [&](const QNetworkRequest &r) { if (r.url().path() == "/drive/v3/files") ++lookups; };
	CloudSyncFileStore files(&n); QSignalSpy done(&files, &CloudSyncFileStore::workspaceIdentityCreated), errors(&files, &CloudSyncFileStore::operationError), conflicts(&files, &CloudSyncFileStore::writeConflict);
	files.createWorkspaceIdentity(scenario == 6 ? CloudSyncProviderType::SubsurfaceCloud : type, token, target, body);
	if (scenario == 13) { QTRY_VERIFY(n.deferred); files.cancel(type); if (n.deferred) n.deferred->complete(); QTest::qWait(20); QCOMPARE(done.size() + errors.size() + conflicts.size(), 0); }
	else { QTRY_COMPARE(done.size() + errors.size() + conflicts.size(), 1); QCOMPARE(done.size(), int(scenario == 0 || (scenario == 11 && google))); }
	QCOMPARE(n.mutations, int(scenario == 0 || scenario >= 7)); QCOMPARE(lookups, 0);
	if (scenario == 11) QCOMPARE(n.files[2].data, QByteArray("preserve competing root"));
	if (scenario == 12) QVERIFY(errors.first()[3].toBool());
	for (int i = 0; i < old.size(); ++i) { QCOMPARE(n.files[i].data, old[i].data); QCOMPARE(n.files[i].rev, old[i].rev); QCOMPARE(n.files[i].name, old[i].name); }
	QVERIFY(n.allowed);
}

void TestCloudSyncHistory::workspaceCopyRecord_data()
{
	QTest::addColumn<bool>("google"); QTest::addColumn<int>("mode");
	const QStringList cases = {"transitions", "foreign-provider", "foreign-account", "stale-parent", "duplicate-ticket", "bad-source", "source-as-payload", "root-as-payload",
		"missing-ticket", "changed-proof", "downgrade", "retroactive-proof", "unknown-format", "ticket-size", "source-as-root", "unsafe-identity", "pending-preservation", "checkpoint", "empty-proof-table", "unknown-source-field", "equal-name-foreign-root"};
	for (bool g : {false, true}) for (int m = 0; m < cases.size(); ++m) QTest::newRow(qPrintable(QString("%1-copy-record-%2").arg(g).arg(cases[m]))) << g << m;
}
void TestCloudSyncHistory::workspaceCopyRecord()
{
	QFETCH(bool, google); QFETCH(int, mode); Network n; n.seed(); auto source = copySource(n, google); QVERIFY(source.valid());
	const auto provider = QString(google ? "google-drive" : "dropbox"), epoch = uid(), ticket = uid();
	const auto ref = CloudWorkspaceReference::create(provider, Account, epoch, google ? "root" : "id:root");
	auto root = CloudWorkspaceRecord::create(ref, CloudSyncManifest::sha256("genesis")); const auto initial = root;
	if (mode == 1) source.value["provider"] = google ? "dropbox" : "google-drive";
	if (mode == 2) source.value["account"] = CloudSyncManifest::sha256("foreign");
	if (mode == 5) source.value["unknown"] = true;
	if (mode == 14) { auto p = source.value["payload"].toObject(); p["id"] = ref.value["root_id"]; source.value["payload"] = p; }
	if (mode == 1 || mode == 2 || mode == 3 || mode == 5 || mode == 14) { QVERIFY(!root.registerCopy(ticket, mode == 3 ? uid() : QString(), source)); QCOMPARE(root.json(), initial.json()); return; }
	QVERIFY(root.registerCopy(ticket, {}, source)); QVERIFY(root.follows(initial)); const auto registered = root;
	QCOMPARE(root.copySource(ticket).json(), source.json()); QVERIFY(!CloudHistoryRecord::parse(root.json()).valid());
	if (mode == 4 || mode == 6 || mode == 7 || mode == 15) {
		if (mode == 4) QVERIFY(!root.registerCopy(ticket, {}, source));
		else QVERIFY(!root.completeUpload(ticket, mode == 6 ? source.value["payload"].toObject()["id"].toString() : mode == 7 ? ref.value["root_id"].toString() : QString("unsafe/id"), "revision"));
		QCOMPARE(root.json(), registered.json()); return;
	}
	if (mode == 8 || mode == 9 || mode == 10 || mode == 11 || mode == 12 || mode == 13 || mode == 18 || mode == 19) {
		auto copies = root.value["copies"].toObject();
		if (mode == 8) { copies[uid()] = source.value; root.value["copies"] = copies; }
		if (mode == 9) { auto changed = source.value; auto m = changed["manifest"].toObject(); m["version"] = "changed-revision"; changed["manifest"] = m; copies[ticket] = changed; root.value["copies"] = copies; }
		if (mode == 10) { root.value.remove("copies"); root.value["format"] = "neo-workspace-history-development-1"; }
		if (mode == 11) {
			auto ordinary = registered; ordinary.value.remove("copies"); ordinary.value["format"] = "neo-workspace-history-development-1";
			auto journal = root.value["journal"].toObject(); journal["sequence"] = journal["sequence"].toInt() + 1; root.value["journal"] = journal;
			QVERIFY(root.valid()); QVERIFY(ordinary.valid()); QVERIFY(!root.follows(ordinary)); return;
		}
		if (mode == 12) root.value["format"] = "future-copy-root";
		if (mode == 13) { auto changed = root.value["journal"].toObject(); auto tickets = changed["tickets"].toObject(); auto t = tickets[ticket].toObject(); t["bytes"] = A.size() + 1; tickets[ticket] = t; changed["tickets"] = tickets; root.value["journal"] = changed; }
		if (mode == 18) root.value["copies"] = QJsonObject();
		if (mode == 19) { auto changed = source.value; changed["unknown_safety_field"] = true; copies[ticket] = changed; root.value["copies"] = copies; }
		if (mode == 9 || mode == 10) { QVERIFY(root.valid()); QVERIFY(!root.follows(registered)); }
		else QVERIFY(!root.valid());
		return;
	}
	QVERIFY(root.completeUpload(ticket, google ? "copied-payload" : "id:copied-payload", "copied-revision")); QVERIFY(root.publish(ticket, {}));
	QVERIFY(root.follows(registered)); QCOMPARE(CloudWorkspaceRecord::parse(root.json()).json(), root.json());
	if (mode == 16) { const auto pending = uid(); QVERIFY(root.registerCopy(pending, ticket, source)); QVERIFY(root.reserve(uid(), ticket)); QCOMPARE(root.copySource(ticket).json(), source.json()); QCOMPARE(root.ticket(pending)["state"].toString(), QString("registered")); }
	if (mode == 17) { QTemporaryDir dir; CloudWorkspaceAuthority cp(dir.filePath("checkpoint.json"), ref); QVERIFY(cp.observe(root, "root-revision")); QVERIFY(!cp.observe(initial, "initial-revision", true)); CloudWorkspaceAuthority restarted(dir.filePath("checkpoint.json"), ref); QVERIFY(restarted.load()); QCOMPARE(restarted.record().json(), root.json()); }
	if (mode == 20) { auto other = CloudWorkspaceRecord::create(CloudWorkspaceReference::create(provider, Account, epoch, google ? "other-root" : "id:other-root"), CloudSyncManifest::sha256("genesis")); QVERIFY(other.registerCopy(ticket, {}, source)); QVERIFY(other.reference().payloadName(ticket) != ref.payloadName(ticket)); QVERIFY(!root.follows(other)); }
}
void TestCloudSyncHistory::workspaceCopyIntent_data()
{
	QTest::addColumn<bool>("google"); QTest::addColumn<int>("mode");
	const QStringList cases = {"restart", "competing-submit", "foreign-ticket", "foreign-parent", "foreign-scope", "foreign-proof", "corrupt", "missing", "locked", "oversized", "blocked-parent",
		"unsafe-id", "source-id", "root-id", "immutable-id", "bad-version", "invalid-ticket", "future-format", "unknown-state", "early-id", "early-version", "cannot-skip"};
	for (bool g : {false, true}) for (int m = 0; m < cases.size(); ++m) QTest::newRow(qPrintable(QString("%1-copy-intent-%2").arg(g).arg(cases[m]))) << g << m;
}
void TestCloudSyncHistory::workspaceCopyIntent()
{
	QFETCH(bool, google); QFETCH(int, mode); Network n; n.seed(); const auto source = copySource(n, google);
	const auto provider = QString(google ? "google-drive" : "dropbox"), ticket = uid(), id = QString(google ? "new-payload" : "id:new-payload");
	const auto ref = CloudWorkspaceReference::create(provider, Account, uid(), google ? "root" : "id:root"); QTemporaryDir dir; const auto path = dir.filePath("intent.json");
	const auto read = [&] { QFile f(path); return f.open(QIODevice::ReadOnly) ? f.readAll() : QByteArray(); };
	const auto write = [&](const QByteArray &bytes) { QFile f(path); return f.open(QIODevice::WriteOnly) && f.write(bytes) == bytes.size(); };
	CloudWorkspaceCopyIntent intent(path, ref, source, ticket, {}); QVERIFY(intent.prepare()); const auto prepared = read();
	if (mode == 2 || mode == 3 || mode == 4 || mode == 5 || mode == 16) {
		auto otherSource = source; auto otherRef = ref;
		if (mode == 4) otherRef.value["account"] = CloudSyncManifest::sha256("foreign");
		if (mode == 5) { auto p = otherSource.value["payload"].toObject(); p["version"] = "different-version"; otherSource.value["payload"] = p; }
		CloudWorkspaceCopyIntent other(path, otherRef, otherSource, mode == 2 ? uid() : mode == 16 ? QString("invalid") : ticket, mode == 3 ? uid() : QString());
		QVERIFY(!other.prepare()); QCOMPARE(read(), prepared); return;
	}
	if (mode == 6 || mode == 7 || mode == 8 || mode == 9 || mode >= 17) {
		std::unique_ptr<QLockFile> lock;
		if (mode == 6) QVERIFY(write("corrupt")); if (mode == 7) QVERIFY(QFile::remove(path)); if (mode == 9) QVERIFY(write(QByteArray(8193, ' ')));
		if (mode == 8) { lock = std::make_unique<QLockFile>(path + ".lock"); QVERIFY(lock->tryLock(0)); }
		if (mode >= 17 && mode <= 20) { auto v = QJsonDocument::fromJson(read()).object(); if (mode == 17) v["format"] = "future-copy-intent"; if (mode == 18) v["state"] = "unknown"; if (mode == 19) v["object"] = id; if (mode == 20) v["version"] = "early-version"; QVERIFY(write(n.json(v))); }
		const auto before = read();
		if (mode == 21) { QVERIFY(!intent.submit()); QVERIFY(!intent.identify(id)); QVERIFY(!intent.verify("v")); QVERIFY(!intent.upload()); QVERIFY(!intent.commit()); }
		else { QVERIFY(!intent.reload()); QVERIFY(!intent.beginRegistration()); }
		QCOMPARE(read(), before); return;
	}
	if (mode == 10) { CloudWorkspaceCopyIntent blocked(path + "/child.json", ref, source, ticket, {}); QVERIFY(!blocked.prepare()); QCOMPARE(read(), prepared); return; }
	QVERIFY(intent.beginRegistration()); QVERIFY(intent.beginRegistration()); QVERIFY(intent.registerTicket()); QVERIFY(intent.submit()); const auto submitted = read(); QVERIFY(!intent.submit()); QCOMPARE(read(), submitted);
	if (mode == 1) { CloudWorkspaceCopyIntent other(path, ref, source, ticket, {}); QVERIFY(other.prepare()); QVERIFY(!other.submit()); QCOMPARE(read(), submitted); return; }
	if (mode == 11 || mode == 12 || mode == 13) { QVERIFY(!intent.identify(mode == 11 ? QString("unsafe/id") : mode == 12 ? source.value["payload"].toObject()["id"].toString() : ref.value["root_id"].toString())); QCOMPARE(read(), submitted); return; }
	QVERIFY(intent.identify(id)); const auto identified = read(); QVERIFY(intent.identify(id)); QCOMPARE(read(), identified);
	if (mode == 14) { QVERIFY(!intent.identify(google ? "different-id" : "id:different-id")); QCOMPARE(read(), identified); return; }
	if (mode == 15) { QVERIFY(!intent.verify("bad\nrevision")); QCOMPARE(read(), identified); return; }
	QVERIFY(intent.verify("payload-version")); QVERIFY(intent.upload()); QVERIFY(intent.commit()); const auto committed = read(); QVERIFY(intent.commit()); QCOMPARE(read(), committed);
	CloudWorkspaceCopyIntent restarted(path, ref, source, ticket, {}); QVERIFY(restarted.reload()); QCOMPARE(restarted.state(), QString("committed")); QCOMPARE(restarted.id(), id); QCOMPARE(restarted.version(), QString("payload-version")); QVERIFY(!restarted.submit());
}
void TestCloudSyncHistory::workspacePayloadTransport_data()
{
	QTest::addColumn<bool>("google"); QTest::addColumn<int>("mode");
	const QStringList cases = {"create", "ordinary-name", "root-name", "bad-key", "bad-ticket", "empty-token", "empty-body", "oversized", "bad-provider", "lost-ack", "missing-id", "wrong-name", "same-name", "rejected-token", "cancel", "unsafe-id"};
	for (bool g : {false, true}) for (int m = 0; m < cases.size(); ++m) QTest::newRow(qPrintable(QString("%1-payload-create-%2").arg(g).arg(cases[m]))) << g << m;
}
void TestCloudSyncHistory::workspacePayloadTransport()
{
	QFETCH(bool, google); QFETCH(int, mode); Network n; n.seed(); n.workspaceWrites = n.workspaceCopies = true;
	const auto ref = CloudWorkspaceReference::create(google ? "google-drive" : "dropbox", Account, uid(), google ? "3" : "id:3"); const auto ticket = uid();
	auto root = CloudWorkspaceRecord::create(ref, CloudSyncManifest::sha256("genesis")); QVERIFY(root.registerCopy(ticket, {}, copySource(n, google))); n.add(ref.rootName(), root.json());
	const auto before = n.files; QString name = ref.payloadName(ticket), token = "token"; QByteArray data = A;
	if (mode == 1) name = "subsurface-neo.xml"; if (mode == 2) name = ref.rootName(); if (mode == 3) name.replace(ref.key(), "invalid"); if (mode == 4) name.replace(ticket, "invalid");
	if (mode == 5) token.clear(); if (mode == 6) data.clear(); if (mode == 7) data = QByteArray(64 * 1024 * 1024 + 1, ' '); if (mode == 9) n.loseWriteAck = true;
	if (mode == 10 || mode == 11 || mode == 15) n.response = [&](const QNetworkRequest &, const QByteArray &bytes) { auto v = QJsonDocument::fromJson(bytes).object(); v[mode == 11 ? "name" : "id"] = mode == 10 ? "" : mode == 11 ? "wrong-name" : "unsafe/id"; return n.json(v); };
	if (mode == 12) n.add(name, A); if (mode == 13) n.writeFailureStatus = 401; if (mode == 14) n.deferReads = true;
	int lookups = 0; n.requested = [&](const QNetworkRequest &r) { if (r.url().path() == "/drive/v3/files") ++lookups; };
	CloudSyncFileStore store(&n); QSignalSpy done(&store, &CloudSyncFileStore::workspacePayloadCreated), errors(&store, &CloudSyncFileStore::operationError), conflicts(&store, &CloudSyncFileStore::writeConflict);
	store.createWorkspacePayloadIdentity(mode == 8 ? CloudSyncProviderType::SubsurfaceCloud : google ? CloudSyncProviderType::GoogleDrive : CloudSyncProviderType::Dropbox, token, name, data);
	if (mode == 14) { QTRY_VERIFY(n.deferred); store.cancel(google ? CloudSyncProviderType::GoogleDrive : CloudSyncProviderType::Dropbox); if (n.deferred) n.deferred->complete(); QTest::qWait(20); QCOMPARE(done.size() + errors.size() + conflicts.size(), 0); }
	else { QTRY_COMPARE(done.size() + errors.size() + conflicts.size(), 1); QCOMPARE(done.size(), int(mode == 0 || (mode == 12 && google))); }
	QCOMPARE(lookups, 0); QCOMPARE(n.mutations, int(mode == 0 || mode >= 9)); if (mode == 13) QVERIFY(errors.first()[3].toBool());
	for (int i = 0; i < before.size(); ++i) QCOMPARE(n.files[i].data, before[i].data); if (mode == 12) QCOMPARE(n.files[3].data, A); QVERIFY(n.allowed);
}

void TestCloudSyncHistory::workspaceCopyWorkflow_data()
{
	QTest::addColumn<bool>("google"); QTest::addColumn<int>("mode");
	const QStringList cases = {"copy-restart", "schema1", "scope-before", "missing-choice", "stale-choice", "missing-checkpoint", "corrupt-checkpoint", "corrupt-intent", "locked-intent", "submitted-unknown",
		"register-ack-lost", "payload-ack-lost", "upload-ack-lost", "publish-ack-lost", "source-changed-before-publish", "source-changed-after-publish", "token-rejected", "cancel-upload",
		"choice-during-upload", "scope-during-upload", "checkpoint-lost-during-upload", "missing-proof-on-resume", "resume-identified", "resume-uploaded", "fresh-attempt-idempotence", "rollback",
		"fresh-after-unknown", "competing-head", "ack-points-at-root", "ack-points-at-source", "wrong-payload", "payload-changed-before-publish", "payload-changed-after-publish", "choice-away-back"};
	for (bool g : {false, true}) for (int m = 0; m < cases.size(); ++m) QTest::newRow(qPrintable(QString("%1-copy-workflow-%2").arg(g).arg(cases[m]))) << g << m;
}
void TestCloudSyncHistory::workspaceCopyWorkflow()
{
	QFETCH(bool, google); QFETCH(int, mode); Network n; n.seed(mode == 1 ? 1 : 2); n.workspaceWrites = n.workspaceCopies = true;
	const auto source = copySource(n, google); QVERIFY(source.valid()); const auto original = n.files;
	const auto provider = QString(google ? "google-drive" : "dropbox"), ticket = uid();
	const auto ref = CloudWorkspaceReference::create(provider, Account, uid(), google ? "3" : "id:3");
	const auto initial = CloudWorkspaceRecord::create(ref, CloudSyncManifest::sha256("genesis")); n.add(ref.rootName(), initial.json());
	QTemporaryDir dir; QVERIFY(dir.isValid()); const auto choiceFile = dir.filePath("choice.json"), cpFile = dir.filePath("checkpoint.json"), intentFile = dir.filePath("intent.json"), proofFile = dir.filePath("source.json");
	CloudWorkspaceSelection choice(choiceFile, provider, Account); QVERIFY(choice.select({}, ref)); const auto choiceVersion = choice.version();
	CloudWorkspaceAuthority cp(cpFile, ref); QVERIFY(cp.observe(initial, "rev-3")); QVERIFY(source.preserve(proofFile));
	const auto read = [](const QString &path) { QFile f(path); return f.open(QIODevice::ReadOnly) ? f.readAll() : QByteArray(); };
	const auto write = [](const QString &path, const QByteArray &bytes) { QFile f(path); return f.open(QIODevice::WriteOnly) && f.write(bytes) == bytes.size(); };
	const auto localFile = dir.filePath("local.xml"), baselineFile = dir.filePath("legacy-baseline.json"); QVERIFY(write(localFile, B)); QVERIFY(write(baselineFile, "preserve old baseline"));
	const auto choiceBytes = read(choiceFile), proofBytes = read(proofFile);
	std::unique_ptr<QLockFile> lock;
	if (mode == 2) n.guard = false; if (mode == 3) QVERIFY(QFile::remove(choiceFile)); if (mode == 5) QVERIFY(QFile::remove(cpFile));
	if (mode == 6) QVERIFY(write(cpFile, "invalid checkpoint")); if (mode == 7) QVERIFY(write(intentFile, "corrupt intent"));
	if (mode == 8) { lock = std::make_unique<QLockFile>(intentFile + ".lock"); QVERIFY(lock->tryLock(0)); }
	if (mode == 9 || mode == 21) { CloudWorkspaceCopyIntent saved(intentFile, ref, source, ticket, {}); QVERIFY(saved.prepare()); if (mode == 9) { QVERIFY(saved.beginRegistration()); QVERIFY(saved.registerTicket()); QVERIFY(saved.submit()); } else QVERIFY(QFile::remove(proofFile)); }
	if (mode == 25) { auto advanced = initial; QVERIFY(advanced.registerUpload(uid(), {}, CloudSyncManifest::sha256(A), A.size())); QVERIFY(cp.observe(advanced, "advanced-version")); }
	const auto cpBytes = read(cpFile);
	int requests = 0, payloadCreates = 0, rootWrites = 0, manifestAliases = 0; bool injected = false, changedSource = false, injectionFailed = false;
	const auto changeSource = [&] {
		auto m = CloudSyncManifest::forPayload(B); m.schemaVersion = 2; m.payloadFile = "subsurface-neo-" + m.revisionId + ".xml";
		n.files[0].data = m.toJson(); n.files[0].rev += "-changed"; n.add(m.payloadFileName(), B); changedSource = true;
	};
	const auto changeChoice = [&](bool back) {
		const auto other = CloudWorkspaceReference::create(provider, Account, uid(), google ? "other-root" : "id:other-root");
		CloudWorkspaceSelection c(choiceFile, provider, Account); QVERIFY(c.load()); QVERIFY(c.select(c.version(), other)); if (back) QVERIFY(c.select(c.version(), ref));
	};
	n.requested = [&](const QNetworkRequest &r) {
		++requests; const auto path = r.url().path(); const auto arg = QJsonDocument::fromJson(r.rawHeader("Dropbox-API-Arg")).object();
		const bool create = path == "/upload/drive/v3/files" || (!google && path == "/2/files/upload" && arg["mode"].toString() == "add");
		const bool update = path.startsWith("/upload/drive/v2/") || (!google && path == "/2/files/upload" && arg["mode"].isObject());
		if (create) { ++payloadCreates; if (mode == 11 || (mode == 26 && payloadCreates == 1)) n.loseWriteAck = true; if (mode == 16) n.writeFailureStatus = 401; if (mode == 17) n.deferReads = true; }
		if (update) { ++rootWrites; if ((mode == 10 && rootWrites == 1) || (mode == 12 && rootWrites == 2) || (mode == 13 && rootWrites == 3)) n.loseWriteAck = true; }
		QString alias;
		if (google && path == "/drive/v3/files") alias = QUrlQuery(r.url()).queryItemValue("q").section('\'', 1, 1);
		else if (!google) alias = arg["path"].toString().mid(1);
		if (alias == Legacy) ++manifestAliases;
		if (mode == 23 && !injected && manifestAliases == 3) { n.deferReads = true; injected = true; }
		if (mode == 22 && !injected && n.files.size() > 3 && (google ? path.endsWith("/" + n.files[3].id.mid(3)) : arg["path"].toString() == n.files[3].id)) { n.deferReads = true; injected = true; }
	};
	n.wrote = [&](const QString &name) {
		if (!name.endsWith(".json")) {
			if (mode == 14) changeSource();
			if (mode == 30) n.files[n.find(name)].data = B;
		} else {
			if (mode == 15 && rootWrites == 3) changeSource();
			if ((mode == 31 && rootWrites == 2) || (mode == 32 && rootWrites == 3)) { n.files[3].data = B; n.files[3].rev += "-changed"; }
			if (mode == 27 && rootWrites == 1) {
				auto competing = CloudWorkspaceRecord::parse(n.files[2].data); const auto other = uid(); QVERIFY(competing.registerUpload(other, {}, CloudSyncManifest::sha256(B), B.size()));
				QVERIFY(competing.completeUpload(other, google ? "competing-payload" : "id:competing-payload", "competing-version")); QVERIFY(competing.publish(other, {}));
				n.files[2].data = competing.json(); n.files[2].rev += "-competing";
			}
		}
	};
	n.response = [&](const QNetworkRequest &r, const QByteArray &bytes) {
		const auto path = r.url().path(); const auto arg = QJsonDocument::fromJson(r.rawHeader("Dropbox-API-Arg")).object();
		const bool create = path == "/upload/drive/v3/files" || (!google && path == "/2/files/upload" && arg["mode"].toString() == "add");
		if (create && !injected) {
			if (mode == 18 || mode == 33) { changeChoice(mode == 33); injected = true; }
			if (mode == 19) { n.guard = false; injected = true; }
			if (mode == 20) { injectionFailed = !QFile::remove(cpFile); injected = true; }
		}
		if (create && (mode == 28 || mode == 29)) { auto v = QJsonDocument::fromJson(bytes).object(); v["id"] = google ? (mode == 28 ? "3" : "2") : (mode == 28 ? "id:3" : "id:2"); return n.json(v); }
		return bytes;
	};
	CloudWorkspaceCopy client(&n); QSignalSpy done(&client, &CloudWorkspaceCopy::finished), errors(&client, &CloudWorkspaceCopy::failed);
	const auto start = [&](CloudWorkspaceCopy &c, const QString &attemptTicket, const QString &file) { c.start("token", ref, source, choiceFile, mode == 4 ? uid() : choiceVersion, cpFile, file, proofFile, attemptTicket, {}, [&] { return n.guard; }); };
	start(client, ticket, intentFile);
	if (mode == 17 || mode == 22 || mode == 23) {
		QTRY_VERIFY(n.deferred); client.cancel(); if (n.deferred) n.deferred->complete(); n.deferReads = false; QTest::qWait(20); QCOMPARE(done.size() + errors.size(), 0);
		if (mode != 17) { start(client, ticket, intentFile); QTRY_COMPARE(done.size() + errors.size(), 1); QCOMPARE(done.size(), 1); }
	} else { QTRY_COMPARE(done.size() + errors.size(), 1); const bool success = mode == 0 || mode == 1 || mode == 10 || mode == 12 || mode == 13 || mode == 15 || mode == 24; QCOMPARE(done.size(), int(success)); }
	if (!done.isEmpty()) {
		const auto completed = CloudWorkspaceRecord::parse(n.files[2].data); QVERIFY(completed.valid()); QCOMPARE(completed.head(), ticket); QCOMPARE(completed.copySource(ticket).json(), source.json());
		QCOMPARE(completed.ticket(ticket)["state"].toString(), QString("committed")); QCOMPARE(n.files[n.find(ref.payloadName(ticket))].data, A); QCOMPARE(done.last()[3].toBool(), mode == 15);
		QCOMPARE(payloadCreates, 1); QCOMPARE(rootWrites, 3); const auto mutations = n.mutations; const auto checkpointBytes = read(cpFile);
		if (mode == 0 || mode == 24) { start(client, mode == 24 ? uid() : ticket, mode == 24 ? dir.filePath("new-intent.json") : intentFile); QTRY_COMPARE(done.size(), 2); QCOMPARE(n.mutations, mutations); QCOMPARE(read(cpFile), checkpointBytes); }
	}
	if (mode == 26) {
		const auto requestCount = requests; CloudWorkspaceCopy refused(&n); QSignalSpy stopped(&refused, &CloudWorkspaceCopy::failed); start(refused, ticket, intentFile); QCOMPARE(stopped.size(), 1); QCOMPARE(requests, requestCount);
		const auto fresh = uid(); CloudWorkspaceCopy retry(&n); QSignalSpy completed(&retry, &CloudWorkspaceCopy::finished); start(retry, fresh, dir.filePath("fresh-intent.json")); QTRY_COMPARE(completed.size(), 1);
		const auto root = CloudWorkspaceRecord::parse(n.files[2].data); QCOMPARE(root.head(), fresh); QCOMPARE(root.ticket(ticket)["state"].toString(), QString("registered")); QCOMPARE(payloadCreates, 2);
	}
	if (mode == 2 || mode == 3 || mode == 4 || mode == 5 || mode == 6 || mode == 7 || mode == 8 || mode == 9 || mode == 21) QCOMPARE(requests, 0);
	if (mode == 16) QVERIFY(errors.first()[1].toBool());
	if (mode == 5 || mode == 20) QVERIFY(!QFileInfo::exists(cpFile)); if (mode == 6 || mode == 25) QCOMPARE(read(cpFile), cpBytes);
	if (mode == 31) QVERIFY(CloudWorkspaceRecord::parse(n.files[2].data).head().isEmpty());
	if (mode == 32) QCOMPARE(CloudWorkspaceRecord::parse(n.files[2].data).head(), ticket);
	if (mode != 3 && mode != 18 && mode != 33) QCOMPARE(read(choiceFile), choiceBytes);
	if (mode != 21) QCOMPARE(read(proofFile), proofBytes);
	QCOMPARE(read(localFile), B); QCOMPARE(read(baselineFile), QByteArray("preserve old baseline")); QCOMPARE(n.files[1].data, original[1].data);
	if (!changedSource) QCOMPARE(n.files[0].data, original[0].data); QVERIFY(n.allowed); QVERIFY(!injectionFailed); QCOMPARE(n.renames, 0);
}

void TestCloudSyncHistory::workspaceCopySource_data()
{
	QTest::addColumn<bool>("google"); QTest::addColumn<int>("mode");
	const QStringList cases = {"preserve-restart", "foreign-account", "future-format", "extra-field", "invalid-base64", "manifest-hash", "payload-hash", "wrong-name",
		"foreign-id", "same-id", "bad-revision", "fractional-size", "oversized-size", "wrong-payload", "corrupt-proof", "locked-proof", "blocked-parent", "conflicting-proof",
		"manifest-extra-field", "manifest-invalid-parent", "fractional-schema"};
	for (bool g : {false, true}) for (int m = 0; m < cases.size(); ++m) QTest::newRow(qPrintable(QString("%1-source-%2").arg(g).arg(cases[m]))) << g << m;
}
void TestCloudSyncHistory::workspaceCopySource()
{
	QFETCH(bool, google); QFETCH(int, mode);
	const auto provider = QString(google ? "google-drive" : "dropbox"); const auto m = CloudSyncManifest::forPayload(A); const auto bytes = m.toJson();
	CloudWorkspaceCopySource source; source.value = {{"schema", 1}, {"format", "neo-workspace-copy-source-development-1"}, {"provider", provider}, {"account", Account},
		{"manifest", QJsonObject{{"name", Legacy}, {"id", google ? "manifest" : "id:manifest"}, {"version", "m-rev"}, {"sha256", CloudSyncManifest::sha256(bytes)}, {"body_base64", QString::fromLatin1(bytes.toBase64())}}},
		{"payload", QJsonObject{{"name", m.payloadFileName()}, {"id", google ? "payload" : "id:payload"}, {"version", "p-rev"}, {"sha256", CloudSyncManifest::sha256(A)}, {"bytes", A.size()}}}};
	QVERIFY(source.valid()); QVERIFY(source.matches(provider, Account)); QVERIFY(source.accepts(A)); const auto original = source;
	QTemporaryDir temporary; QVERIFY(temporary.isValid()); const auto path = temporary.filePath("source.json");
	if ((mode >= 2 && mode <= 12) || mode >= 18) {
		auto manifest = source.value["manifest"].toObject(), payload = source.value["payload"].toObject();
		if (mode == 2) source.value["format"] = "future-source";
		if (mode == 3) source.value["unknown_safety_field"] = true;
		if (mode == 4) manifest["body_base64"] = "invalid%%%";
		if (mode == 5) manifest["sha256"] = CloudSyncManifest::sha256("other");
		if (mode == 6) payload["sha256"] = CloudSyncManifest::sha256(B);
		if (mode == 7) payload["name"] = "unrelated.xml";
		if (mode == 8) payload["id"] = "unsafe/id";
		if (mode == 9) payload["id"] = manifest["id"];
		if (mode == 10) payload["version"] = "invalid\nrevision";
		if (mode == 11) payload["bytes"] = 1.5;
		if (mode == 12) payload["bytes"] = 64 * 1024 * 1024 + 1;
		if (mode >= 18) {
			auto body = QJsonDocument::fromJson(bytes).object();
			if (mode == 18) body["unknown_safety_field"] = true;
			if (mode == 19) body["parent_sha256"] = "invalid-parent";
			if (mode == 20) body["schema"] = 1.5;
			const auto changed = QJsonDocument(body).toJson(QJsonDocument::Compact);
			manifest["body_base64"] = QString::fromLatin1(changed.toBase64()); manifest["sha256"] = CloudSyncManifest::sha256(changed);
		}
		source.value["manifest"] = manifest; source.value["payload"] = payload;
		QVERIFY(!source.valid()); QVERIFY(!source.preserve(path)); QVERIFY(!QFileInfo::exists(path)); return;
	}
	if (mode == 1) { QVERIFY(!source.matches(provider, CloudSyncManifest::sha256("foreign"))); QVERIFY(!source.matches(google ? "dropbox" : "google-drive", Account)); }
	if (mode == 13) QVERIFY(!source.accepts(B));
	if (mode == 16) { QFile blocker(path); QVERIFY(blocker.open(QIODevice::WriteOnly)); QCOMPARE(blocker.write("keep"), qint64(4)); blocker.close(); QVERIFY(!source.preserve(path + "/child.json")); return; }
	QVERIFY(source.preserve(path)); QFile file(path); QVERIFY(file.open(QIODevice::ReadOnly)); const auto saved = file.readAll(); file.close();
	QCOMPARE(CloudWorkspaceCopySource::load(path).json(), original.json()); QCOMPARE(CloudWorkspaceCopySource::parse(saved).key(), original.key());
	if (mode == 14) { QVERIFY(file.open(QIODevice::WriteOnly)); QCOMPARE(file.write("corrupt"), qint64(7)); file.close(); QVERIFY(!source.preserve(path)); QVERIFY(file.open(QIODevice::ReadOnly)); QCOMPARE(file.readAll(), QByteArray("corrupt")); }
	else if (mode == 15) { QLockFile lock(path + ".lock"); QVERIFY(lock.tryLock(0)); QVERIFY(!source.preserve(path)); }
	else if (mode == 17) { auto payload = source.value["payload"].toObject(); payload["version"] = "new-revision"; source.value["payload"] = payload; QVERIFY(source.valid()); QVERIFY(!source.preserve(path)); }
	else QVERIFY(source.preserve(path));
	if (mode != 14) { QVERIFY(file.open(QIODevice::ReadOnly)); QCOMPARE(file.readAll(), saved); }
}
void TestCloudSyncHistory::workspaceSourceReview_data()
{
	QTest::addColumn<bool>("google"); QTest::addColumn<int>("mode");
	const QStringList cases = {"schema2", "schema1", "bad-provider", "bad-account", "empty-token", "missing-guard", "scope-before", "missing-manifest", "missing-payload",
		"bad-manifest", "unsupported-schema", "checksum-mismatch", "invalid-xml", "oversized-manifest", "ambiguous-metadata", "revision-race", "manifest-replaced",
		"payload-replaced", "cancel", "scope-during-read", "rejected-token", "provider-failure", "manifest-renamed", "manifest-extra-field", "manifest-invalid-parent", "fractional-schema", "schema1-invalid-revision"};
	for (bool g : {false, true}) for (int m = 0; m < cases.size(); ++m) QTest::newRow(qPrintable(QString("%1-source-review-%2").arg(g).arg(cases[m]))) << g << m;
}
void TestCloudSyncHistory::workspaceSourceReview()
{
	QFETCH(bool, google); QFETCH(int, mode); Network n; n.seed(mode == 1 || mode == 26 ? 1 : 2);
	const auto provider = QString(google ? "google-drive" : "dropbox"), payloadName = n.files[1].name;
	if (mode == 7) n.files.removeAt(0); if (mode == 8) n.files.removeAt(1);
	if (mode == 9) n.files[0].data = "invalid manifest";
	if (mode == 10) { auto v = QJsonDocument::fromJson(n.files[0].data).object(); v["schema"] = 99; n.files[0].data = n.json(v); }
	if (mode == 11) n.files[1].data = B;
	if (mode == 12) { n.files[1].data = "not XML"; auto m = CloudSyncManifest::fromJson(n.files[0].data); m.payloadSha256 = CloudSyncManifest::sha256(n.files[1].data); n.files[0].data = m.toJson(); }
	if (mode == 13) n.files[0].data += QByteArray(16385, ' ');
	if (mode >= 23) {
		auto body = QJsonDocument::fromJson(n.files[0].data).object();
		if (mode == 23) body["unknown_safety_field"] = true;
		if (mode == 24) body["parent_sha256"] = "invalid-parent";
		if (mode == 25) body["schema"] = 1.5;
		if (mode == 26) body["revision"] = "unsupported-revision";
		n.files[0].data = n.json(body);
	}
	if (mode == 14) {
		if (google) n.add(Legacy, n.files[0].data);
		else n.metadataResponse = [&](const QByteArray &bytes) { auto m = QJsonDocument::fromJson(bytes).object(); m["id"] = ""; return n.json(m); };
	}
	if (mode == 18) n.deferReads = true; if (mode == 20) n.readFailureStatus = 401; if (mode == 21) n.readFailureStatus = 503; if (mode == 6) n.guard = false;
	int requests = 0, manifestAliases = 0, payloadAliases = 0, payloadReads = 0; bool injected = false;
	n.requested = [&](const QNetworkRequest &r) {
		++requests; if (mode == 19) n.guard = false;
		QString alias;
		if (google && r.url().path() == "/drive/v3/files") alias = QUrlQuery(r.url()).queryItemValue("q").section('\'', 1, 1);
		else if (!google) alias = QJsonDocument::fromJson(r.rawHeader("Dropbox-API-Arg")).object()["path"].toString().mid(1);
		if (alias == Legacy) ++manifestAliases; if (alias == payloadName) ++payloadAliases;
		if (!injected && ((mode == 16 && manifestAliases == 2) || (mode == 17 && payloadAliases == 2))) {
			n.files[mode == 16 ? 0 : 1].id = "id:replacement"; injected = true;
		}
	};
	n.response = [&](const QNetworkRequest &r, const QByteArray &bytes) {
		const auto target = QJsonDocument::fromJson(r.rawHeader("Dropbox-API-Arg")).object()["path"].toString();
		const bool payloadRead = google ? r.url().path() == "/drive/v3/files/2" : target == "/" + payloadName || target == "id:2";
		if (payloadRead) ++payloadReads;
		// Initial discovery can legitimately retry to a stable first revision.
		// Change an already observed source during its later exact verification.
		if (!injected && ((mode == 15 && payloadReads == 2) || (mode == 22 && payloadReads == 1))) {
			if (mode == 15) n.files[1].rev = "changed-revision"; else n.files[0].name = "retained-renamed-source.json"; injected = true;
		}
		return bytes;
	};
	const auto before = n.files; CloudWorkspaceSourceReview review(&n); QSignalSpy done(&review, &CloudWorkspaceSourceReview::finished), errors(&review, &CloudWorkspaceSourceReview::failed);
	review.start(mode == 4 ? "" : "token", mode == 2 ? "other" : provider, mode == 3 ? "bad-account" : Account, mode == 5 ? std::function<bool()>() : [&] { return n.guard; });
	if (mode == 18) { QTRY_VERIFY(n.deferred); review.cancel(); if (n.deferred) n.deferred->complete(); QTest::qWait(20); QCOMPARE(done.size() + errors.size(), 0); }
	else { QTRY_COMPARE(done.size() + errors.size(), 1); QCOMPARE(done.size(), int(mode == 0 || mode == 1)); }
	if (!done.isEmpty()) {
		CloudWorkspaceCopySource source; source.value = done.first()[0].toJsonObject(); QVERIFY(source.matches(provider, Account)); QVERIFY(source.accepts(A)); QCOMPARE(done.first()[1].toByteArray(), A);
		QCOMPARE(manifestAliases, 2); QCOMPARE(payloadAliases, 2); QCOMPARE(QByteArray::fromBase64(source.value["manifest"].toObject()["body_base64"].toString().toLatin1()), before[0].data);
		QTemporaryDir temporary; QVERIFY(source.preserve(temporary.filePath("proof.json"))); QCOMPARE(CloudWorkspaceCopySource::load(temporary.filePath("proof.json")).json(), source.json());
	}
	if (mode >= 2 && mode <= 6) QCOMPARE(requests, 0); if (mode == 20) QVERIFY(errors.first()[1].toBool());
	QCOMPARE(n.mutations, 0); QCOMPARE(n.renames, 0); QCOMPARE(n.files.size(), before.size());
	for (int i = 0; i < before.size(); ++i) QCOMPARE(n.files[i].data, before[i].data);
	QVERIFY(n.allowed);
}

void TestCloudSyncHistory::workspaceCreate_data()
{
	QTest::addColumn<bool>("google"); QTest::addColumn<int>("scenario");
	const QStringList cases = {"create-restart", "submitted-unknown", "lost-create-ack", "bad-create-ack", "wrong-create-id", "lost-bind-ack", "bad-bind-ack",
		"bind-cas-race", "two-creators", "scope-before", "scope-after-create", "scope-after-bind", "cancel-create", "cancel-known-read", "cancel-bind",
		"token-create", "token-bind", "corrupt-checkpoint", "missing-checkpoint", "unknown-bootstrap", "wrong-bootstrap-nonce", "moved-id-with-replacement",
		"blocked-intent-parent", "corrupt-intent", "locked-intent", "bad-scope", "blocked-checkpoint-directory", "bound-root-reset", "checkpoint-rollback", "same-name-root", "missing-established-root", "future-intent", "ack-points-at-original", "ack-points-at-same-scope-root"};
	for (bool g : {false, true}) for (int s = 0; s < cases.size(); ++s) QTest::newRow(qPrintable(QString("%1-create-%2").arg(g).arg(cases[s]))) << g << s;
}
void TestCloudSyncHistory::workspaceCreate()
{
	QFETCH(bool, google); QFETCH(int, scenario); Network n; n.seed(); n.workspaceWrites = true; const auto old = n.files;
	const auto provider = QString(google ? "google-drive" : "dropbox"), epoch = uid(), nonce = uid(), origin = CloudSyncManifest::sha256("origin");
	const auto name = "subsurface-neo-workspace-" + epoch + ".json";
	QTemporaryDir temporary; QVERIFY(temporary.isValid()); QString intentFile = temporary.filePath("intent.json"), directory = temporary.filePath("checkpoints");
	const auto write = [](const QString &path, const QByteArray &bytes) { QFile f(path); return f.open(QIODevice::WriteOnly) && f.write(bytes) == bytes.size(); };
	const auto readFile = [](const QString &path) { QFile f(path); return f.open(QIODevice::ReadOnly) ? f.readAll() : QByteArray(); };
	const auto selection = temporary.filePath("selection.json"), baseline = temporary.filePath("old-baseline.json"), local = temporary.filePath("divelog.xml");
	QVERIFY(write(selection, "preserve selection")); QVERIFY(write(baseline, "preserve baseline")); QVERIFY(write(local, A));
	if (scenario == 1 || scenario == 31) {
		CloudWorkspaceCreationIntent prepared(intentFile, provider, Account, epoch, nonce, origin); QVERIFY(prepared.prepare());
		if (scenario == 1) QVERIFY(prepared.submit()); else { auto v = QJsonDocument::fromJson(readFile(intentFile)).object(); v["format"] = "unknown-future-intent"; QVERIFY(write(intentFile, n.json(v))); }
	}
	if (scenario == 2) n.loseWriteAck = true;
	if (scenario == 3 || scenario == 4 || scenario == 6 || scenario == 32 || scenario == 33) n.response = [&](const QNetworkRequest &r, const QByteArray &bytes) {
		const bool update = r.url().path().startsWith("/upload/drive/v2/") || (!google && QJsonDocument::fromJson(r.rawHeader("Dropbox-API-Arg")).object()["mode"].isObject());
		const bool create = (google ? r.url().path() == "/upload/drive/v3/files" : r.url().path() == "/2/files/upload" && !update);
		if ((scenario == 6 && update) || ((scenario == 3 || scenario == 4 || scenario == 32 || scenario == 33) && create)) {
			if (scenario == 3 || scenario == 6) return QByteArray("{}");
			auto m = QJsonDocument::fromJson(bytes).object(); m["id"] = scenario == 32 ? (google ? "1" : "id:1") : scenario == 33 ? (google ? "3" : "id:3") : (google ? "wrong-id" : "id:wrong-id"); return n.json(m);
		} return bytes;
	};
	if (scenario == 9) n.guard = false; if (scenario == 12) n.deferReads = true; if (scenario == 15) n.writeFailureStatus = 401;
	std::unique_ptr<QLockFile> held;
	if (scenario == 22) { const auto blocker = temporary.filePath("blocker"); QVERIFY(write(blocker, "preserve blocker")); intentFile = blocker + "/intent.json"; }
	if (scenario == 23) QVERIFY(write(intentFile, "corrupt intent"));
	if (scenario == 24) { held = std::make_unique<QLockFile>(intentFile + ".lock"); QVERIFY(held->tryLock(0)); }
	if (scenario == 26) { directory = temporary.filePath("blocked-directory"); QVERIFY(write(directory, "preserve blocker")); }
	if (scenario == 29) n.add(name, "preserve competing root");
	if (scenario == 33) {
		// Dropbox cannot create a second equal path; acknowledge another existing
		// equal-scope root under a changed name to exercise the receipt guard.
		const auto ref = CloudWorkspaceReference::create(provider, Account, epoch, google ? "3" : "id:3");
		n.add(google ? name : "existing-workspace.json", CloudWorkspaceRecord::create(ref, origin).json());
		if (!google) n.metadataResponse = [&](const QByteArray &bytes) { auto m = QJsonDocument::fromJson(bytes).object(); m["name"] = name; return n.json(m); };
	}
	int creates = 0, binds = 0, lookups = 0, reads = 0; bool injected = false;
	n.requested = [&](const QNetworkRequest &r) {
		const auto path = r.url().path(); const auto arg = QJsonDocument::fromJson(r.rawHeader("Dropbox-API-Arg")).object();
		const bool update = path.startsWith("/upload/drive/v2/") || (!google && path == "/2/files/upload" && arg["mode"].isObject());
		if (path == "/drive/v3/files") ++lookups;
		if (update) {
			QCOMPARE(QJsonDocument::fromJson(readFile(intentFile)).object()["state"].toString(), QString("binding"));
			++binds; if (scenario == 5) n.loseWriteAck = true; if (scenario == 16 && !injected) { n.writeFailureStatus = 401; injected = true; }
			if (scenario == 7 && !injected) { n.files.last().rev += "-raced"; injected = true; }
		} else if (path == "/upload/drive/v3/files" || (!google && path == "/2/files/upload")) {
			++creates; const auto state = QJsonDocument::fromJson(readFile(intentFile)).object(); QCOMPARE(state["state"].toString(), QString("submitted"));
			QCOMPARE(state["nonce"].toString(), nonce);
		}
		else {
			++reads; const auto state = QJsonDocument::fromJson(readFile(intentFile)).object(); QVERIFY(!state["id"].toString().isEmpty());
			QVERIFY(state["state"].toString() == "identified" || state["state"].toString() == "binding" || state["state"].toString() == "bound");
			if (scenario == 13 && !injected) { n.deferReads = true; injected = true; }
		}
	};
	n.wrote = [&](const QString &) {
		const bool bound = CloudWorkspaceRecord::parse(n.files.last().data).valid();
		if ((scenario == 10 && !bound) || (scenario == 11 && bound)) n.guard = false;
		if (scenario == 14 && bound) n.deferReads = true;
		if (scenario == 17 && !bound) {
			const auto id = google ? n.files.last().id.mid(3) : n.files.last().id;
			const auto ref = CloudWorkspaceReference::create(provider, Account, epoch, id); QVERIFY(QDir().mkpath(directory)); QVERIFY(write(QDir(directory).filePath(ref.key() + ".json"), "corrupt checkpoint"));
		}
		if ((scenario == 19 || scenario == 20) && !bound) { auto v = QJsonDocument::fromJson(n.files.last().data).object(); v[scenario == 19 ? "format" : "nonce"] = scenario == 19 ? "unknown-bootstrap" : uid(); n.files.last().data = n.json(v); }
		if (scenario == 21 && !bound) { const auto bytes = n.files.last().data; n.files.last().name = "preserved-elsewhere.json"; n.add(name, bytes); }
	};
	CloudWorkspaceCreate client(&n), other(&n); QSignalSpy done(&client, &CloudWorkspaceCreate::finished), errors(&client, &CloudWorkspaceCreate::failed), otherDone(&other, &CloudWorkspaceCreate::finished), otherErrors(&other, &CloudWorkspaceCreate::failed);
	const auto start = [&](CloudWorkspaceCreate &target) { target.start(intentFile, directory, provider, "token", scenario == 25 ? QString("invalid-account") : Account, epoch, nonce, origin, [&] { return n.guard; }); };
	start(client);
	if (scenario == 8) { start(other); QTRY_COMPARE(otherErrors.size(), 1); }
	if (scenario == 12 || scenario == 13 || scenario == 14) {
		QTRY_VERIFY(n.deferred); client.cancel(); if (n.deferred) n.deferred->complete(); n.deferReads = false; QTest::qWait(20); QCOMPARE(done.size() + errors.size(), 0); start(client);
	} else if (scenario == 7 || scenario == 10 || scenario == 11 || scenario == 16) {
		QTRY_COMPARE(errors.size(), 1); n.guard = true; n.writeFailureStatus = 0; n.wrote = {}; start(client);
	}
	const bool success = scenario == 0 || scenario == 5 || scenario == 6 || scenario == 7 || scenario == 8 || scenario == 10 || scenario == 11 || scenario == 13 || scenario == 14 || scenario == 16 || scenario == 18 || scenario == 27 || scenario == 28 || scenario == 30 || (scenario == 29 && google);
	QTRY_COMPARE(done.size() + errors.size(), (scenario == 7 || scenario == 10 || scenario == 11 || scenario == 16) ? 2 : 1); QCOMPARE(done.size(), int(success));
	if (scenario == 15) QVERIFY(errors.first()[1].toBool());
	if (success) {
		CloudWorkspaceRecord root; root.value = done.first()[0].toJsonObject(); QVERIFY(root.valid()); const auto checkpointFile = done.first()[2].toString();
		CloudWorkspaceAuthority authority(checkpointFile, root.reference()); QVERIFY(authority.load()); QCOMPARE(authority.record().json(), root.json());
		const auto state = QJsonDocument::fromJson(readFile(intentFile)).object(); QCOMPARE(state["state"].toString(), QString("bound")); QCOMPARE(state["id"].toString(), root.reference().value["root_id"].toString());
		const auto mutations = n.mutations, requests = reads; const auto checkpoint = readFile(checkpointFile); n.wrote = {};
		if (scenario == 18) QVERIFY(QFile::remove(checkpointFile));
		if (scenario == 27) { CloudWorkspaceCreationIntent original(intentFile, provider, Account, epoch, nonce, origin); QVERIFY(original.prepare()); n.files.last().data = original.bootstrap(); n.files.last().rev += "-reset"; }
		if (scenario == 28) { auto advanced = root; QVERIFY(advanced.registerUpload(uid(), {}, CloudSyncManifest::sha256(A), A.size())); QVERIFY(authority.observe(advanced, "advanced-revision")); }
		if (scenario == 30) n.files.removeLast();
		start(client); QTRY_COMPARE(done.size() + errors.size(), (scenario == 7 || scenario == 10 || scenario == 11 || scenario == 16) ? 3 : 2);
		QCOMPARE(n.mutations, mutations);
		if (scenario == 18 || scenario == 27 || scenario == 28 || scenario == 30) { QCOMPARE(done.size(), 1); if (scenario == 18) QCOMPARE(reads, requests); }
		else { QCOMPARE(done.size(), 2); QCOMPARE(readFile(checkpointFile), checkpoint); }
		if (scenario == 8) { start(other); QTRY_COMPARE(otherDone.size(), 1); QCOMPARE(n.mutations, mutations); }
	}
	if (scenario == 2 || scenario == 3) { const auto mutations = n.mutations; start(client); QTRY_COMPARE(errors.size(), 2); QCOMPARE(n.mutations, mutations); }
	QCOMPARE(lookups, 0); QVERIFY(creates <= 1); if (scenario != 7 && scenario != 16) QVERIFY(binds <= 1); else QCOMPARE(binds, 2);
	if (scenario == 33) { QCOMPARE(binds, 0); QVERIFY(QDir(directory).entryList({"*.json"}, QDir::Files).isEmpty()); }
	for (int i = 0; i < old.size(); ++i) { QCOMPARE(n.files[i].data, old[i].data); QCOMPARE(n.files[i].rev, old[i].rev); QCOMPARE(n.files[i].name, old[i].name); }
	if (scenario == 29) QCOMPARE(n.files[2].data, QByteArray("preserve competing root"));
	QCOMPARE(readFile(selection), QByteArray("preserve selection")); QCOMPARE(readFile(baseline), QByteArray("preserve baseline")); QCOMPARE(readFile(local), A); QVERIFY(n.allowed);
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
void TestCloudSyncHistory::bufferedUpload_data()
{
	QTest::addColumn<bool>("google"); QTest::addColumn<int>("scenario");
	for (bool g : {false, true}) for (int s = 0; s < 12; ++s)
		QTest::newRow(qPrintable(QString("%1-buffered%2").arg(g).arg(s))) << g << s;
}
void TestCloudSyncHistory::bufferedUpload()
{
	QFETCH(bool, google); QFETCH(int, scenario);
	QueuedUploadNetwork network; CloudSyncFileStore store(&network);
	QSignalSpy done(&store, &CloudSyncFileStore::uploadFinished), errors(&store, &CloudSyncFileStore::operationError);
	const auto provider = google ? CloudSyncProviderType::GoogleDrive : CloudSyncProviderType::Dropbox;
	const auto name = CloudHistoryRecord::payloadName(uid(), uid());
	const QByteArray payload(scenario == 8 ? 64 * 1024 : 128 * 1024, 'x');
	const auto start = [&] { store.uploadConditional(provider, "token", name, payload, "expected-revision", google ? "exact-root" : "id:exact-root"); };
	start(); QPointer<Reply> reply = network.last; QVERIFY(reply); QCOMPARE(network.sent, payload);
	QPointer<QTimer> idle = reply->findChild<QTimer *>("neo-cloud-upload-idle");
	if (scenario == 8) {
		QVERIFY(!idle); QCOMPARE(reply->request().transferTimeout(), 30000); reply->complete();
		QCOMPARE(done.size(), 1); QCOMPARE(errors.size(), 0); return;
	}
	QVERIFY(idle); QVERIFY(idle->isActive()); QCOMPARE(idle->interval(), 30000); QCOMPARE(reply->request().transferTimeout(), 0);
	reply->uploadProgress(0, 0); QCOMPARE(idle->interval(), 30000);
	if (scenario == 1) reply->uploadProgress(128, payload.size());
	if (scenario >= 2 && scenario != 9 && scenario != 10 && scenario != 11) {
		if (scenario == 7) { reply->uploadProgress(payload.size(), -1); QCOMPARE(idle->interval(), 30000); }
		reply->uploadProgress(payload.size(), payload.size()); QCOMPARE(idle->interval(), 120000);
	}
	if (scenario == 6) {
		const auto remaining = idle->remainingTime(); QTest::qWait(15);
		reply->uploadProgress(payload.size(), payload.size()); reply->uploadProgress(0, 0); reply->downloadProgress(0, -1);
		QCOMPARE(idle->interval(), 120000); QVERIFY(idle->remainingTime() <= remaining);
	}
	if (scenario == 4) { reply->downloadProgress(1, 2); QCOMPARE(idle->interval(), 30000); }
	if (scenario == 5 || scenario == 10) {
		store.cancel(provider); QVERIFY(!idle->isActive());
		// A queued completion cannot revive a cancelled upload.
		reply->complete(); QTest::qWait(10); QCOMPARE(done.size(), 0); QCOMPARE(errors.size(), 0); return;
	}
	if (scenario == 3 || scenario == 6 || scenario == 7 || scenario == 11) {
		reply->complete(); QVERIFY(!idle->isActive()); QCOMPARE(done.size(), 1); QCOMPARE(errors.size(), 0); return;
	}
	// Accelerate the real expiry callback, preserving the production phase
	// transitions above instead of sleeping for 30 or 120 seconds in CI.
	idle->start(1); QTRY_COMPARE(errors.size(), 1); QCOMPARE(done.size(), 0);
	QCOMPARE(errors.first()[1].toString(), name); QVERIFY(!errors.first()[3].toBool());
	if (scenario == 9) {
		start(); QVERIFY(network.last); network.last->complete();
		QCOMPARE(done.size(), 1); QCOMPARE(errors.size(), 1);
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
