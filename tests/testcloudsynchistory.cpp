// SPDX-License-Identifier: GPL-2.0
#include "testcloudsynchistory.h"
#include "core/cloudsynchistory.h"
#include "core/cloudsyncworkspace.h"
#include "core/cloudsyncworkspacesource.h"
#include "core/cloudsyncworkspacecopy.h"
#include "core/cloudsyncworkspaceacceptance.h"
#include "core/cloudsyncworkspacebinding.h"
#include "core/cloudsyncworkspacereceipt.h"
#include "core/cloudsyncworkspaceupload.h"
#include "core/cloudsyncworkspaceread.h"
#include "core/cloudsyncworkspaceapply.h"
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
#include <QSaveFile>
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
	bool workspaceUploads = false;
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
				((workspaceCopies || workspaceUploads) && name.startsWith("subsurface-neo-workspace-") && name.endsWith(".xml")) || (!create && index < 0);
			const bool root = name.endsWith(".json");
			if (!root && (create || index >= 0)) {
				if ((workspaceCopies || workspaceUploads) && name.startsWith("subsurface-neo-workspace-")) {
					const auto ticket = name.chopped(4).right(36); bool registered = false;
					for (const auto &f : files) {
						const auto control = CloudWorkspaceRecord::parse(f.data); if (!control.valid() || control.reference().payloadName(ticket) != name) continue;
						const auto t = control.ticket(ticket); registered |= t["state"] == QJsonValue("registered") && t["sha256"] == QJsonValue(CloudSyncManifest::sha256(data)) &&
							(workspaceUploads ? t["bytes"].toDouble() == data.size() : control.copySource(ticket).valid());
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
void TestCloudSyncHistory::workspaceLegacyAcceptance_data()
{
	QTest::addColumn<bool>("google"); QTest::addColumn<int>("mode");
	const QStringList cases = {"accept-and-aba", "stale-acceptance", "stale-head", "foreign-source", "unknown-copy", "reused-event", "pending-copy",
		"removed-event", "changed-event", "reordered-events", "downgrade", "wrong-parent", "unknown-field", "foreign-head", "unknown-ticket", "checkpoint-rollback"};
	for (bool g : {false, true}) for (int m = 0; m < cases.size(); ++m) QTest::newRow(qPrintable(QString("%1-legacy-acceptance-%2").arg(g).arg(cases[m]))) << g << m;
}
void TestCloudSyncHistory::workspaceLegacyAcceptance()
{
	QFETCH(bool, google); QFETCH(int, mode); Network n; n.seed();
	const auto sourceA = copySource(n, google); QVERIFY(sourceA.valid());
	const auto provider = QString(google ? "google-drive" : "dropbox");
	const auto ref = CloudWorkspaceReference::create(provider, Account, uid(), google ? "accept-root" : "id:accept-root");
	auto root = CloudWorkspaceRecord::create(ref, CloudSyncManifest::sha256("genesis"));
	const auto copyA = uid(), eventA = uid(), copyB = uid(), eventB = uid(), eventAgain = uid();
	QVERIFY(root.registerCopy(copyA, {}, sourceA)); const auto registered = root;
	if (mode == 6) { QVERIFY(!root.acceptLegacyCopy(eventA, {}, {}, copyA, sourceA)); QCOMPARE(root.json(), registered.json()); return; }
	QVERIFY(root.completeUpload(copyA, google ? "copy-a" : "id:copy-a", "copy-rev-a")); QVERIFY(root.publish(copyA, {}));
	const auto beforeAcceptance = root;
	if (mode == 1 || mode == 2 || mode == 3 || mode == 4) {
		auto observed = sourceA; if (mode == 3) observed.value["account"] = CloudSyncManifest::sha256("other-account");
		QVERIFY(!root.acceptLegacyCopy(eventA, mode == 1 ? uid() : QString(), mode == 2 ? uid() : root.head(), mode == 4 ? uid() : copyA, observed));
		QCOMPARE(root.json(), beforeAcceptance.json()); return;
	}
	QVERIFY(root.acceptLegacyCopy(eventA, {}, copyA, copyA, sourceA)); const auto acceptedA = root;
	QVERIFY(root.valid()); QVERIFY(root.follows(beforeAcceptance)); QVERIFY(!beforeAcceptance.follows(root));
	QCOMPARE(root.latestLegacyAcceptance().value("id").toString(), eventA);
	QVERIFY(!CloudHistoryRecord::parse(root.json()).valid()); QCOMPARE(CloudWorkspaceRecord::parse(root.json()).json(), root.json());
	if (mode == 5) { QVERIFY(!root.acceptLegacyCopy(eventA, eventA, copyA, copyA, sourceA)); QCOMPARE(root.json(), acceptedA.json()); return; }
	n.files[0].rev = "new-legacy-revision"; const auto sourceB = copySource(n, google); QVERIFY(sourceB.valid()); QVERIFY(sourceB.json() != sourceA.json());
	QVERIFY(root.registerCopy(copyB, copyA, sourceB)); QVERIFY(root.completeUpload(copyB, google ? "copy-b" : "id:copy-b", "copy-rev-b")); QVERIFY(root.publish(copyB, copyA));
	// Merely protecting another branch cannot silently accept it.
	QCOMPARE(root.latestLegacyAcceptance().value("id").toString(), eventA); QVERIFY(root.follows(acceptedA));
	QVERIFY(root.acceptLegacyCopy(eventB, eventA, copyB, copyB, sourceB)); const auto acceptedB = root;
	QVERIFY(root.acceptLegacyCopy(eventAgain, eventB, copyB, copyA, sourceA)); const auto acceptedAgain = root;
	QVERIFY(root.follows(acceptedB)); QVERIFY(root.follows(acceptedA)); QVERIFY(!acceptedB.follows(root));
	QCOMPARE(root.latestLegacyAcceptance().value("ticket").toString(), copyA);
	if (mode == 15) {
		QTemporaryDir dir; CloudWorkspaceAuthority checkpoint(dir.filePath("root.json"), ref);
		QVERIFY(checkpoint.observe(root, "latest")); QVERIFY(!checkpoint.observe(acceptedB, "older", true));
		CloudWorkspaceAuthority restarted(dir.filePath("root.json"), ref); QVERIFY(restarted.load()); QCOMPARE(restarted.record().json(), root.json()); return;
	}
	if (mode == 0) return;
	auto events = root.value["legacy_acceptances"].toArray();
	if (mode == 7) events.removeLast();
	if (mode == 8) { auto event = events[0].toObject(); event["head"] = copyB; events[0] = event; }
	if (mode == 9) { auto first = events[0]; events[0] = events[1]; events[1] = first; }
	if (mode == 10) { root.value.remove("legacy_acceptances"); root.value["format"] = "neo-workspace-history-development-2"; QVERIFY(root.valid()); QVERIFY(!root.follows(acceptedAgain)); return; }
	if (mode == 11) { auto event = events[1].toObject(); event["parent"] = uid(); events[1] = event; }
	if (mode == 12) { auto event = events[1].toObject(); event["unknown"] = true; events[1] = event; }
	if (mode == 13) { auto event = events[1].toObject(); event["head"] = uid(); events[1] = event; }
	if (mode == 14) { auto event = events[1].toObject(); event["ticket"] = uid(); events[1] = event; }
	root.value["legacy_acceptances"] = events;
	if (mode == 7 || mode == 8) { QVERIFY(root.valid()); QVERIFY(!root.follows(acceptedAgain)); }
	else QVERIFY(!root.valid());
}
void TestCloudSyncHistory::workspaceAcceptanceIntent_data()
{
	QTest::addColumn<bool>("google"); QTest::addColumn<int>("mode");
	const QStringList cases = {"restart", "different-choice", "different-source", "different-event", "different-previous", "different-head", "different-copy", "corrupt", "locked", "future-state", "different-local", "unknown-neighbor"};
	for (bool g : {false, true}) for (int m = 0; m < cases.size(); ++m) QTest::newRow(qPrintable(QString("%1-accept-intent-%2").arg(g).arg(cases[m]))) << g << m;
}
void TestCloudSyncHistory::workspaceAcceptanceIntent()
{
	QFETCH(bool, google); QFETCH(int, mode); Network n; n.seed(); const auto source = copySource(n, google);
	const auto ref = CloudWorkspaceReference::create(google ? "google-drive" : "dropbox", Account, uid(), google ? "root" : "id:root");
	const auto choice = uid(), event = uid(), previous = uid(), head = uid(), copy = uid(); QTemporaryDir dir; QVERIFY(dir.isValid());
	const auto file = dir.filePath("acceptance/intent.json"); CloudWorkspaceAcceptanceIntent intent(file, ref, choice, source, CloudSyncManifest::sha256(B), event, previous, head, copy);
	if (mode == 11) {
		QVERIFY(QDir().mkpath(QFileInfo(file).absolutePath())); QFile neighbor(dir.filePath("acceptance/unknown"));
		QVERIFY(neighbor.open(QIODevice::WriteOnly)); neighbor.write("preserve"); neighbor.close(); QVERIFY(!intent.prepare());
		QVERIFY(neighbor.open(QIODevice::ReadOnly)); QCOMPARE(neighbor.readAll(), QByteArray("preserve")); return;
	}
	QVERIFY(intent.prepare()); QCOMPARE(intent.state(), QString("prepared")); const auto original = intent.proof();
	if ((mode >= 1 && mode <= 6) || mode == 10) {
		auto otherSource = source; if (mode == 2) { auto m = otherSource.value["manifest"].toObject(); m["version"] = "other-revision"; otherSource.value["manifest"] = m; }
		CloudWorkspaceAcceptanceIntent other(file, ref, mode == 1 ? uid() : choice, otherSource, CloudSyncManifest::sha256(mode == 10 ? A : B), mode == 3 ? uid() : event,
			mode == 4 ? uid() : previous, mode == 5 ? uid() : head, mode == 6 ? uid() : copy);
		QVERIFY(!other.prepare()); QVERIFY(!other.inspect()); QVERIFY(intent.inspect()); QCOMPARE(intent.proof(), original); return;
	}
	if (mode == 7 || mode == 9) {
		QFile f(file); QVERIFY(f.open(QIODevice::WriteOnly | QIODevice::Truncate));
		if (mode == 7) f.write("corrupt"); else { auto body = original; body["state"] = "future"; f.write(QJsonDocument(body).toJson(QJsonDocument::Compact)); }
		f.close(); QVERIFY(!intent.prepare()); QVERIFY(!intent.inspect()); return;
	}
	if (mode == 8) { QLockFile lock(file + ".lock"); QVERIFY(lock.tryLock(0)); QVERIFY(!intent.submitted()); QVERIFY(intent.inspect()); QCOMPARE(intent.proof(), original); return; }
	QVERIFY(!intent.committed()); QVERIFY(intent.submitted()); QVERIFY(intent.submitted()); QVERIFY(intent.committed()); QVERIFY(intent.committed()); QVERIFY(intent.completed()); QVERIFY(intent.completed());
	CloudWorkspaceAcceptanceIntent restarted(file, ref, choice, source, CloudSyncManifest::sha256(B), event, previous, head, copy);
	QVERIFY(restarted.inspect()); QCOMPARE(restarted.state(), QString("completed"));
	QFile f(file); QVERIFY(f.open(QIODevice::ReadOnly)); QVERIFY(!f.readAll().contains("access_token"));
}

void TestCloudSyncHistory::workspaceAcceptanceWorkflow_data()
{
	QTest::addColumn<bool>("google"); QTest::addColumn<int>("mode");
	const QStringList cases = {"accept-restart", "scope-before", "missing-choice", "choice-changed", "missing-checkpoint", "stale-head", "stale-acceptance", "source-changed-before", "damaged-copy", "ack-lost", "source-changed-after", "cancel-read", "token-rejected", "later-acceptance", "corrupt-intent", "choice-away-back", "cancel-after-write-resume", "completed-source-changed", "foreign-copy", "write-conflict-resume"};
	for (bool g : {false, true}) for (int m = 0; m < cases.size(); ++m) QTest::newRow(qPrintable(QString("%1-accept-workflow-%2").arg(g).arg(cases[m]))) << g << m;
}
void TestCloudSyncHistory::workspaceAcceptanceWorkflow()
{
	QFETCH(bool, google); QFETCH(int, mode); Network n; n.seed(); n.workspaceWrites = true;
	const auto source = copySource(n, google); const auto provider = QString(google ? "google-drive" : "dropbox");
	const auto ref = CloudWorkspaceReference::create(provider, Account, uid(), google ? "3" : "id:3");
	const auto copy = uid(), event = uid(); auto root = CloudWorkspaceRecord::create(ref, CloudSyncManifest::sha256("genesis"));
	QVERIFY(root.registerCopy(copy, {}, source)); QVERIFY(root.completeUpload(copy, google ? "4" : "id:4", "rev-4")); QVERIFY(root.publish(copy, {}));
	n.add(ref.rootName(), root.json()); n.add(ref.payloadName(copy), A); const auto original = n.files;
	QTemporaryDir dir; QVERIFY(dir.isValid()); const auto choiceFile = dir.filePath("choice.json"), checkpoint = dir.filePath("checkpoint.json"), intent = dir.filePath("acceptance/intent.json");
	CloudWorkspaceSelection choice(choiceFile, provider, Account); QVERIFY(choice.select({}, ref)); const auto choiceVersion = choice.version();
	CloudWorkspaceAuthority authority(checkpoint, ref); QVERIFY(authority.observe(root, "rev-3"));
	if (mode == 1) n.guard = false; if (mode == 2) QVERIFY(QFile::remove(choiceFile)); if (mode == 4) QVERIFY(QFile::remove(checkpoint));
	if (mode == 7) n.files[0].rev += "-changed"; if (mode == 8) n.files[3].data = B;
	if (mode == 9) n.loseWriteAck = true; if (mode == 12) n.writeFailureStatus = 401; if (mode == 19) n.fault = 4;
	if (mode == 14) { QVERIFY(QDir().mkpath(QFileInfo(intent).absolutePath())); QFile f(intent); QVERIFY(f.open(QIODevice::WriteOnly)); f.write("unknown intent"); f.close(); }
	bool changed = false, mutatedChoice = false;
	const auto changeChoice = [&](bool back) {
		CloudWorkspaceSelection c(choiceFile, provider, Account); QVERIFY(c.load());
		const auto other = CloudWorkspaceReference::create(provider, Account, uid(), google ? "other" : "id:other");
		QVERIFY(c.select(c.version(), other)); if (back) QVERIFY(c.select(c.version(), ref));
	};
	n.requested = [&](const QNetworkRequest &) { if ((mode == 3 || mode == 15) && !mutatedChoice) { mutatedChoice = true; changeChoice(mode == 15); } };
	n.wrote = [&](const QString &name) {
		if (name != ref.rootName()) return;
		if (mode == 10) { n.files[0].rev += "-later"; changed = true; }
		if (mode == 13) {
			auto later = CloudWorkspaceRecord::parse(n.files[2].data); const auto next = uid();
			QVERIFY(later.acceptLegacyCopy(next, event, copy, copy, source)); n.files[2].data = later.json(); n.files[2].rev += "-later";
		}
		if (mode == 16) n.deferReads = true;
	};
	if (mode == 11) n.deferReads = true;
	CloudWorkspaceAcceptance client(&n); QSignalSpy done(&client, &CloudWorkspaceAcceptance::finished), errors(&client, &CloudWorkspaceAcceptance::failed);
	const auto start = [&] { client.start("token", ref, source, choiceFile, choiceVersion, checkpoint, intent, CloudSyncManifest::sha256(B), event,
		mode == 6 ? uid() : QString(), mode == 5 ? uid() : copy, mode == 18 ? uid() : copy, [&] { return n.guard; }); };
	start();
	if (mode == 11 || mode == 16) {
		QTRY_VERIFY(n.deferred); client.cancel(); if (n.deferred) n.deferred->complete(); n.deferReads = false; QTest::qWait(20);
		QCOMPARE(done.size() + errors.size(), 0);
		if (mode == 16) { n.wrote = {}; const auto writes = n.mutations; start(); QTRY_COMPARE(done.size() + errors.size(), 1); QCOMPARE(done.size(), 1); QCOMPARE(n.mutations, writes); }
	} else {
		QTRY_COMPARE(done.size() + errors.size(), 1);
		const bool success = mode == 0 || mode == 9 || mode == 10 || mode == 13 || mode == 17;
		QCOMPARE(done.size(), int(success));
	}
	if (mode == 19) {
		const auto writes = n.mutations; start(); QTRY_COMPARE(done.size(), 1); QCOMPARE(n.mutations, writes + 1);
	}
	if (!done.isEmpty()) {
		const auto observed = CloudWorkspaceRecord::parse(n.files[2].data); QVERIFY(observed.valid());
		QCOMPARE(done.last()[2].toString(), event); QCOMPARE(done.last()[3].toBool(), mode != 13); QCOMPARE(done.last()[4].toBool(), mode == 10);
		QCOMPARE(observed.head(), copy); QCOMPARE(observed.copySource(copy).json(), source.json());
		if (mode == 0 || mode == 17) {
			const auto writes = n.mutations;
			if (mode == 17) { n.files[0].rev += "-later"; changed = true; }
			start(); QTRY_COMPARE(done.size(), 2); QCOMPARE(n.mutations, writes);
			if (mode == 17) QVERIFY(done.last()[4].toBool());
		}
	}
	if (mode == 12) QVERIFY(errors.first()[1].toBool());
	QCOMPARE(n.files[1].data, original[1].data); QCOMPARE(n.files[3].data, mode == 8 ? B : original[3].data);
	if (!changed && mode != 7) QCOMPARE(n.files[0].data, original[0].data);
	QCOMPARE(n.renames, 0); QVERIFY(n.allowed);
}

void TestCloudSyncHistory::workspaceAbsenceWorkflow_data()
{
	QTest::addColumn<bool>("google"); QTest::addColumn<int>("mode");
	const QStringList cases = {"accept-restart", "source-present-before", "source-appears-after-write", "lost-write-ack",
		"choice-changed", "stale-head", "stale-acceptance", "token-rejected", "cancel-read", "cancel-after-write-resume",
		"missing-checkpoint", "corrupt-intent", "scope-changed", "source-appears-before-write"};
	for (bool g : {false, true}) for (int m = 0; m < cases.size(); ++m)
		QTest::newRow(qPrintable(QString("%1-absence-workflow-%2").arg(g).arg(cases[m]))) << g << m;
}
void TestCloudSyncHistory::workspaceAbsenceWorkflow()
{
	QFETCH(bool, google); QFETCH(int, mode); Network n; n.seed(); n.workspaceWrites = true;
	const auto manifest = n.files[0], payload = n.files[1]; n.files.clear();
	const auto provider = QString(google ? "google-drive" : "dropbox");
	const auto ref = CloudWorkspaceReference::create(provider, Account, uid(), google ? "3" : "id:3");
	const auto head = uid(), event = uid(); auto root = CloudWorkspaceRecord::create(ref, CloudSyncManifest::sha256("genesis"));
	QVERIFY(root.registerUpload(head, {}, CloudSyncManifest::sha256(A), A.size()));
	QVERIFY(root.completeUpload(head, google ? "4" : "id:4", "rev-4")); QVERIFY(root.publish(head, {}));
	n.add(ref.rootName(), root.json()); n.add(ref.payloadName(head), A);
	const auto addSource = [&] { n.add(manifest.name, manifest.data); n.add(payload.name, payload.data); };
	if (mode == 1) addSource();
	QTemporaryDir dir; QVERIFY(dir.isValid()); const auto choiceFile = dir.filePath("choice.json"), checkpoint = dir.filePath("checkpoint.json"), intent = dir.filePath("absence/intent.json");
	CloudWorkspaceSelection choice(choiceFile, provider, Account); QVERIFY(choice.select({}, ref)); const auto choiceVersion = choice.version();
	CloudWorkspaceAuthority authority(checkpoint, ref); QVERIFY(authority.observe(root, "rev-3"));
	if (mode == 3) n.loseWriteAck = true;
	if (mode == 4) { const auto other = CloudWorkspaceReference::create(provider, Account, uid(), google ? "other" : "id:other"); QVERIFY(choice.select(choiceVersion, other)); }
	if (mode == 7) n.writeFailureStatus = 401;
	if (mode == 8) n.deferReads = true;
	if (mode == 10) QVERIFY(QFile::remove(checkpoint));
	if (mode == 11) { QVERIFY(QDir().mkpath(QFileInfo(intent).absolutePath())); QFile f(intent); QVERIFY(f.open(QIODevice::WriteOnly)); QVERIFY(f.write("unknown") == 7); f.close(); }
	bool added = false; int manifestAliases = 0;
	n.requested = [&](const QNetworkRequest &r) {
		if (mode == 12) n.guard = false;
		QString alias;
		if (google && r.url().path() == "/drive/v3/files") alias = QUrlQuery(r.url()).queryItemValue("q").section('\'', 1, 1);
		else if (!google) alias = QJsonDocument::fromJson(r.rawHeader("Dropbox-API-Arg")).object()["path"].toString().mid(1);
		if (alias == Legacy) ++manifestAliases;
		if (mode == 13 && manifestAliases >= 2 && !added && alias != Legacy) { addSource(); added = true; }
	};
	n.wrote = [&](const QString &name) {
		if (name != ref.rootName()) return;
		if (mode == 2 && !added) { addSource(); added = true; }
		if (mode == 9) n.deferReads = true;
	};
	CloudWorkspaceAcceptance client(&n); QSignalSpy done(&client, &CloudWorkspaceAcceptance::finished), errors(&client, &CloudWorkspaceAcceptance::failed);
	const auto start = [&] { client.startMissing("token", ref, choiceFile, choiceVersion, checkpoint, intent, CloudSyncManifest::sha256(A), event,
		mode == 6 ? uid() : QString(), mode == 5 ? uid() : head, [&] { return n.guard; }); };
	start();
	if (mode == 8 || mode == 9) {
		QTRY_VERIFY(n.deferred); client.cancel(); if (n.deferred) n.deferred->complete(); n.deferReads = false; QTest::qWait(20);
		QCOMPARE(done.size() + errors.size(), 0);
		if (mode == 9) { n.wrote = {}; const auto writes = n.mutations; start(); QTRY_COMPARE(done.size() + errors.size(), 1); QCOMPARE(done.size(), 1); QCOMPARE(n.mutations, writes); }
	} else {
		QTRY_COMPARE(done.size() + errors.size(), 1);
		QCOMPARE(done.size(), int(mode == 0 || mode == 2 || mode == 3 || mode == 13));
	}
	if (!done.isEmpty()) {
		const auto observed = CloudWorkspaceRecord::parse(n.files[0].data); QVERIFY(observed.valid());
		QCOMPARE(observed.head(), head); QCOMPARE(observed.latestLegacyAcceptance()["id"].toString(), event);
		QCOMPARE(observed.latestLegacyAcceptance()["kind"].toString(), QString("absent"));
		QCOMPARE(done.last()[2].toString(), event); QVERIFY(done.last()[3].toBool()); QCOMPARE(done.last()[4].toBool(), mode == 2 || mode == 13);
		if (mode == 0) { const auto writes = n.mutations; start(); QTRY_COMPARE(done.size(), 2); QCOMPARE(n.mutations, writes); }
	}
	if (mode == 7) QVERIFY(errors.first()[1].toBool());
	QCOMPARE(n.files[1].data, A); QCOMPARE(n.renames, 0); QVERIFY(n.allowed);
}

void TestCloudSyncHistory::workspaceBinding_data()
{
	QTest::addColumn<bool>("google"); QTest::addColumn<int>("mode");
	const QStringList cases = {"activate-advance-restart", "foreign-choice", "foreign-root", "no-acceptance", "wrong-raw", "different-initial-model",
		"invalid-xml", "corrupt-binding", "locked-binding", "wrong-revision", "missing-upload-proof", "uncommitted-upload-proof",
		"wrong-upload-identity", "legacy-changed", "checkpoint-rollback", "unknown-baseline", "different-upload-model", "existing-binding-preserved", "remote-advanced-before-own-upload", "stale-activation-checkpoint",
		"lost-binding-ack", "lost-ack-later-local-edit", "lost-ack-later-remote-head", "lost-ack-corrupt-previous-pin"};
	for (bool g : {false, true}) for (int m = 0; m < cases.size(); ++m) QTest::newRow(qPrintable(QString("%1-binding-%2").arg(g).arg(cases[m]))) << g << m;
}
void TestCloudSyncHistory::workspaceBinding()
{
	QFETCH(bool, google); QFETCH(int, mode); Network n; n.seed(); const auto source = copySource(n, google);
	const auto provider = QString(google ? "google-drive" : "dropbox"), choice = uid(), copy = uid(), event = uid(), revision = uid();
	const auto ref = CloudWorkspaceReference::create(provider, Account, uid(), google ? "root" : "id:root");
	auto root = CloudWorkspaceRecord::create(ref, CloudSyncManifest::sha256("genesis"));
	QVERIFY(root.registerCopy(copy, {}, source)); QVERIFY(root.completeUpload(copy, google ? "object-a" : "id:object-a", "rev-a")); QVERIFY(root.publish(copy, {}));
	const auto withoutAcceptance = root; QVERIFY(root.acceptLegacyCopy(event, {}, copy, copy, source)); const auto baseline = root;
	QTemporaryDir dir; QVERIFY(dir.isValid()); const auto file = dir.filePath("binding.json"); CloudWorkspaceBinding binding(file, ref, choice);
	const auto readBinding = [](const QString &path) { QFile f(path); return f.open(QIODevice::ReadOnly) ? f.readAll() : QByteArray(); };
	if (mode == 3) { QVERIFY(!binding.activate(withoutAcceptance, withoutAcceptance, A, A, revision)); return; }
	if (mode == 19) { QVERIFY(!binding.activate(root, withoutAcceptance, A, A, revision)); return; }
	if (mode == 4 || mode == 5 || mode == 6) {
		const auto raw = mode == 4 ? B : mode == 6 ? QByteArray("bad xml") : A;
		const auto model = mode == 5 ? B : raw; QVERIFY(!binding.activate(root, root, raw, model, revision)); return;
	}
	if (mode == 7) { QFile f(file); QVERIFY(f.open(QIODevice::WriteOnly)); f.write("unknown"); f.close(); QVERIFY(!binding.activate(root, root, A, A, revision)); return; }
	if (mode == 8) { QLockFile lock(file + ".lock"); QVERIFY(lock.tryLock(0)); QVERIFY(!binding.activate(root, root, A, A, revision)); return; }
	QVERIFY(binding.activate(root, root, A, A, revision)); QCOMPARE(binding.commit(), copy); QCOMPARE(binding.acceptance(), event); QVERIFY(binding.matchesModel(A)); QVERIFY(!binding.matchesModel(B));
	const auto saved = binding.value();
	if (mode == 1 || mode == 2) {
		const auto other = mode == 2 ? CloudWorkspaceReference::create(provider, Account, uid(), google ? "other" : "id:other") : ref;
		CloudWorkspaceBinding foreign(file, other, mode == 1 ? uid() : choice); QVERIFY(!foreign.load()); QVERIFY(!foreign.activate(root, root, A, A, uid())); QCOMPARE(binding.value(), saved); return;
	}
	if (mode == 17) { QVERIFY(!binding.activate(root, root, A, A, uid())); QCOMPARE(binding.value(), saved); return; }
	QString uploadParent = copy;
	if (mode == 18) {
		const auto other = uid(); QVERIFY(root.registerUpload(other, copy, CloudSyncManifest::sha256(A), A.size()));
		QVERIFY(root.completeUpload(other, google ? "object-other" : "id:object-other", "rev-other")); QVERIFY(root.publish(other, copy)); uploadParent = other;
	}
	const auto upload = uid(), nextRevision = uid(); QVERIFY(root.registerUpload(upload, uploadParent, CloudSyncManifest::sha256(B), B.size()));
	const auto object = QString(google ? "object-b" : "id:object-b"); QVERIFY(root.completeUpload(upload, object, "rev-b")); QVERIFY(root.publish(upload, uploadParent));
	const auto proofFile = dir.filePath("upload.json"); CloudWorkspaceUploadIntent proof(proofFile, ref, CloudSyncManifest::sha256(B), B.size(), upload, uploadParent);
	QVERIFY(proof.prepare()); QVERIFY(proof.beginRegistration()); QVERIFY(proof.registerTicket()); QVERIFY(proof.submit());
	QVERIFY(proof.identify(mode == 12 ? (google ? "foreign" : "id:foreign") : object)); QVERIFY(proof.verify("rev-b")); QVERIFY(proof.upload());
	if (mode != 11) QVERIFY(proof.commit());
	if (mode == 13) QVERIFY(root.acceptLegacyCopy(uid(), event, upload, copy, source));
	if (mode == 15) { auto journal = root.value["journal"].toObject(); auto tickets = journal["tickets"].toObject(); auto old = tickets[copy].toObject(); old["sha256"] = CloudSyncManifest::sha256(B); tickets[copy] = old; journal["tickets"] = tickets; root.value["journal"] = journal; }
	const bool ok = binding.advanceOwnedUpload(mode == 9 ? uid() : revision, root, mode == 14 ? withoutAcceptance : baseline, B,
		mode == 16 ? A : B, mode == 10 ? dir.filePath("missing.json") : proofFile, nextRevision);
	if (mode == 0 || mode >= 20) {
		QVERIFY(ok); QCOMPARE(binding.commit(), upload); QCOMPARE(binding.revision(), nextRevision); QVERIFY(binding.matchesModel(B));
		CloudWorkspaceBinding restarted(file, ref, choice); QVERIFY(restarted.load()); QCOMPARE(restarted.value(), binding.value());
		QVERIFY(!restarted.advanceOwnedUpload(revision, root, baseline, B, B, proofFile, uid()));
		const auto written = readBinding(file);
		if (mode == 20) QVERIFY(restarted.advanceOwnedUpload(revision, root, baseline, B, B, proofFile, nextRevision));
		if (mode == 21) QVERIFY(!restarted.advanceOwnedUpload(revision, root, baseline, B, A, proofFile, nextRevision));
		if (mode == 22) {
			const auto later = uid(); QVERIFY(root.registerUpload(later, upload, CloudSyncManifest::sha256(A), A.size()));
			QVERIFY(root.completeUpload(later, google ? "object-later" : "id:object-later", "rev-later")); QVERIFY(root.publish(later, upload));
			QVERIFY(!restarted.advanceOwnedUpload(revision, root, baseline, B, B, proofFile, nextRevision));
		}
		QCOMPARE(readBinding(file), written);
		if (mode == 23) {
			auto corrupted = QJsonDocument::fromJson(written).object(); auto previous = corrupted["previous"].toObject();
			auto raw = previous["raw"].toObject(); raw["name"] = "foreign"; previous["raw"] = raw; corrupted["previous"] = previous;
			QFile f(file); QVERIFY(f.open(QIODevice::WriteOnly | QIODevice::Truncate)); QVERIFY(f.write(QJsonDocument(corrupted).toJson()) > 0); f.close();
			CloudWorkspaceBinding invalid(file, ref, choice); QVERIFY(!invalid.load());
			QVERIFY(!invalid.advanceOwnedUpload(revision, root, baseline, B, B, proofFile, nextRevision));
		}
	} else { QVERIFY(!ok); CloudWorkspaceBinding restarted(file, ref, choice); QVERIFY(restarted.load()); QCOMPARE(restarted.value(), saved); }
}

void TestCloudSyncHistory::workspaceLegacyAbsence_data()
{
	QTest::addColumn<bool>("google"); QTest::addColumn<int>("mode");
	const QStringList cases = {"empty-legacy-copy-and-return", "stale-head", "stale-acceptance", "reused-event-id",
		"unknown-event-field", "unknown-event-kind", "invalid-event-head", "changed-event", "downgrade", "missing-copies", "empty-workspace",
		"upgrade-accepted-copy", "upgrade-unaccepted-copy", "upgrade-without-absence"};
	for (bool g : {false, true}) for (int m = 0; m < cases.size(); ++m)
		QTest::newRow(qPrintable(QString("%1-legacy-absence-%2").arg(g).arg(cases[m]))) << g << m;
}
void TestCloudSyncHistory::workspaceLegacyAbsence()
{
	QFETCH(bool, google); QFETCH(int, mode); Network n; n.seed(); const auto source = copySource(n, google);
	const auto provider = QString(google ? "google-drive" : "dropbox");
	const auto ref = CloudWorkspaceReference::create(provider, Account, uid(), google ? "root" : "id:root");
	auto root = CloudWorkspaceRecord::create(ref, CloudSyncManifest::sha256("genesis"));
	if (mode == 10) { QVERIFY(!root.acceptLegacyAbsence(uid(), {}, {})); return; }
	const auto baseline = uid(); QVERIFY(root.registerUpload(baseline, {}, CloudSyncManifest::sha256(A), A.size()));
	QVERIFY(root.completeUpload(baseline, google ? "object-a" : "id:object-a", "rev-a")); QVERIFY(root.publish(baseline, {}));
	if (mode == 11 || mode == 12 || mode == 13) {
		const auto copy = uid(), accepted = uid(); QVERIFY(root.registerCopy(copy, baseline, source));
		QVERIFY(root.completeUpload(copy, google ? "object-copy" : "id:object-copy", "rev-copy")); QVERIFY(root.publish(copy, baseline));
		if (mode == 11 || mode == 13) QVERIFY(root.acceptLegacyCopy(accepted, {}, copy, copy, source));
		if (mode == 13) { auto forged = root; forged.value["format"] = "neo-workspace-history-development-4"; QVERIFY(forged.valid()); QVERIFY(!forged.follows(root)); return; }
		const auto old = root; QVERIFY(root.acceptLegacyAbsence(uid(), mode == 11 ? accepted : QString(), copy));
		QVERIFY(root.follows(old)); QCOMPARE(root.value["format"].toString(), QString("neo-workspace-history-development-4"));
		QCOMPARE(root.copySource(copy).json(), source.json()); return;
	}
	const auto before = root; const auto event = uid();
	if (mode == 1 || mode == 2) {
		QVERIFY(!root.acceptLegacyAbsence(event, mode == 2 ? uid() : QString(), mode == 1 ? uid() : baseline)); QCOMPARE(root.json(), before.json()); return;
	}
	QVERIFY(root.acceptLegacyAbsence(event, {}, baseline)); QVERIFY(root.valid()); QVERIFY(root.follows(before));
	QCOMPARE(root.value["format"].toString(), QString("neo-workspace-history-development-4")); QCOMPARE(root.latestLegacyAcceptance()["kind"].toString(), QString("absent"));
	QVERIFY(CloudWorkspaceRecord::parse(root.json()).valid());
	if (mode == 3) { const auto saved = root.json(); QVERIFY(!root.acceptLegacyAbsence(event, event, baseline)); QCOMPARE(root.json(), saved); return; }
	if (mode >= 4 && mode <= 9) {
		auto bad = root;
		if (mode == 9) bad.value.remove("copies");
		else if (mode == 8) bad.value["format"] = "neo-workspace-history-development-3";
		else {
			auto events = bad.value["legacy_acceptances"].toArray(); auto entry = events[0].toObject();
			if (mode == 4) entry["unexpected"] = true;
			if (mode == 5) entry["kind"] = "unknown";
			if (mode == 6) entry["head"] = uid();
			if (mode == 7) entry["parent"] = uid();
			events[0] = entry; bad.value["legacy_acceptances"] = events;
		}
		QVERIFY(!bad.valid()); QVERIFY(!bad.follows(root)); return;
	}
	const auto first = root; const auto copy = uid(), accepted = uid();
	QVERIFY(root.registerCopy(copy, baseline, source)); QVERIFY(root.completeUpload(copy, google ? "object-b" : "id:object-b", "rev-b")); QVERIFY(root.publish(copy, baseline));
	QVERIFY(root.acceptLegacyCopy(accepted, event, copy, copy, source)); QVERIFY(root.follows(first)); QCOMPARE(root.value["format"].toString(), QString("neo-workspace-history-development-4"));
	const auto second = root; const auto missingAgain = uid(); QVERIFY(root.acceptLegacyAbsence(missingAgain, accepted, copy)); QVERIFY(root.follows(second));
	QCOMPARE(root.latestLegacyAcceptance()["parent"].toString(), accepted); QCOMPARE(root.latestLegacyAcceptance()["kind"].toString(), QString("absent"));
	auto changed = root; auto events = changed.value["legacy_acceptances"].toArray(); auto firstEvent = events[0].toObject();
	firstEvent["head"] = copy; events[0] = firstEvent; changed.value["legacy_acceptances"] = events;
	QVERIFY(!changed.follows(first));
	changed = root; events = changed.value["legacy_acceptances"].toArray(); auto lastEvent = events.last().toObject();
	lastEvent["head"] = baseline; events[events.size() - 1] = lastEvent; changed.value["legacy_acceptances"] = events;
	QVERIFY(!changed.valid());
}

void TestCloudSyncHistory::workspaceReceiveReceipt_data()
{
	QTest::addColumn<bool>("google"); QTest::addColumn<int>("mode");
	const QStringList cases = {"durable-receive", "different-local-model", "wrong-raw", "wrong-choice", "wrong-revision",
		"wrong-reader-reservation", "unverified-reader", "released-reservation", "later-head", "unexpected-recovery-file",
		"corrupt-recovery-file", "wrong-apply-observation", "wrong-persist-observation", "changed-legacy-acceptance",
		"stale-checkpoint", "prepared-recovery", "locked-receipt", "advance-persisted-and-replay",
		"advance-wrong-revision", "advance-released-reader", "advance-with-later-head",
		"advance-changed-acceptance", "advance-corrupt-recovery", "advance-not-persisted", "advance-released-reservation", "advance-later-local-edit",
		"advance-different-recovery-revision", "receive-revision-reuse", "owned-verified", "owned-releasing",
		"owned-released", "released-reader-active-root", "verified-reader-released-root", "foreign-reader",
		"owned-corrupt-receipt", "owned-after-legacy-change", "owned-after-later-head"};
	for (bool g : {false, true}) for (int m = 0; m < cases.size(); ++m)
		QTest::newRow(qPrintable(QString("%1-receive-receipt-%2").arg(g).arg(cases[m]))) << g << m;
}
void TestCloudSyncHistory::workspaceReceiveReceipt()
{
	QFETCH(bool, google); QFETCH(int, mode); Network n; n.seed(); const auto source = copySource(n, google);
	const auto provider = QString(google ? "google-drive" : "dropbox"), choice = uid(), baseline = uid(), event = uid(), revision = uid();
	const auto ref = CloudWorkspaceReference::create(provider, Account, uid(), google ? "root" : "id:root");
	auto root = CloudWorkspaceRecord::create(ref, CloudSyncManifest::sha256("genesis"));
	QVERIFY(root.registerCopy(baseline, {}, source)); QVERIFY(root.completeUpload(baseline, google ? "object-a" : "id:object-a", "rev-a")); QVERIFY(root.publish(baseline, {}));
	const auto stale = root; QVERIFY(root.acceptLegacyCopy(event, {}, baseline, baseline, source)); const auto checkpoint = root;
	QTemporaryDir temp; QVERIFY(temp.isValid()); CloudWorkspaceBinding binding(temp.filePath("binding.json"), ref, choice);
	QVERIFY(binding.activate(root, root, A, A, revision));
	const auto target = uid(), reservation = uid(); QVERIFY(root.registerUpload(target, baseline, CloudSyncManifest::sha256(B), B.size()));
	QVERIFY(root.completeUpload(target, google ? "object-b" : "id:object-b", "rev-b")); QVERIFY(root.publish(target, baseline)); QVERIFY(root.reserve(reservation, target));
	const auto ticket = root.ticket(target);
	const QJsonObject snapshot = {{"name", ticket["name"]}, {"object", ticket["object"]}, {"version", ticket["version"]}, {"sha256", ticket["sha256"]}, {"bytes", ticket["bytes"]}};
	CloudWorkspaceReadIntent reader(temp.filePath("reader.json"), ref, target, mode == 5 ? uid() : reservation);
	QVERIFY(reader.prepare()); QVERIFY(reader.pin(snapshot)); QVERIFY(reader.markReserved()); if (mode != 6) QVERIFY(reader.markVerified());
	if (mode == 7) QVERIFY(root.release(reservation));
	if (mode == 8) {
		const auto later = uid(); QVERIFY(root.registerUpload(later, target, CloudSyncManifest::sha256(A), A.size()));
		QVERIFY(root.completeUpload(later, google ? "object-c" : "id:object-c", "rev-c")); QVERIFY(root.publish(later, target));
	}
	if (mode == 13) QVERIFY(root.acceptLegacyCopy(uid(), event, target, baseline, source));
	const auto dir = temp.filePath("receive"), intent = QDir(dir).filePath("intent.json"), nextRevision = uid();
	const auto readReceipt = [](const QString &path) { QFile f(path); return f.open(QIODevice::ReadOnly) ? f.readAll() : QByteArray(); };
	if (mode == 9) { QVERIFY(QDir().mkpath(dir)); QFile f(QDir(dir).filePath("unknown")); QVERIFY(f.open(QIODevice::WriteOnly)); QVERIFY(f.write("keep") == 4); f.close(); }
	if (mode == 16) QVERIFY(QDir().mkpath(dir));
	CloudWorkspaceReceiveReceipt receipt(dir, ref, mode == 3 ? uid() : choice, mode == 4 ? uid() : revision, baseline, target, reservation, mode == 27 ? revision : nextRevision);
	std::unique_ptr<QLockFile> held;
	if (mode == 16) { held = std::make_unique<QLockFile>(intent + ".lock"); QVERIFY(held->tryLock(0)); }
	const auto prepare = [&] { return receipt.prepare(binding, root, mode == 14 ? stale : checkpoint, reader, mode == 1 ? B : A, mode == 2 ? A : B, B); };
	if ((mode >= 1 && mode <= 9) || mode == 13 || mode == 14 || mode == 16 || mode == 27) { QVERIFY(!prepare()); QVERIFY(!QFileInfo::exists(intent)); return; }
	QVERIFY(prepare()); QCOMPARE(receipt.state(), QString("ready")); QCOMPARE(receipt.before(), A); QCOMPARE(receipt.raw(), B); QCOMPARE(receipt.after(), B);
	const auto initial = readReceipt(intent);
	if (mode == 10) { QFile f(QDir(dir).filePath("raw.xml")); QVERIFY(f.open(QIODevice::WriteOnly | QIODevice::Truncate)); QVERIFY(f.write("foreign") == 7); f.close(); QVERIFY(!receipt.reload()); QVERIFY(!receipt.beginApply()); QCOMPARE(readReceipt(intent), initial); return; }
	if (mode == 15) {
		auto pending = QJsonDocument::fromJson(initial).object(); pending["state"] = "prepared";
		QFile f(intent); QVERIFY(f.open(QIODevice::WriteOnly | QIODevice::Truncate)); const auto json = QJsonDocument(pending).toJson(); QCOMPARE(f.write(json), qint64(json.size())); f.close();
		QVERIFY(QFile::remove(QDir(dir).filePath("raw.xml"))); QVERIFY(prepare()); QCOMPARE(receipt.raw(), B);
	}
	QVERIFY(!receipt.persisted(B)); QVERIFY(!receipt.applied(B)); QVERIFY(receipt.beginApply());
	if (mode == 11) { QVERIFY(!receipt.applied(A)); QCOMPARE(receipt.state(), QString("applying")); return; }
	QVERIFY(receipt.applied(B));
	if (mode == 12) { QVERIFY(!receipt.persisted(A)); QCOMPARE(receipt.state(), QString("applied")); return; }
	if (mode == 23) { QVERIFY(!binding.advanceReceived(revision, root, checkpoint, receipt, reader, B, uid())); QCOMPARE(binding.commit(), baseline); return; }
	QVERIFY(receipt.persisted(B)); QVERIFY(receipt.persisted(B)); QVERIFY(!receipt.beginApply());
	CloudWorkspaceReceiveReceipt restarted(dir, ref, choice, revision, baseline, target, reservation, nextRevision);
	QVERIFY(restarted.reload()); QCOMPARE(restarted.state(), QString("persisted")); QCOMPARE(restarted.before(), A); QCOMPARE(restarted.raw(), B); QCOMPARE(restarted.after(), B);
	if (mode < 17) return;
	if (mode == 19) { QVERIFY(reader.beginRelease()); QVERIFY(reader.markReleased()); }
	if (mode == 20) {
		const auto later = uid(); QVERIFY(root.registerUpload(later, target, CloudSyncManifest::sha256(A), A.size()));
		QVERIFY(root.completeUpload(later, google ? "object-c" : "id:object-c", "rev-c")); QVERIFY(root.publish(later, target));
	}
	if (mode == 21) QVERIFY(root.acceptLegacyCopy(uid(), event, target, baseline, source));
	if (mode == 22) { QFile f(QDir(dir).filePath("after.xml")); QVERIFY(f.open(QIODevice::WriteOnly | QIODevice::Truncate)); QVERIFY(f.write("foreign") == 7); f.close(); }
	if (mode == 24) QVERIFY(root.release(reservation));
	const auto newRevision = mode == 26 ? uid() : nextRevision; const auto advanced = binding.advanceReceived(mode == 18 ? uid() : revision, root, checkpoint, restarted, reader, mode == 25 ? A : B, newRevision);
	if (mode == 17 || mode == 20 || mode >= 28) {
		QVERIFY(advanced); QCOMPARE(binding.commit(), target); QCOMPARE(binding.revision(), newRevision); QVERIFY(binding.matchesModel(B));
		QVERIFY(binding.advanceReceived(revision, root, checkpoint, restarted, reader, B, newRevision));
		QVERIFY(!binding.advanceReceived(revision, root, checkpoint, restarted, reader, B, uid()));
		if (mode >= 28) {
			if (mode == 29 || mode == 30 || mode == 31) QVERIFY(reader.beginRelease());
			if (mode == 30 || mode == 31) QVERIFY(reader.markReleased());
			if (mode == 30 || mode == 32) QVERIFY(root.release(reservation));
			if (mode == 34) { QFile f(QDir(dir).filePath("after.xml")); QVERIFY(f.open(QIODevice::WriteOnly | QIODevice::Truncate)); QVERIFY(f.write("foreign") == 7); f.close(); }
			if (mode == 35) QVERIFY(root.acceptLegacyCopy(uid(), event, target, baseline, source));
			if (mode == 36) {
				const auto later = uid(); QVERIFY(root.registerUpload(later, target, CloudSyncManifest::sha256(A), A.size()));
				QVERIFY(root.completeUpload(later, google ? "object-c" : "id:object-c", "rev-c")); QVERIFY(root.publish(later, target));
			}
			if (mode == 33) {
				CloudWorkspaceReadIntent foreign(temp.filePath("foreign-reader.json"), ref, target, uid());
				QVERIFY(!binding.ownsReceived(root, checkpoint, restarted, foreign));
			} else QCOMPARE(binding.ownsReceived(root, checkpoint, restarted, reader), mode != 31 && mode != 32 && mode != 34);
		}
	} else { QVERIFY(!advanced); QCOMPARE(binding.commit(), baseline); QCOMPARE(binding.revision(), revision); }
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
		"fresh-after-unknown", "competing-head", "ack-points-at-root", "ack-points-at-source", "wrong-payload", "payload-changed-before-publish", "payload-changed-after-publish", "choice-away-back",
		"empty-choice-lock-released", "cancel-waiting-empty-lock", "empty-intent-lock-released", "empty-checkpoint-lock-released", "expired-empty-choice-lock", "choice-changed-while-waiting", "scope-changed-while-waiting", "live-choice-lock"};
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
	if (mode >= 34 && mode <= 40) {
		const auto path = (mode == 36 ? intentFile : mode == 37 ? cpFile : choiceFile) + ".lock"; QFile orphan(path); QVERIFY(orphan.open(QIODevice::ReadWrite));
		if (mode == 38) QVERIFY(orphan.setFileTime(QDateTime::currentDateTimeUtc().addSecs(-60), QFileDevice::FileModificationTime)); orphan.close();
		if (mode != 38) QTimer::singleShot(40, &client, [&, path] { injectionFailed = !QFile::remove(path); if (mode == 39) changeChoice(false); if (mode == 40) n.guard = false; });
		if (mode == 35) QTimer::singleShot(20, &client, [&] { client.cancel(); });
	}
	if (mode == 41) { lock = std::make_unique<QLockFile>(choiceFile + ".lock"); QVERIFY(lock->tryLock(0)); }
	start(client, ticket, intentFile);
	if (mode == 35) { QTest::qWait(250); QCOMPARE(done.size() + errors.size(), 0); QCOMPARE(requests, 0); }
	else if (mode == 17 || mode == 22 || mode == 23) {
		QTRY_VERIFY(n.deferred); client.cancel(); if (n.deferred) n.deferred->complete(); n.deferReads = false; QTest::qWait(20); QCOMPARE(done.size() + errors.size(), 0);
		if (mode != 17) { start(client, ticket, intentFile); QTRY_COMPARE(done.size() + errors.size(), 1); QCOMPARE(done.size(), 1); }
	} else { QTRY_COMPARE(done.size() + errors.size(), 1); const bool success = mode == 0 || mode == 1 || mode == 10 || mode == 12 || mode == 13 || mode == 15 || mode == 24 || mode == 34 || mode == 36 || mode == 37 || mode == 38; QCOMPARE(done.size(), int(success)); }
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
	if (mode == 2 || mode == 3 || mode == 4 || mode == 5 || mode == 6 || mode == 7 || mode == 8 || mode == 9 || mode == 21 || mode == 39 || mode == 40 || mode == 41) QCOMPARE(requests, 0);
	if (mode == 16) QVERIFY(errors.first()[1].toBool());
	if (mode == 5 || mode == 20) QVERIFY(!QFileInfo::exists(cpFile)); if (mode == 6 || mode == 25) QCOMPARE(read(cpFile), cpBytes);
	if (mode == 31) QVERIFY(CloudWorkspaceRecord::parse(n.files[2].data).head().isEmpty());
	if (mode == 32) QCOMPARE(CloudWorkspaceRecord::parse(n.files[2].data).head(), ticket);
	if (mode != 3 && mode != 18 && mode != 33 && mode != 39) QCOMPARE(read(choiceFile), choiceBytes);
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
		"payload-replaced", "cancel", "scope-during-read", "rejected-token", "provider-failure", "manifest-renamed", "manifest-extra-field", "manifest-invalid-parent", "fractional-schema", "schema1-invalid-revision",
		"confirm-absent", "manifest-appears-during-confirmation", "missing-payload-is-not-absence", "scope-changes-during-confirmation", "token-rejected-during-confirmation"};
	for (bool g : {false, true}) for (int m = 0; m < cases.size(); ++m) QTest::newRow(qPrintable(QString("%1-source-review-%2").arg(g).arg(cases[m]))) << g << m;
}
void TestCloudSyncHistory::workspaceSourceReview()
{
	QFETCH(bool, google); QFETCH(int, mode); Network n; n.seed(mode == 1 || mode == 26 ? 1 : 2);
	const auto provider = QString(google ? "google-drive" : "dropbox"), payloadName = n.files[1].name;
	const auto originalManifest = n.files[0].data;
	if (mode == 7 || mode == 27 || mode == 28 || mode == 30 || mode == 31) n.files.removeAt(0); if (mode == 8 || mode == 29) n.files.removeAt(1);
	if (mode == 9) n.files[0].data = "invalid manifest";
	if (mode == 10) { auto v = QJsonDocument::fromJson(n.files[0].data).object(); v["schema"] = 99; n.files[0].data = n.json(v); }
	if (mode == 11) n.files[1].data = B;
	if (mode == 12) { n.files[1].data = "not XML"; auto m = CloudSyncManifest::fromJson(n.files[0].data); m.payloadSha256 = CloudSyncManifest::sha256(n.files[1].data); n.files[0].data = m.toJson(); }
	if (mode == 13) n.files[0].data += QByteArray(16385, ' ');
	if (mode >= 23 && mode <= 26) {
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
		if (manifestAliases == 2 && mode == 28 && !injected) { n.add(Legacy, originalManifest); injected = true; }
		if (manifestAliases == 2 && mode == 30) n.guard = false;
		if (manifestAliases == 2 && mode == 31) n.readFailureStatus = 401;
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
	const auto before = n.files; CloudWorkspaceSourceReview review(&n); QSignalSpy done(&review, &CloudWorkspaceSourceReview::finished), errors(&review, &CloudWorkspaceSourceReview::failed), absent(&review, &CloudWorkspaceSourceReview::absent);
	review.start(mode == 4 ? "" : "token", mode == 2 ? "other" : provider, mode == 3 ? "bad-account" : Account, mode == 5 ? std::function<bool()>() : [&] { return n.guard; }, mode >= 27);
	if (mode == 18) { QTRY_VERIFY(n.deferred); review.cancel(); if (n.deferred) n.deferred->complete(); QTest::qWait(20); QCOMPARE(done.size() + errors.size(), 0); }
	else { QTRY_COMPARE(done.size() + errors.size() + absent.size(), 1); QCOMPARE(done.size(), int(mode == 0 || mode == 1)); QCOMPARE(absent.size(), int(mode == 27)); }
	if (!done.isEmpty()) {
		CloudWorkspaceCopySource source; source.value = done.first()[0].toJsonObject(); QVERIFY(source.matches(provider, Account)); QVERIFY(source.accepts(A)); QCOMPARE(done.first()[1].toByteArray(), A);
		QCOMPARE(manifestAliases, 2); QCOMPARE(payloadAliases, 2); QCOMPARE(QByteArray::fromBase64(source.value["manifest"].toObject()["body_base64"].toString().toLatin1()), before[0].data);
		QTemporaryDir temporary; QVERIFY(source.preserve(temporary.filePath("proof.json"))); QCOMPARE(CloudWorkspaceCopySource::load(temporary.filePath("proof.json")).json(), source.json());
	}
	if (mode >= 2 && mode <= 6) QCOMPARE(requests, 0); if (mode == 20 || mode == 31) QVERIFY(errors.first()[1].toBool());
	if (mode == 27) QCOMPARE(manifestAliases, 2);
	QCOMPARE(n.mutations, 0); QCOMPARE(n.renames, 0); QCOMPARE(n.files.size(), before.size() + int(mode == 28));
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
void TestCloudSyncHistory::workspaceUploadIntent_data()
{
	QTest::addColumn<bool>("google"); QTest::addColumn<int>("mode");
	const QStringList cases = {"durable-phases", "changed-payload-hash", "changed-length", "foreign-root", "foreign-ticket", "foreign-parent", "locked-writes-read-only-inspect",
		"corrupt", "future-format", "unknown-field", "future-schema", "wrong-state", "forged-id-before-submit", "forged-version-before-identify", "oversized", "blocked-parent",
		"invalid-hash", "empty-length", "too-large-length", "bad-ticket", "same-parent", "bad-parent"};
	for (bool g : {false, true}) for (int mode = 0; mode < cases.size(); ++mode) QTest::newRow(qPrintable(QString("%1-upload-intent-%2").arg(g).arg(cases[mode]))) << g << mode;
}
void TestCloudSyncHistory::workspaceUploadIntent()
{
	QFETCH(bool, google); QFETCH(int, mode); QTemporaryDir dir; QVERIFY(dir.isValid());
	const auto provider = QString(google ? "google-drive" : "dropbox"), ticket = uid(), hash = CloudSyncManifest::sha256(B), parent = uid();
	const auto ref = CloudWorkspaceReference::create(provider, Account, uid(), google ? "root" : "id:root"); const auto file = dir.filePath("intent.json");
	const auto read = [&] { QFile f(file); return f.open(QIODevice::ReadOnly) ? f.readAll() : QByteArray(); };
	CloudWorkspaceUploadIntent first(file, ref, hash, B.size(), ticket, parent); QVERIFY(first.prepare()); const auto before = read();
	if (mode >= 7 && mode <= 14) {
		auto object = QJsonDocument::fromJson(before).object();
		if (mode == 8) object["format"] = "future-upload-format"; if (mode == 9) object["future_fence"] = true; if (mode == 10) object["schema"] = 2;
		if (mode == 11) object["state"] = "expired"; if (mode == 12) object["object"] = google ? "payload" : "id:payload"; if (mode == 13) object["version"] = "forged-version";
		QFile corrupt(file); QVERIFY(corrupt.open(QIODevice::WriteOnly)); corrupt.write(mode == 7 ? QByteArray("broken") : mode == 14 ? QByteArray(8193, 'x') : QJsonDocument(object).toJson(QJsonDocument::Compact)); corrupt.close();
		const auto bad = read(); QVERIFY(!first.prepare()); QVERIFY(!first.reload()); QCOMPARE(read(), bad); return;
	}
	std::unique_ptr<QLockFile> lock;
	if (mode == 6) { lock = std::make_unique<QLockFile>(file + ".lock"); QVERIFY(lock->tryLock(0)); QVERIFY(first.inspect()); QVERIFY(!first.reload()); QVERIFY(!first.beginRegistration()); QCOMPARE(read(), before); return; }
	if (mode == 15) { const auto blocked = dir.filePath("blocked"); QFile f(blocked); QVERIFY(f.open(QIODevice::WriteOnly)); f.write("keep"); f.close(); CloudWorkspaceUploadIntent bad(blocked + "/intent.json", ref, hash, B.size(), ticket, parent); QVERIFY(!bad.prepare()); return; }
	auto other = ref; if (mode == 3) other = CloudWorkspaceReference::create(provider, Account, ref.value["epoch"].toString(), google ? "other-root" : "id:other-root");
	CloudWorkspaceUploadIntent second(file, other, mode == 1 ? CloudSyncManifest::sha256(A) : mode == 16 ? "bad-hash" : hash,
		mode == 2 ? B.size() + 1 : mode == 17 ? 0 : mode == 18 ? 64 * 1024 * 1024 + 1 : B.size(), mode == 4 ? uid() : mode == 19 ? "bad-ticket" : ticket,
		mode == 5 ? uid() : mode == 20 ? ticket : mode == 21 ? "bad-parent" : parent);
	if (mode != 0) { QVERIFY(!second.prepare()); QVERIFY(!second.reload()); QCOMPARE(read(), before); return; }
	QVERIFY(second.reload()); QVERIFY(!second.submit()); QVERIFY(second.beginRegistration()); QVERIFY(second.registerTicket()); QVERIFY(second.submit()); QVERIFY(!second.submit());
	QVERIFY(!second.identify(ref.value["root_id"].toString())); const auto id = google ? "new-payload" : "id:new-payload";
	QVERIFY(second.identify(id)); QVERIFY(!second.identify(google ? "other-payload" : "id:other-payload")); QVERIFY(!second.verify("bad\nrevision"));
	QVERIFY(second.verify("payload-version")); QVERIFY(!second.verify("other-version")); QVERIFY(second.upload()); QVERIFY(second.commit());
	CloudWorkspaceUploadIntent restarted(file, ref, hash, B.size(), ticket, parent); QVERIFY(restarted.reload()); QCOMPARE(restarted.state(), QString("committed")); QCOMPARE(restarted.id(), id);
	const auto completed = read(); QVERIFY(restarted.prepare()); QVERIFY(!restarted.submit()); QCOMPARE(read(), completed); QVERIFY(!completed.contains(B)); QVERIFY(!completed.contains("token"));
}
void TestCloudSyncHistory::workspaceUploadWorkflow_data()
{
	QTest::addColumn<bool>("google"); QTest::addColumn<int>("mode");
	const QStringList cases = {"publish-restart", "preserve-copies-reservations", "scope-before", "missing-choice", "stale-choice", "missing-checkpoint", "corrupt-checkpoint", "corrupt-intent", "live-intent-lock", "submitted-unknown",
		"register-ack-lost", "payload-ack-lost", "upload-ack-lost", "publish-ack-lost", "cancel-identified-resume", "scope-after-ack", "choice-after-ack", "checkpoint-lost-after-ack", "ack-points-at-root", "wrong-payload",
		"payload-changed-before-publish", "payload-changed-after-publish", "rollback", "stale-parent", "advanced-head-final-observation", "competing-head-before-create", "missing-local-identity", "changed-snapshot-resume", "resume-identified", "cancel-uploaded-resume",
		"cancel-root-read", "token-rejected", "invalid-xml", "empty-payload", "too-large-payload", "empty-choice-lock-released", "empty-intent-lock-released", "empty-checkpoint-lock-released", "expired-empty-choice-lock", "cancel-waiting", "choice-aba-while-waiting", "scope-while-waiting",
		"restart-after-later-head", "next-local-version", "ack-points-at-existing-payload", "payload-revision-changed", "foreign-choice-same-epoch", "dtd-payload", "refuse-copy-ticket-resume", "future-root", "missing-root"};
	for (bool g : {false, true}) for (int mode = 0; mode < cases.size(); ++mode) QTest::newRow(qPrintable(QString("%1-upload-workflow-%2").arg(g).arg(cases[mode]))) << g << mode;
}
void TestCloudSyncHistory::workspaceUploadWorkflow()
{
	QFETCH(bool, google); QFETCH(int, mode); Network n; n.seed(); n.workspaceWrites = n.workspaceUploads = true;
	const auto original = n.files; const auto provider = QString(google ? "google-drive" : "dropbox"), ticket = uid();
	const auto ref = CloudWorkspaceReference::create(provider, Account, uid(), google ? "3" : "id:3");
	auto initial = CloudWorkspaceRecord::create(ref, CloudSyncManifest::sha256("genesis")); n.add(ref.rootName(), initial.json());
	const auto copy = copySource(n, google); QString priorTicket, reservation, pendingTicket;
	if (mode == 1 || mode == 44 || mode == 48) {
		priorTicket = mode == 48 ? ticket : uid(); reservation = uid(); pendingTicket = uid(); QVERIFY(initial.registerCopy(priorTicket, {}, copy)); n.add(ref.payloadName(priorTicket), A);
		QVERIFY(initial.completeUpload(priorTicket, google ? n.files.last().id.mid(3) : n.files.last().id, n.files.last().rev)); QVERIFY(initial.publish(priorTicket, {}));
		QVERIFY(initial.reserve(reservation, priorTicket)); QVERIFY(initial.registerUpload(pendingTicket, priorTicket, CloudSyncManifest::sha256(B), B.size()));
		n.files[2].data = initial.json(); n.files[2].rev += "-seed";
	}
	const auto addOther = [&] {
		auto other = CloudWorkspaceRecord::parse(n.files[2].data); const auto id = uid(), parent = other.head(); QVERIFY(other.registerUpload(id, parent, CloudSyncManifest::sha256(A), A.size()));
		n.add(ref.payloadName(id), A); QVERIFY(other.completeUpload(id, google ? n.files.last().id.mid(3) : n.files.last().id, n.files.last().rev)); QVERIFY(other.publish(id, parent));
		n.files[2].data = other.json(); n.files[2].rev += "-other";
	};
	if (mode == 23) { addOther(); initial = CloudWorkspaceRecord::parse(n.files[2].data); }
	const auto parent = mode == 23 || mode == 48 ? QString() : initial.head();
	QTemporaryDir dir; QVERIFY(dir.isValid()); const auto choiceFile = dir.filePath("choice.json"), cpFile = dir.filePath("checkpoint.json"), intentFile = dir.filePath("intent.json");
	const auto read = [](const QString &path) { QFile f(path); return f.open(QIODevice::ReadOnly) ? f.readAll() : QByteArray(); };
	CloudWorkspaceSelection choice(choiceFile, provider, Account); QVERIFY(choice.load()); QVERIFY(choice.select(choice.version(), ref)); const auto choiceVersion = choice.version(); const auto choiceBytes = read(choiceFile);
	CloudWorkspaceAuthority checkpoint(cpFile, ref); QVERIFY(checkpoint.observe(initial, n.files[2].rev));
	if (mode == 22) { auto newer = initial; QVERIFY(newer.registerUpload(uid(), initial.head(), CloudSyncManifest::sha256(A), A.size())); QVERIFY(checkpoint.observe(newer, "newer-version")); }
	const auto cpBytes = read(cpFile); auto payload = B; if (mode == 48) payload = A;
	CloudWorkspaceUploadIntent saved(intentFile, ref, CloudSyncManifest::sha256(mode == 27 ? A : payload), mode == 27 ? A.size() : payload.size(), ticket, parent);
	if (mode == 9 || mode == 28 || mode == 48) {
		QVERIFY(saved.prepare()); QVERIFY(saved.beginRegistration()); QVERIFY(saved.registerTicket()); QVERIFY(saved.submit());
		if (mode == 28) {
			QVERIFY(initial.registerUpload(ticket, parent, CloudSyncManifest::sha256(payload), payload.size())); n.files[2].data = initial.json(); n.files[2].rev += "-registered";
			n.add(ref.payloadName(ticket), payload); QVERIFY(saved.identify(google ? n.files.last().id.mid(3) : n.files.last().id));
		}
		if (mode == 48) { QVERIFY(saved.identify(google ? n.files[3].id.mid(3) : n.files[3].id)); QVERIFY(saved.verify(n.files[3].rev)); QVERIFY(saved.upload()); }
	}
	if (mode == 26) { QVERIFY(initial.registerUpload(ticket, parent, CloudSyncManifest::sha256(payload), payload.size())); n.files[2].data = initial.json(); n.files[2].rev += "-registered"; }
	if (mode == 27) QVERIFY(saved.prepare());
	if (mode == 3) QVERIFY(QFile::remove(choiceFile)); if (mode == 5) QVERIFY(QFile::remove(cpFile));
	if (mode == 6 || mode == 7) { QFile f(mode == 6 ? cpFile : intentFile); QVERIFY(f.open(QIODevice::WriteOnly)); f.write("corrupt"); f.close(); }
	std::unique_ptr<QLockFile> lock; if (mode == 8) { lock = std::make_unique<QLockFile>(intentFile + ".lock"); QVERIFY(lock->tryLock(0)); }
	if (mode == 2) n.guard = false; if (mode == 31) n.readFailureStatus = 401;
	if (mode == 32) payload = "<divelog><dives>"; if (mode == 33) payload.clear(); if (mode == 34) payload = QByteArray(64 * 1024 * 1024 + 1, 'x');
	if (mode == 47) payload = "<!DOCTYPE divelog><divelog><dives/></divelog>";
	if (mode == 49) { auto future = initial.value; future["format"] = "future-workspace"; n.files[2].data = n.json(future); }
	if (mode == 50) n.files.removeAt(2);
	bool injected = false, injectionFailed = false; int requests = 0, creates = 0, updates = 0, lookups = 0;
	const auto changeChoice = [&](bool back) {
		CloudWorkspaceSelection changed(choiceFile, provider, Account); if (!changed.load()) { injectionFailed = true; return; }
		const auto other = CloudWorkspaceReference::create(provider, Account, ref.value["epoch"].toString(), google ? "other-root" : "id:other-root");
		if (!changed.select(changed.version(), other) || (back && !changed.select(changed.version(), ref))) injectionFailed = true;
	};
	if (mode == 46) changeChoice(false);
	n.requested = [&](const QNetworkRequest &r) {
		++requests; const auto path = r.url().path(); const auto arg = QJsonDocument::fromJson(r.rawHeader("Dropbox-API-Arg")).object();
		const bool create = path == "/upload/drive/v3/files" || (!google && path == "/2/files/upload" && arg["mode"].toString() == "add");
		const bool update = path.startsWith("/upload/drive/v2/") || (!google && path == "/2/files/upload" && arg["mode"].isObject());
		if (path == "/drive/v3/files") ++lookups;
		if (create) { ++creates; if (mode == 11 && creates == 1) n.loseWriteAck = true; }
		if (update) { ++updates; if ((mode == 10 && updates == 1) || (mode == 12 && updates == 2) || (mode == 13 && updates == 3)) n.loseWriteAck = true; }
		const int payloadIndex = n.find(ref.payloadName(ticket));
		const bool payloadRead = payloadIndex >= 0 && (google ? path.endsWith("/" + n.files[payloadIndex].id.mid(3)) : arg["path"].toString() == n.files[payloadIndex].id);
		if (!create && !update && !injected && ((mode == 14 && creates == 1 && payloadRead) ||
		    (mode == 29 && updates == 2 && (google ? path.endsWith("/3") : arg["path"].toString() == "id:3")))) { n.deferReads = true; injected = true; }
	};
	n.wrote = [&](const QString &name) {
		if (!name.endsWith(".json")) { if (mode == 19) n.files[n.find(name)].data = A; }
		else {
			if ((mode == 20 && updates == 2) || (mode == 21 && updates == 3) || (mode == 45 && updates == 2)) {
				const auto index = n.find(ref.payloadName(ticket)); QVERIFY(index >= 0); if (mode != 45) n.files[index].data = A; n.files[index].rev += "-changed";
			}
			if ((mode == 24 && updates == 3) || (mode == 25 && updates == 1)) addOther();
		}
	};
	n.response = [&](const QNetworkRequest &r, const QByteArray &bytes) {
		const auto path = r.url().path(); const auto arg = QJsonDocument::fromJson(r.rawHeader("Dropbox-API-Arg")).object();
		const bool create = path == "/upload/drive/v3/files" || (!google && path == "/2/files/upload" && arg["mode"].toString() == "add");
		if (create) {
			if (mode == 15) n.guard = false; if (mode == 16) changeChoice(false); if (mode == 17) injectionFailed = !QFile::remove(cpFile);
			if (mode == 18 || mode == 44) { auto response = QJsonDocument::fromJson(bytes).object(); response["id"] = google ? (mode == 18 ? "3" : n.files[3].id.mid(3)) : (mode == 18 ? "id:3" : n.files[3].id); return n.json(response); }
		}
		return bytes;
	};
	CloudWorkspaceUpload client(&n); QSignalSpy done(&client, &CloudWorkspaceUpload::finished), errors(&client, &CloudWorkspaceUpload::failed);
	const auto start = [&](const QString &attempt, const QString &file, const QByteArray &body, const QString &expectedParent) { client.start("token", ref, body, choiceFile, mode == 4 ? uid() : choiceVersion, cpFile, file, attempt, expectedParent, [&] { return n.guard; }); };
	if (mode >= 35 && mode <= 41) {
		const auto path = (mode == 36 ? intentFile : mode == 37 ? cpFile : choiceFile) + ".lock"; QFile orphan(path); QVERIFY(orphan.open(QIODevice::ReadWrite));
		if (mode == 38) QVERIFY(orphan.setFileTime(QDateTime::currentDateTimeUtc().addSecs(-60), QFileDevice::FileModificationTime)); orphan.close();
		if (mode != 38) QTimer::singleShot(40, &client, [&, path] { injectionFailed = !QFile::remove(path); if (mode == 40) changeChoice(true); if (mode == 41) n.guard = false; });
		if (mode == 39) QTimer::singleShot(20, &client, [&] { client.cancel(); });
	}
	if (mode == 30) n.deferReads = true;
	start(ticket, intentFile, payload, parent);
	if (mode == 39) { QTest::qWait(250); QCOMPARE(done.size() + errors.size(), 0); QCOMPARE(requests, 0); }
	else if (mode == 14 || mode == 29 || mode == 30) {
		QTRY_VERIFY(n.deferred); client.cancel(); if (n.deferred) n.deferred->complete(); n.deferReads = false; QTest::qWait(20); QCOMPARE(done.size() + errors.size(), 0);
		if (mode != 30) { start(ticket, intentFile, payload, parent); QTRY_COMPARE(done.size() + errors.size(), 1); QCOMPARE(done.size(), 1); }
	} else {
		QTRY_COMPARE(done.size() + errors.size(), 1); const bool success = mode == 0 || mode == 1 || mode == 10 || mode == 12 || mode == 13 || mode == 24 || mode == 28 || (mode >= 35 && mode <= 38) || mode == 42 || mode == 43;
		QCOMPARE(done.size(), int(success));
	}
	if (!done.isEmpty()) {
		auto completed = CloudWorkspaceRecord::parse(n.files[2].data); QVERIFY(completed.valid()); QVERIFY(completed.follows(initial)); QCOMPARE(completed.ticket(ticket)["state"].toString(), QString("committed"));
		QCOMPARE(n.files[n.find(ref.payloadName(ticket))].data, B); QCOMPARE(done.last()[3].toBool(), mode != 24); QCOMPARE(creates, mode == 28 ? 0 : 1); QCOMPARE(updates, mode == 28 ? 2 : 3);
		if (mode == 1) { QCOMPARE(completed.copySource(priorTicket).json(), copy.json()); QCOMPARE(completed.ticket(pendingTicket), initial.ticket(pendingTicket)); QCOMPARE(completed.value["journal"].toObject()["reservations"], initial.value["journal"].toObject()["reservations"]); }
		const auto mutations = n.mutations;
		if (mode == 0 || mode == 42) { if (mode == 42) addOther(); start(ticket, intentFile, payload, parent); QTRY_COMPARE(done.size(), 2); QCOMPARE(n.mutations, mutations); QCOMPARE(done.last()[3].toBool(), mode == 0); }
		if (mode == 43) { const auto next = uid(); start(next, dir.filePath("next.json"), A, completed.head()); QTRY_COMPARE(done.size(), 2); completed = CloudWorkspaceRecord::parse(n.files[2].data); QCOMPARE(completed.head(), next); QCOMPARE(completed.ticket(ticket)["state"].toString(), QString("committed")); QCOMPARE(n.files[n.find(ref.payloadName(ticket))].data, B); }
	}
	if (mode == 11) {
		const auto requestCount = requests; start(ticket, intentFile, payload, parent); QCOMPARE(requests, requestCount); QCOMPARE(errors.size(), 2); const auto fresh = uid(); start(fresh, dir.filePath("fresh.json"), payload, parent); QTRY_COMPARE(done.size(), 1);
		const auto root = CloudWorkspaceRecord::parse(n.files[2].data); QCOMPARE(root.head(), fresh); QCOMPARE(root.ticket(ticket)["state"].toString(), QString("registered")); QCOMPARE(creates, 2);
	}
	if (mode == 2 || mode == 3 || mode == 4 || mode == 5 || mode == 6 || mode == 7 || mode == 8 || mode == 9 || mode == 27 || mode == 32 || mode == 33 || mode == 34 || mode == 40 || mode == 41 || mode == 46 || mode == 47) QCOMPARE(requests, 0);
	if (mode == 15 || mode == 16 || mode == 17) { CloudWorkspaceUploadIntent captured(intentFile, ref, CloudSyncManifest::sha256(payload), payload.size(), ticket, parent); QVERIFY(captured.reload()); QVERIFY(!captured.id().isEmpty()); }
	if (mode == 31) QVERIFY(errors.first()[1].toBool()); if (mode == 5 || mode == 17) QVERIFY(!QFileInfo::exists(cpFile)); if (mode == 22) QCOMPARE(read(cpFile), cpBytes);
	if (mode == 19 || mode == 20 || mode == 45) QCOMPARE(CloudWorkspaceRecord::parse(n.files[2].data).head(), initial.head());
	if (mode == 21) QCOMPARE(CloudWorkspaceRecord::parse(n.files[2].data).head(), ticket);
	if (mode == 48 || mode == 49 || mode == 50) QCOMPARE(n.mutations, 0);
	if (mode == 48) QCOMPARE(CloudWorkspaceRecord::parse(n.files[2].data).copySource(ticket).json(), copy.json());
	if (mode != 3 && mode != 16 && mode != 40 && mode != 46) QCOMPARE(read(choiceFile), choiceBytes);
	QCOMPARE(n.files[0].data, original[0].data); QCOMPARE(n.files[1].data, original[1].data); QVERIFY(n.allowed); QVERIFY(!injectionFailed); QCOMPARE(n.renames, 0); QCOMPARE(lookups, 0);
}
namespace {
struct ReadFixture {
	CloudWorkspaceReference reference;
	CloudWorkspaceRecord root;
	QString commit, head, pending, protectedRead, releasedRead;
	QJsonObject snapshot;
};
ReadFixture readFixture(Network &n, bool google, bool copied = false, const QByteArray &body = A)
{
	n.seed(); n.workspaceWrites = true; ReadFixture f;
	f.reference = CloudWorkspaceReference::create(google ? "google-drive" : "dropbox", Account, uid(), google ? "3" : "id:3");
	f.root = CloudWorkspaceRecord::create(f.reference, CloudSyncManifest::sha256("genesis")); n.add(f.reference.rootName(), f.root.json());
	f.commit = uid(); f.head = uid(); f.pending = uid(); f.protectedRead = uid(); f.releasedRead = uid();
	if (copied ? !f.root.registerCopy(f.commit, {}, copySource(n, google)) : !f.root.registerUpload(f.commit, {}, CloudSyncManifest::sha256(body), body.size())) return {};
	n.add(f.reference.payloadName(f.commit), body);
	if (!f.root.completeUpload(f.commit, google ? n.files.last().id.mid(3) : n.files.last().id, n.files.last().rev) || !f.root.publish(f.commit, {})) return {};
	if (!f.root.registerUpload(f.head, f.commit, CloudSyncManifest::sha256(B), B.size())) return {};
	n.add(f.reference.payloadName(f.head), B);
	if (!f.root.completeUpload(f.head, google ? n.files.last().id.mid(3) : n.files.last().id, n.files.last().rev) || !f.root.publish(f.head, f.commit) ||
	    !f.root.registerUpload(f.pending, f.head, CloudSyncManifest::sha256(A), A.size()) || !f.root.reserve(f.protectedRead, f.commit) ||
	    !f.root.reserve(f.releasedRead, f.commit) || !f.root.release(f.releasedRead)) return {};
	n.files[2].data = f.root.json(); n.files[2].rev += "-fixture";
	const auto t = f.root.ticket(f.commit); f.snapshot = {{"name", t["name"]}, {"object", t["object"]}, {"version", t["version"]}, {"sha256", t["sha256"]}, {"bytes", t["bytes"]}};
	return f;
}
}
void TestCloudSyncHistory::workspaceReadIntent_data()
{
	QTest::addColumn<bool>("google"); QTest::addColumn<int>("mode");
	const QStringList cases = {"durable-phases", "changed-root", "changed-commit", "changed-reservation", "same-identities", "bad-identities", "corrupt", "future-format", "extra-field", "future-state",
		"prepared-with-pin", "reserved-without-pin", "wrong-payload-name", "root-as-payload", "bad-revision", "bad-hash", "bad-length", "oversized", "locked-mutators", "cancel-before-registered", "scope-preserved-on-repin"};
	for (bool g : {false, true}) for (int mode = 0; mode < cases.size(); ++mode) QTest::newRow(qPrintable(QString("%1-read-intent-%2").arg(g).arg(cases[mode]))) << g << mode;
}
void TestCloudSyncHistory::workspaceReadIntent()
{
	QFETCH(bool, google); QFETCH(int, mode); Network n; const auto f = readFixture(n, google); QVERIFY(f.root.valid()); QTemporaryDir dir; QVERIFY(dir.isValid());
	const auto file = dir.filePath("read.json"), reservation = uid(); const auto read = [&] { QFile q(file); return q.open(QIODevice::ReadOnly) ? q.readAll() : QByteArray(); };
	CloudWorkspaceReadIntent first(file, f.reference, f.commit, reservation); QVERIFY(first.prepare()); const auto before = read();
	if ((mode >= 6 && mode <= 11) || mode == 17) {
		auto data = QJsonDocument::fromJson(before).object(); if (mode == 7) data["format"] = "future"; if (mode == 8) data["future_fence"] = true; if (mode == 9) data["state"] = "expired";
		if (mode == 10) data["snapshot"] = f.snapshot; if (mode == 11) data["state"] = "reserved";
		QFile q(file); QVERIFY(q.open(QIODevice::WriteOnly)); q.write(mode == 6 ? QByteArray("broken") : mode == 17 ? QByteArray(8193, 'x') : QJsonDocument(data).toJson(QJsonDocument::Compact)); q.close();
		const auto bad = read(); QVERIFY(!first.prepare()); QVERIFY(!first.reload()); QCOMPARE(read(), bad); return;
	}
	if (mode >= 1 && mode <= 5) {
		auto ref = f.reference; if (mode == 1) ref = CloudWorkspaceReference::create(google ? "google-drive" : "dropbox", Account, ref.value["epoch"].toString(), google ? "other" : "id:other");
		CloudWorkspaceReadIntent other(file, ref, mode == 2 ? uid() : mode == 5 ? "bad" : f.commit, mode == 3 ? uid() : mode == 4 ? f.commit : reservation);
		QVERIFY(!other.prepare()); QCOMPARE(read(), before); return;
	}
	if (mode == 18) { QLockFile lock(file + ".lock"); QVERIFY(lock.tryLock(0)); QVERIFY(first.inspect()); QVERIFY(!first.pin(f.snapshot)); QVERIFY(!first.reload()); QCOMPARE(read(), before); return; }
	if (mode >= 12 && mode <= 16) {
		auto pin = f.snapshot; if (mode == 12) pin["name"] = "other.xml"; if (mode == 13) pin["object"] = f.reference.value["root_id"]; if (mode == 14) pin["version"] = "bad\nversion";
		if (mode == 15) pin["sha256"] = "bad"; if (mode == 16) pin["bytes"] = 1.5; QVERIFY(!first.pin(pin)); QCOMPARE(read(), before); return;
	}
	QVERIFY(!first.markVerified()); QVERIFY(!first.beginRelease()); QVERIFY(first.pin(f.snapshot));
	if (mode == 20) { auto changed = f.snapshot; changed["version"] = "other-version"; const auto pinned = read(); QVERIFY(!first.pin(changed)); QCOMPARE(read(), pinned); }
	if (mode != 19) { QVERIFY(first.markReserved()); QVERIFY(first.markVerified()); }
	QVERIFY(first.beginRelease()); QVERIFY(!first.markReserved()); QVERIFY(!first.markVerified()); QVERIFY(first.markReleased());
	CloudWorkspaceReadIntent restart(file, f.reference, f.commit, reservation); QVERIFY(restart.reload()); QCOMPARE(restart.state(), QString("released")); const auto released = read();
	QVERIFY(restart.prepare()); QVERIFY(restart.beginRelease()); QVERIFY(restart.markReleased()); QVERIFY(!restart.pin(f.snapshot)); QCOMPARE(read(), released); QVERIFY(!released.contains(A));
}
void TestCloudSyncHistory::workspaceReadWorkflow_data()
{
	QTest::addColumn<bool>("google"); QTest::addColumn<int>("mode");
	const QStringList cases = {"read-restart-release", "copy-proofs-preserved", "scope-before", "missing-choice", "stale-choice", "missing-checkpoint", "corrupt-checkpoint", "corrupt-intent", "live-intent-lock", "registration-ack-lost",
		"wrong-payload-metadata", "bad-checksum", "missing-payload", "cancel-resume", "scope-after-registration", "choice-after-registration", "checkpoint-lost-after-registration", "invalid-xml", "dtd-xml", "released-before-payload",
		"released-after-payload", "advanced-head", "future-root", "rollback", "missing-commit", "collision-without-intent", "reserving-resume", "verified-resume", "released-no-revive", "reservation-limit",
		"token-rejected", "empty-choice-lock", "empty-intent-lock", "empty-checkpoint-lock", "expired-empty-lock", "cancel-waiting", "choice-aba-waiting", "guard-waiting", "missing-root", "changed-version"};
	for (bool g : {false, true}) for (int mode = 0; mode < cases.size(); ++mode) QTest::newRow(qPrintable(QString("%1-reserved-read-%2").arg(g).arg(cases[mode]))) << g << mode;
}
void TestCloudSyncHistory::workspaceReadWorkflow()
{
	QFETCH(bool, google); QFETCH(int, mode); Network n; const auto body = mode == 17 ? QByteArray("<divelog><dives>") : mode == 18 ? QByteArray("<!DOCTYPE divelog><divelog><dives/></divelog>") : A;
	auto f = readFixture(n, google, mode == 1, body); QVERIFY(f.root.valid()); const auto originals = n.files; auto commit = mode == 24 ? uid() : f.commit; auto reservation = mode == 25 ? f.protectedRead : mode == 28 ? f.releasedRead : uid();
	QTemporaryDir dir; QVERIFY(dir.isValid()); const auto choiceFile = dir.filePath("choice.json"), cpFile = dir.filePath("checkpoint.json"), intentFile = dir.filePath("read.json");
	const auto read = [](const QString &p) { QFile q(p); return q.open(QIODevice::ReadOnly) ? q.readAll() : QByteArray(); };
	if (mode == 29) { auto journal = f.root.value["journal"].toObject(), reservations = journal["reservations"].toObject(); while (reservations.size() < 512) reservations[uid()] = QJsonObject{{"commit", f.commit}, {"active", true}}; journal["reservations"] = reservations; journal["sequence"] = journal["sequence"].toInt() + 510; f.root.value["journal"] = journal; QVERIFY(f.root.valid()); n.files[2].data = f.root.json(); n.files[2].rev += "-full"; }
	CloudWorkspaceSelection choice(choiceFile, google ? "google-drive" : "dropbox", Account); QVERIFY(choice.load()); QVERIFY(choice.select(choice.version(), f.reference)); const auto choiceVersion = choice.version(); const auto choiceBytes = read(choiceFile);
	CloudWorkspaceAuthority cp(cpFile, f.reference); QVERIFY(cp.observe(f.root, n.files[2].rev));
	if (mode == 23) { auto advanced = f.root; QVERIFY(advanced.registerUpload(uid(), f.head, CloudSyncManifest::sha256(B), B.size())); QVERIFY(cp.observe(advanced, "advanced-version")); }
	const auto cpBytes = read(cpFile); CloudWorkspaceReadIntent saved(intentFile, f.reference, commit, reservation);
	if (mode == 26 || mode == 27 || mode == 28) { QVERIFY(saved.prepare()); QVERIFY(saved.pin(f.snapshot)); }
	if (mode == 27 || mode == 28) {
		if (mode == 27) { QVERIFY(f.root.reserve(reservation, f.commit)); n.files[2].data = f.root.json(); n.files[2].rev += "-reserved"; }
		QVERIFY(saved.markReserved()); QVERIFY(saved.markVerified()); if (mode == 28) { QVERIFY(saved.beginRelease()); QVERIFY(saved.markReleased()); }
	}
	if (mode == 3) QVERIFY(QFile::remove(choiceFile)); if (mode == 5) QVERIFY(QFile::remove(cpFile));
	if (mode == 6 || mode == 7) { QFile q(mode == 6 ? cpFile : intentFile); QVERIFY(q.open(QIODevice::WriteOnly)); q.write("corrupt"); q.close(); }
	std::unique_ptr<QLockFile> lock; if (mode == 8) { lock = std::make_unique<QLockFile>(intentFile + ".lock"); QVERIFY(lock->tryLock(0)); }
	if (mode == 2) n.guard = false; if (mode == 11) n.files[3].data = B; if (mode == 12) n.files.removeAt(3); if (mode == 30) n.readFailureStatus = 401;
	if (mode == 22) { auto root = f.root.value; root["format"] = "future"; n.files[2].data = n.json(root); }
	if (mode == 38) n.files.removeAt(2); if (mode == 39) n.files[3].rev += "-changed";
	bool injected = false, injectionFailed = false, registeredBeforePayload = true; int requests = 0, updates = 0, creates = 0, payloadReads = 0, lookups = 0;
	const auto changeChoice = [&](bool back) { CloudWorkspaceSelection changed(choiceFile, google ? "google-drive" : "dropbox", Account); if (!changed.load()) { injectionFailed = true; return; } const auto ref = CloudWorkspaceReference::create(google ? "google-drive" : "dropbox", Account, f.reference.value["epoch"].toString(), google ? "other" : "id:other"); if (!changed.select(changed.version(), ref) || (back && !changed.select(changed.version(), f.reference))) injectionFailed = true; };
	const auto remoteRelease = [&] { auto root = CloudWorkspaceRecord::parse(n.files[2].data); QVERIFY(root.release(reservation)); n.files[2].data = root.json(); n.files[2].rev += "-released"; };
	const auto advanceHead = [&] { auto root = CloudWorkspaceRecord::parse(n.files[2].data); const auto id = uid(), parent = root.head(); QVERIFY(root.registerUpload(id, parent, CloudSyncManifest::sha256(B), B.size())); n.add(f.reference.payloadName(id), B); QVERIFY(root.completeUpload(id, google ? n.files.last().id.mid(3) : n.files.last().id, n.files.last().rev)); QVERIFY(root.publish(id, parent)); n.files[2].data = root.json(); n.files[2].rev += "-new-head"; };
	n.requested = [&](const QNetworkRequest &r) {
		++requests; const auto path = r.url().path(); const auto arg = QJsonDocument::fromJson(r.rawHeader("Dropbox-API-Arg")).object();
		const bool update = path.startsWith("/upload/drive/v2/") || (!google && path == "/2/files/upload" && arg["mode"].isObject()); if (update) { ++updates; if (mode == 9) n.loseWriteAck = true; }
		if (path == "/upload/drive/v3/files" || (!google && path == "/2/files/upload" && arg["mode"].toString() == "add")) ++creates;
		if (path == "/drive/v3/files") ++lookups;
		const bool target = google ? path.endsWith("/4") : arg["path"].toString() == "id:4";
		const bool data = target && (google ? path.startsWith("/drive/v3/") && QUrlQuery(r.url()).queryItemValue("alt") == "media" : path == "/2/files/download");
		if (data) { ++payloadReads; const auto root = CloudWorkspaceRecord::parse(n.files[2].data); const auto res = root.value["journal"].toObject()["reservations"].toObject()[reservation].toObject(); registeredBeforePayload &= res["commit"] == QJsonValue(commit) && res["active"] == QJsonValue(true); }
		if (mode == 13 && target && !injected) { n.deferReads = true; injected = true; }
	};
	n.wrote = [&](const QString &) {
		if (mode == 14) n.guard = false; if (mode == 15) changeChoice(false); if (mode == 16) injectionFailed = !QFile::remove(cpFile);
		if (mode == 19) remoteRelease();
	};
	n.response = [&](const QNetworkRequest &r, const QByteArray &bytes) {
		const auto path = r.url().path(); const auto arg = QJsonDocument::fromJson(r.rawHeader("Dropbox-API-Arg")).object();
		const bool data = google ? path == "/drive/v3/files/4" : path == "/2/files/download" && arg["path"].toString() == "id:4";
		if (data && !injected && (mode == 20 || mode == 21)) {
			injected = true; if (mode == 20) remoteRelease();
			else advanceHead();
		}
		return bytes;
	};
	if (mode == 10) n.metadataResponse = [&](const QByteArray &bytes) { auto data = QJsonDocument::fromJson(bytes).object(); if (data.value("id").toString() == (google ? "4" : "id:4")) data[google ? "title" : "name"] = "wrong.xml"; return n.json(data); };
	CloudWorkspaceRead client(&n); QSignalSpy done(&client, &CloudWorkspaceRead::finished), released(&client, &CloudWorkspaceRead::released), errors(&client, &CloudWorkspaceRead::failed);
	const auto start = [&](CloudWorkspaceRead::Action action) { client.start(action, "token", f.reference, commit, reservation, choiceFile, mode == 4 ? uid() : choiceVersion, cpFile, intentFile, [&] { return n.guard; }); };
	if (mode >= 31 && mode <= 37) {
		const auto path = (mode == 32 ? intentFile : mode == 33 ? cpFile : choiceFile) + ".lock"; QFile orphan(path); QVERIFY(orphan.open(QIODevice::ReadWrite)); if (mode == 34) QVERIFY(orphan.setFileTime(QDateTime::currentDateTimeUtc().addSecs(-60), QFileDevice::FileModificationTime)); orphan.close();
		if (mode != 34) QTimer::singleShot(40, &client, [&, path] { injectionFailed = !QFile::remove(path); if (mode == 36) changeChoice(true); if (mode == 37) n.guard = false; });
		if (mode == 35) QTimer::singleShot(20, &client, [&] { client.cancel(); });
	}
	start(CloudWorkspaceRead::Action::Read);
	if (mode == 35) { QTest::qWait(250); QCOMPARE(done.size() + errors.size(), 0); QCOMPARE(requests, 0); }
	else if (mode == 13) { QTRY_VERIFY(n.deferred); client.cancel(); if (n.deferred) n.deferred->complete(); n.deferReads = false; QTest::qWait(20); QCOMPARE(done.size() + errors.size(), 0); QVERIFY(CloudWorkspaceRecord::parse(n.files[2].data).value["journal"].toObject()["reservations"].toObject()[reservation].toObject()["active"].toBool()); start(CloudWorkspaceRead::Action::Read); QTRY_COMPARE(done.size(), 1); }
	else { QTRY_COMPARE(done.size() + errors.size(), 1); const bool success = mode == 0 || mode == 1 || mode == 9 || mode == 21 || mode == 26 || mode == 27 || (mode >= 31 && mode <= 34); QCOMPARE(done.size(), int(success)); }
	if (!done.isEmpty()) {
		const auto root = done.last()[0].value<QJsonObject>();
		CloudWorkspaceRecord result; result.value = root; QVERIFY(result.valid()); QCOMPARE(done.last()[4].toByteArray(), A); QCOMPARE(result.ticket(f.commit), f.root.ticket(f.commit)); QCOMPARE(result.ticket(f.pending), f.root.ticket(f.pending));
		const auto rs = result.value["journal"].toObject()["reservations"].toObject(); QCOMPARE(rs[f.protectedRead], f.root.value["journal"].toObject()["reservations"].toObject()[f.protectedRead]); QVERIFY(rs[reservation].toObject()["active"].toBool());
		QVERIFY(saved.reload()); QCOMPARE(saved.state(), QString("verified")); if (mode == 1) QCOMPARE(result.copySource(f.commit).json(), f.root.copySource(f.commit).json());
		if (mode == 0) {
			const auto writes = updates; start(CloudWorkspaceRead::Action::Read); QTRY_COMPARE(done.size(), 2); QCOMPARE(updates, writes);
			start(CloudWorkspaceRead::Action::Release); QTRY_COMPARE(released.size(), 1); QCOMPARE(updates, writes + 1); start(CloudWorkspaceRead::Action::Release); QTRY_COMPARE(released.size(), 2); QCOMPARE(updates, writes + 1);
			const auto count = requests; start(CloudWorkspaceRead::Action::Read); QCOMPARE(errors.size(), 1); QCOMPARE(requests, count);
		}
	}
	if (mode == 2 || mode == 3 || mode == 4 || mode == 5 || mode == 6 || mode == 7 || mode == 8 || mode == 28 || mode == 36 || mode == 37) QCOMPARE(requests, 0);
	if (mode == 14 || mode == 15 || mode == 16 || mode == 19) QCOMPARE(payloadReads, 0);
	if (mode == 5 || mode == 16) QVERIFY(!QFileInfo::exists(cpFile)); if (mode == 23) QCOMPARE(read(cpFile), cpBytes); if (mode == 30) QVERIFY(errors.first()[1].toBool());
	if (mode != 3 && mode != 15 && mode != 36) QCOMPARE(read(choiceFile), choiceBytes);
	QCOMPARE(n.files[0].data, originals[0].data); QCOMPARE(n.files[1].data, originals[1].data); QCOMPARE(creates, 0); QCOMPARE(lookups, 0); QCOMPARE(n.renames, 0); QVERIFY(n.allowed); QVERIFY(!injectionFailed); QVERIFY(registeredBeforePayload);
}
void TestCloudSyncHistory::workspaceReleaseWorkflow_data()
{
	QTest::addColumn<bool>("google"); QTest::addColumn<int>("mode");
	const QStringList cases = {"active-release-restart", "uncertain-registration-tombstone", "active-ack-lost", "absent-ack-lost", "conflict-retry", "read-releasing-no-revive", "prepared-refused", "missing-intent-refused", "corrupt-intent",
		"changed-pin", "foreign-reservation", "missing-checkpoint", "checkpoint-lost-after-write", "scope-before", "scope-after-write", "choice-after-write", "token-rejected", "cancel-resume", "empty-choice-lock", "empty-intent-lock", "empty-checkpoint-lock", "expired-empty-lock",
		"already-inactive", "local-released-restart", "tombstone-limit", "reservation-disappeared", "ticket-identity-collision", "late-registration-wins-then-fenced", "late-registration-loses-to-fence"};
	for (bool g : {false, true}) for (int mode = 0; mode < cases.size(); ++mode) QTest::newRow(qPrintable(QString("%1-reservation-release-%2").arg(g).arg(cases[mode]))) << g << mode;
}
void TestCloudSyncHistory::workspaceReleaseWorkflow()
{
	QFETCH(bool, google); QFETCH(int, mode); Network n; auto f = readFixture(n, google, true); QVERIFY(f.root.valid()); const auto originals = n.files;
	const auto reservation = mode == 26 ? f.head : uid(); const bool absent = mode == 1 || mode == 3 || mode == 24 || mode == 26 || mode == 27 || mode == 28;
	if (!absent) { QVERIFY(f.root.reserve(reservation, mode == 10 ? f.head : f.commit)); if (mode == 22 || mode == 23) QVERIFY(f.root.release(reservation)); }
	if (mode == 24) { auto journal = f.root.value["journal"].toObject(), rs = journal["reservations"].toObject(); while (rs.size() < 512) rs[uid()] = QJsonObject{{"commit", f.commit}, {"active", true}}; journal["reservations"] = rs; journal["sequence"] = journal["sequence"].toInt() + 510; f.root.value["journal"] = journal; }
	QVERIFY(f.root.valid()); n.files[2].data = f.root.json(); n.files[2].rev += "-release-fixture"; const auto originalVersion = n.files[2].rev;
	QTemporaryDir dir; QVERIFY(dir.isValid()); const auto choiceFile = dir.filePath("choice.json"), cpFile = dir.filePath("checkpoint.json"), intentFile = dir.filePath("read.json");
	const auto read = [](const QString &path) { QFile q(path); return q.open(QIODevice::ReadOnly) ? q.readAll() : QByteArray(); };
	CloudWorkspaceSelection choice(choiceFile, google ? "google-drive" : "dropbox", Account); QVERIFY(choice.load()); QVERIFY(choice.select(choice.version(), f.reference)); const auto choiceVersion = choice.version(); const auto choiceBytes = read(choiceFile);
	CloudWorkspaceAuthority cp(cpFile, f.reference); QVERIFY(cp.observe(f.root, originalVersion)); const auto cpBytes = read(cpFile);
	CloudWorkspaceReadIntent saved(intentFile, f.reference, f.commit, reservation);
	if (mode != 7) {
		QVERIFY(saved.prepare());
		if (mode != 6) { auto pin = f.snapshot; if (mode == 9) pin["version"] = "other-version"; QVERIFY(saved.pin(pin)); if (!absent) { QVERIFY(saved.markReserved()); QVERIFY(saved.markVerified()); } }
		if (mode == 5 || mode == 23) { QVERIFY(saved.beginRelease()); if (mode == 23) QVERIFY(saved.markReleased()); }
	}
	if (mode == 8) { QFile q(intentFile); QVERIFY(q.open(QIODevice::WriteOnly)); q.write("corrupt"); q.close(); }
	if (mode == 11) QVERIFY(QFile::remove(cpFile)); if (mode == 13) n.guard = false; if (mode == 16) n.readFailureStatus = 401;
	if (mode == 25) { auto root = f.root.value, journal = root["journal"].toObject(), rs = journal["reservations"].toObject(); rs.remove(reservation); journal["reservations"] = rs; journal["sequence"] = journal["sequence"].toInt() + 1; root["journal"] = journal; n.files[2].data = n.json(root); n.files[2].rev += "-missing"; }
	int requests = 0, updates = 0, creates = 0, payloadReads = 0; bool injected = false, injectionFailed = false;
	n.requested = [&](const QNetworkRequest &r) {
		++requests; const auto path = r.url().path(); const auto arg = QJsonDocument::fromJson(r.rawHeader("Dropbox-API-Arg")).object();
		const bool update = path.startsWith("/upload/drive/v2/") || (!google && path == "/2/files/upload" && arg["mode"].isObject());
		if (update) {
			++updates; if ((mode == 2 || mode == 3) && updates == 1) n.loseWriteAck = true; if (mode == 4 && updates == 1) n.fault = 4;
			if (mode == 27 && !injected) { auto late = CloudWorkspaceRecord::parse(n.files[2].data); QVERIFY(late.reserve(reservation, f.commit)); n.files[2].data = late.json(); n.files[2].rev += "-late"; injected = true; }
		}
		if (path == "/upload/drive/v3/files" || (!google && path == "/2/files/upload" && arg["mode"].toString() == "add")) ++creates;
		if (google ? path.endsWith("/4") : arg["path"].toString() == "id:4") ++payloadReads;
	};
	n.wrote = [&](const QString &) {
		if (mode == 12) injectionFailed = !QFile::remove(cpFile); if (mode == 14) n.guard = false;
		if (mode == 15) { CloudWorkspaceSelection changed(choiceFile, google ? "google-drive" : "dropbox", Account); QVERIFY(changed.load()); const auto ref = CloudWorkspaceReference::create(google ? "google-drive" : "dropbox", Account, uid(), google ? "other" : "id:other"); QVERIFY(changed.select(changed.version(), ref)); }
	};
	CloudWorkspaceRead client(&n); QSignalSpy done(&client, &CloudWorkspaceRead::finished), released(&client, &CloudWorkspaceRead::released), errors(&client, &CloudWorkspaceRead::failed);
	const auto start = [&](CloudWorkspaceRead::Action action) { client.start(action, "token", f.reference, f.commit, reservation, choiceFile, choiceVersion, cpFile, intentFile, [&] { return n.guard; }); };
	if (mode >= 18 && mode <= 21) {
		const auto path = (mode == 19 ? intentFile : mode == 20 ? cpFile : choiceFile) + ".lock"; QFile orphan(path); QVERIFY(orphan.open(QIODevice::ReadWrite)); if (mode == 21) QVERIFY(orphan.setFileTime(QDateTime::currentDateTimeUtc().addSecs(-60), QFileDevice::FileModificationTime)); orphan.close();
		if (mode != 21) QTimer::singleShot(40, &client, [&, path] { injectionFailed = !QFile::remove(path); });
	}
	if (mode == 17) n.deferReads = true;
	start(mode == 5 ? CloudWorkspaceRead::Action::Read : CloudWorkspaceRead::Action::Release);
	if (mode == 17) { QTRY_VERIFY(n.deferred); client.cancel(); if (n.deferred) n.deferred->complete(); n.deferReads = false; QTest::qWait(20); QCOMPARE(done.size() + released.size() + errors.size(), 0); start(CloudWorkspaceRead::Action::Release); QTRY_COMPARE(released.size(), 1); }
	else {
		QTRY_COMPARE(released.size() + errors.size(), 1); const bool success = mode <= 3 || (mode >= 18 && mode <= 23) || mode == 28; QCOMPARE(released.size(), int(success));
	}
	if (mode == 4 || mode == 27) { QVERIFY(saved.reload()); QCOMPARE(saved.state(), QString("releasing")); start(CloudWorkspaceRead::Action::Release); QTRY_COMPARE(released.size(), 1); }
	if (!released.isEmpty()) {
		const auto root = CloudWorkspaceRecord::parse(n.files[2].data); QVERIFY(root.valid()); QVERIFY(root.follows(f.root)); const auto rs = root.value["journal"].toObject()["reservations"].toObject();
		QCOMPARE(rs[reservation].toObject()["commit"].toString(), f.commit); QVERIFY(!rs[reservation].toObject()["active"].toBool()); QCOMPARE(root.copySource(f.commit).json(), f.root.copySource(f.commit).json()); QCOMPARE(root.ticket(f.pending), f.root.ticket(f.pending));
		QVERIFY(saved.reload()); QCOMPARE(saved.state(), QString("released")); const auto oldWrites = updates;
		if (mode == 0 || mode == 1) { start(CloudWorkspaceRead::Action::Release); QTRY_COMPARE(released.size(), 2); QCOMPARE(updates, oldWrites); const auto count = requests; start(CloudWorkspaceRead::Action::Read); QCOMPARE(errors.size(), 1); QCOMPARE(requests, count); }
		auto reused = root; QVERIFY(!reused.reserve(reservation, f.commit));
		if (mode == 28) {
			auto late = f.root; QVERIFY(late.reserve(reservation, f.commit)); CloudSyncFileStore delayed(&n); QSignalSpy conflict(&delayed, &CloudSyncFileStore::writeConflict);
			const auto bytes = n.files[2].data; delayed.uploadIdentity(google ? CloudSyncProviderType::GoogleDrive : CloudSyncProviderType::Dropbox, "token", f.reference.rootName(), late.json(), originalVersion, f.reference.value["root_id"].toString()); QTRY_COMPARE(conflict.size(), 1); QCOMPARE(n.files[2].data, bytes);
		}
	}
	if (mode == 5 || mode == 6 || mode == 7 || mode == 8 || mode == 11 || mode == 13) QCOMPARE(requests, 0);
	if (mode == 9 || mode == 10 || mode == 24 || mode == 25 || mode == 26) QCOMPARE(updates, 0);
	if (mode == 11 || mode == 12) QVERIFY(!QFileInfo::exists(cpFile)); if (mode == 25) QCOMPARE(read(cpFile), cpBytes); if (mode == 16) QVERIFY(errors.first()[1].toBool());
	if (mode != 15) QCOMPARE(read(choiceFile), choiceBytes);
	QCOMPARE(done.size(), 0); QCOMPARE(payloadReads, 0); QCOMPARE(creates, 0); QVERIFY(n.allowed); QVERIFY(!injectionFailed); QCOMPARE(n.renames, 0);
	QCOMPARE(n.files[0].data, originals[0].data); QCOMPARE(n.files[1].data, originals[1].data); QCOMPARE(n.files[3].data, originals[3].data); QCOMPARE(n.files[4].data, originals[4].data);
}

void TestCloudSyncHistory::workspaceRestoreIntent_data()
{
	QTest::addColumn<bool>("google"); QTest::addColumn<int>("mode");
	const QStringList cases = {"durable-phases", "changed-source", "changed-reservation", "changed-object", "changed-revision", "ordinary-mode-refused", "source-is-ticket", "reservation-is-ticket", "reservation-is-source", "reservation-is-parent", "wrong-name", "root-as-payload", "bad-revision", "changed-hash", "changed-size", "extra-proof", "extra-snapshot", "invalid-source", "invalid-reservation", "corrupt", "future-format", "missing-proof", "ordinary-file-refused", "live-lock"};
	for (bool g : {false, true}) for (int mode=0; mode<cases.size(); ++mode) QTest::newRow(qPrintable(QString("%1-restore-intent-%2").arg(g).arg(cases[mode]))) << g << mode;
}
void TestCloudSyncHistory::workspaceRestoreIntent()
{
	QFETCH(bool, google); QFETCH(int, mode); Network n; const auto f=readFixture(n, google); QVERIFY(f.root.valid()); QTemporaryDir dir; QVERIFY(dir.isValid());
	const auto file=dir.filePath("restore.json"), ticket=uid(), reservation=uid(), hash=CloudSyncManifest::sha256(A);
	const QJsonObject proof={{"source", f.commit}, {"reservation", reservation}, {"snapshot", f.snapshot}};
	const auto read=[&] { QFile q(file); return q.open(QIODevice::ReadOnly) ? q.readAll() : QByteArray(); };
	CloudWorkspaceUploadIntent saved(file, f.reference, hash, A.size(), ticket, f.head, mode==22 ? QJsonObject() : proof); QVERIFY(saved.prepare()); const auto before=read();
	if (mode>=19 && mode<=21) {
		auto bad=QJsonDocument::fromJson(before).object(); if (mode==20) bad["format"]="future"; if (mode==21) bad.remove("restore");
		QFile q(file); QVERIFY(q.open(QIODevice::WriteOnly)); q.write(mode==19 ? QByteArray("corrupt") : QJsonDocument(bad).toJson(QJsonDocument::Compact)); q.close(); const auto bytes=read();
		QVERIFY(!saved.prepare()); QVERIFY(!saved.reload()); QCOMPARE(read(), bytes); return;
	}
	if (mode==23) { QLockFile lock(file+".lock"); QVERIFY(lock.tryLock(0)); QVERIFY(saved.inspect()); QVERIFY(!saved.reload()); QVERIFY(!saved.beginRegistration()); QCOMPARE(read(),before); return; }
	auto changed=proof; auto pin=f.snapshot; QString otherTicket=ticket, parent=f.head;
	if (mode==1) changed["source"]=uid(); if (mode==2) changed["reservation"]=uid(); if (mode==3) pin["object"]=google ? "other" : "id:other"; if (mode==4) pin["version"]="other";
	if (mode==6) otherTicket=f.commit; if (mode==7) otherTicket=reservation; if (mode==8) changed["reservation"]=f.commit; if (mode==9) parent=reservation;
	if (mode==10) pin["name"]="other.xml"; if (mode==11) pin["object"]=f.reference.value["root_id"]; if (mode==12) pin["version"]="bad\nversion"; if (mode==13) pin["sha256"]=CloudSyncManifest::sha256(B); if (mode==14) pin["bytes"]=A.size()+1;
	if (mode==15) changed["extra_fence"]=true; if (mode==16) pin["extra_fence"]=true; if (mode==17) changed["source"]="bad"; if (mode==18) changed["reservation"]="bad";
	changed["snapshot"]=pin; if (mode==5) changed={};
	CloudWorkspaceUploadIntent other(file,f.reference,hash,A.size(),otherTicket,parent,changed);
	if (mode!=0) { QVERIFY(!other.prepare()); QVERIFY(!other.reload()); QCOMPARE(read(),before); return; }
	QVERIFY(other.reload()); QVERIFY(other.beginRegistration()); QVERIFY(other.registerTicket()); QVERIFY(other.submit()); QVERIFY(!other.submit());
	QVERIFY(other.identify(google ? "new-copy" : "id:new-copy")); QVERIFY(other.verify("new-copy-version")); QVERIFY(other.upload()); QVERIFY(other.commit());
	CloudWorkspaceUploadIntent restart(file,f.reference,hash,A.size(),ticket,f.head,proof); QVERIFY(restart.reload()); QCOMPARE(restart.state(),QString("committed"));
	const auto complete=read(); QVERIFY(restart.prepare()); QVERIFY(!restart.submit()); QCOMPARE(read(),complete); QVERIFY(!complete.contains(A)); QVERIFY(!complete.contains("token"));
}


void TestCloudSyncHistory::workspaceRestoreWorkflow_data()
{
	QTest::addColumn<bool>("google"); QTest::addColumn<int>("mode");
	const QStringList cases={"restore-restart", "copy-provenance-preserved", "missing-owned-read", "unverified-read", "released-read", "releasing-read", "wrong-supplied-bytes", "missing-reservation", "inactive-reservation", "wrong-source-bytes", "changed-source-revision", "source-changed-after-registration", "source-changed-before-publication", "scope-after-registration", "scope-after-create-ack", "stale-choice", "missing-checkpoint", "registration-ack-lost", "payload-ack-lost", "upload-ack-lost", "publication-ack-lost", "cancel-identified-resume", "cancel-uploaded-resume", "cancel-source-read-resume", "competing-head-before-create", "competing-head-before-publish", "later-head-at-completion", "older-completed-restart", "completed-restart-after-release", "ordinary-intent-adoption-refused", "ordinary-ticket-adoption-refused", "local-pin-changed", "owned-read-lost-after-ack", "release-fence-stops-pending-restore", "foreign-owned-read", "future-root", "ack-points-at-source", "incomplete-read-lock-recovery", "cancel-lock-wait", "scope-during-second-source-read", "rejected-token", "future-intent", "actual-read-then-restore", "missing-restore-arguments"};
	for (bool g:{false,true}) for(int mode=0;mode<cases.size();++mode) QTest::newRow(qPrintable(QString("%1-workspace-restore-%2").arg(g).arg(cases[mode]))) << g << mode;
}
void TestCloudSyncHistory::workspaceRestoreWorkflow()
{
	QFETCH(bool,google); QFETCH(int,mode); Network n; auto f=readFixture(n,google,mode==1); QVERIFY(f.root.valid()); n.workspaceUploads=true;
	const auto originals=n.files; const auto provider=QString(google ? "google-drive" : "dropbox"), ticket=uid(), reservation=uid();
	QTemporaryDir dir; QVERIFY(dir.isValid()); const auto choiceFile=dir.filePath("choice.json"), cpFile=dir.filePath("checkpoint.json"), file=dir.filePath("restore.json"), readFile=dir.filePath("read.json");
	const auto read=[](const QString &path) { QFile q(path); return q.open(QIODevice::ReadOnly) ? q.readAll() : QByteArray(); };
	const auto write=[](const QString &path,const QJsonObject &value) { QFile q(path); return q.open(QIODevice::WriteOnly) && q.write(QJsonDocument(value).toJson(QJsonDocument::Compact))>0; };
	if(mode!=42) { QVERIFY(f.root.reserve(reservation,f.commit)); n.files[2].data=f.root.json(); n.files[2].rev+="-reserved"; }
	if(mode==7) { auto body=f.root.value; auto journal=body["journal"].toObject(); auto reservations=journal["reservations"].toObject(); reservations.remove(reservation); journal["reservations"]=reservations; body["journal"]=journal; f.root.value=body; QVERIFY(f.root.valid()); n.files[2].data=f.root.json(); }
	if(mode==8) { QVERIFY(f.root.release(reservation)); n.files[2].data=f.root.json(); }
	CloudWorkspaceSelection choice(choiceFile,provider,Account); QVERIFY(choice.load()); QVERIFY(choice.select(choice.version(),f.reference)); const auto choiceVersion=choice.version(); const auto choiceBytes=read(choiceFile);
	CloudWorkspaceAuthority checkpoint(cpFile,f.reference); QVERIFY(checkpoint.observe(f.root,n.files[2].rev));
	CloudWorkspaceReadIntent owned(readFile,f.reference,f.commit,reservation);
	if(mode==42) {
		CloudWorkspaceRead reader(&n); QSignalSpy done(&reader,&CloudWorkspaceRead::finished), errors(&reader,&CloudWorkspaceRead::failed);
		reader.start(CloudWorkspaceRead::Action::Read,"token",f.reference,f.commit,reservation,choiceFile,choiceVersion,cpFile,readFile,[&]{return n.guard;});
		QTRY_COMPARE(done.size()+errors.size(),1); QCOMPARE(done.size(),1); QCOMPARE(done[0][4].toByteArray(),A); QVERIFY(owned.reload()); QCOMPARE(owned.state(),QString("verified")); f.root=CloudWorkspaceRecord::parse(n.files[2].data);
	} else { QVERIFY(owned.prepare()); if(mode!=3) { QVERIFY(owned.pin(f.snapshot)); QVERIFY(owned.markReserved()); QVERIFY(owned.markVerified()); } }
	if(mode==2) QVERIFY(QFile::remove(readFile));
	if(mode==4 || mode==5) { QVERIFY(owned.beginRelease()); if(mode==4) QVERIFY(owned.markReleased()); }
	if(mode==16) QVERIFY(QFile::remove(cpFile));
	if(mode==34) { auto foreign=QJsonDocument::fromJson(read(readFile)).object(); auto ref=f.reference.value; ref["account"]=CloudSyncManifest::sha256("foreign"); foreign["workspace"]=ref; QVERIFY(write(readFile,foreign)); }
	if(mode==35) { auto future=f.root.value; future["format"]="future"; n.files[2].data=n.json(future); }
	const QJsonObject proof={{"source",f.commit},{"reservation",reservation},{"snapshot",f.snapshot}};
	if(mode==41) { CloudWorkspaceUploadIntent intent(file,f.reference,CloudSyncManifest::sha256(A),A.size(),ticket,f.head,proof); QVERIFY(intent.prepare()); auto bad=QJsonDocument::fromJson(read(file)).object(); bad["format"]="future"; QVERIFY(write(file,bad)); }
	if(mode==9) n.files[3].data=B; if(mode==10) n.files[3].rev+="-changed"; if(mode==40) n.readFailureStatus=401;
	bool injectionFailed=false, injected=false; int creates=0,updates=0,requests=0,lookups=0,sourceReads=0;
	const auto addOther=[&] { auto root=CloudWorkspaceRecord::parse(n.files[2].data); const auto id=uid(), parent=root.head(); QVERIFY(root.registerUpload(id,parent,CloudSyncManifest::sha256(B),B.size())); n.add(f.reference.payloadName(id),B); QVERIFY(root.completeUpload(id,google ? n.files.last().id.mid(3) : n.files.last().id,n.files.last().rev)); QVERIFY(root.publish(id,parent)); n.files[2].data=root.json(); n.files[2].rev+="-competing"; };
	n.requested=[&](const QNetworkRequest &r) {
		++requests; const auto path=r.url().path(); const auto arg=QJsonDocument::fromJson(r.rawHeader("Dropbox-API-Arg")).object();
		const bool create=path=="/upload/drive/v3/files" || (!google && path=="/2/files/upload" && arg["mode"].toString()=="add");
		const bool update=path.startsWith("/upload/drive/v2/") || (!google && path=="/2/files/upload" && arg["mode"].isObject());
		if(path=="/drive/v3/files") ++lookups; if(create) { ++creates; if(mode==18) n.loseWriteAck=true; }
		if(update) { ++updates; if((mode==17 && updates==1)||(mode==19 && updates==2)||(mode==20 && updates==3)) n.loseWriteAck=true; }
		const bool sourceRead=google ? path.endsWith("/4") && QUrlQuery(r.url()).queryItemValue("alt")=="media" : path=="/2/files/download" && arg["path"].toString()=="id:4";
		if(sourceRead) { ++sourceReads; if(mode==39 && sourceReads==2) n.guard=false; }
		const auto index=n.find(f.reference.payloadName(ticket)); const bool payloadRead=index>=0 && (google ? path.endsWith("/"+n.files[index].id.mid(3)) : arg["path"].toString()==n.files[index].id);
		if(!injected && ((mode==21 && creates==1 && payloadRead && !create && !update)||(mode==22 && updates==2 && !create && !update)||(mode==23 && sourceRead))) { n.deferReads=true; injected=true; }
	};
	n.wrote=[&](const QString &name) {
		if(!name.endsWith(".json")) return;
		if((mode==11 && updates==1)||(mode==12 && updates==2)) { n.files[3].data=B; n.files[3].rev+="-changed"; }
		if(mode==13 && updates==1) n.guard=false;
		if((mode==24 && updates==1)||(mode==25 && updates==2)||(mode==26 && updates==3)) addOther();
		if(mode==31 && updates==2) { auto bad=QJsonDocument::fromJson(read(readFile)).object(); auto pin=bad["snapshot"].toObject(); pin["version"]="changed-local-pin"; bad["snapshot"]=pin; injectionFailed=!write(readFile,bad); }
		if(mode==33 && updates==1) injectionFailed=!owned.beginRelease();
	};
	n.response=[&](const QNetworkRequest &r,const QByteArray &bytes) {
		const auto path=r.url().path(); const auto arg=QJsonDocument::fromJson(r.rawHeader("Dropbox-API-Arg")).object();
		const bool create=path=="/upload/drive/v3/files" || (!google && path=="/2/files/upload" && arg["mode"].toString()=="add");
		if(create) { if(mode==14) n.guard=false; if(mode==32) injectionFailed=!QFile::remove(readFile); if(mode==36) { auto response=QJsonDocument::fromJson(bytes).object(); response["id"]=google ? "4" : "id:4"; return n.json(response); } }
		return bytes;
	};
	CloudWorkspaceUpload client(&n); QSignalSpy done(&client,&CloudWorkspaceUpload::finished),errors(&client,&CloudWorkspaceUpload::failed);
	const auto start=[&] { client.startRestore("token",f.reference,mode==6 ? B : A,choiceFile,mode==15 ? uid() : choiceVersion,cpFile,file,ticket,f.head,mode==43 ? QString() : readFile,mode==43 ? QString() : f.commit,mode==43 ? QString() : reservation,[&]{return n.guard;}); };
	if(mode==37 || mode==38) { QFile lock(readFile+".lock"); QVERIFY(lock.open(QIODevice::WriteOnly)); lock.close(); QTimer::singleShot(40,&client,[&]{injectionFailed=!QFile::remove(readFile+".lock");}); if(mode==38) QTimer::singleShot(20,&client,[&]{client.cancel();}); }
	start();
	if(mode==38) { QTest::qWait(200); QCOMPARE(done.size()+errors.size(),0); QCOMPARE(requests,0); }
	else if(mode==21 || mode==22 || mode==23) { QTRY_VERIFY(n.deferred); client.cancel(); if(n.deferred) n.deferred->complete(); n.deferReads=false; QTest::qWait(20); QCOMPARE(done.size()+errors.size(),0); start(); QTRY_COMPARE(done.size()+errors.size(),1); QCOMPARE(done.size(),1); }
	else { QTRY_COMPARE(done.size()+errors.size(),1); const bool ok=mode==0 || mode==1 || mode==17 || mode==19 || mode==20 || (mode>=26 && mode<=30) || mode==37 || mode==42; QCOMPARE(done.size(),int(ok)); }
	if(!done.isEmpty()) {
		auto root=CloudWorkspaceRecord::parse(n.files[2].data); QVERIFY(root.valid()); QVERIFY(root.follows(f.root)); const auto t=root.ticket(ticket);
		QCOMPARE(t["restore_source"].toString(),f.commit); QCOMPARE(t["reservation"].toString(),reservation); QCOMPARE(t["state"].toString(),QString("committed")); QCOMPARE(t["parent"].toString(),f.head); QCOMPARE(n.files[n.find(f.reference.payloadName(ticket))].data,A); QVERIFY(t["object"]!=f.snapshot["object"]); QCOMPARE(done.last()[3].toBool(),mode!=26); QCOMPARE(creates,1); QCOMPARE(updates,3); QCOMPARE(root.ticket(f.pending),f.root.ticket(f.pending)); QCOMPARE(root.copySource(f.commit).json(),f.root.copySource(f.commit).json());
		QVERIFY(owned.reload()); QCOMPARE(owned.state(),QString("verified")); QVERIFY(root.value["journal"].toObject()["reservations"].toObject()[reservation].toObject()["active"].toBool());
		const auto mutations=n.mutations;
		if(mode==0 || mode==27 || mode==28) {
			if(mode==27) addOther();
			if(mode==28) { CloudWorkspaceRead release(&n); QSignalSpy released(&release,&CloudWorkspaceRead::released),failed(&release,&CloudWorkspaceRead::failed); release.start(CloudWorkspaceRead::Action::Release,"token",f.reference,f.commit,reservation,choiceFile,choiceVersion,cpFile,readFile,[&]{return n.guard;}); QTRY_COMPARE(released.size()+failed.size(),1); QCOMPARE(released.size(),1); }
			const auto writes=n.mutations; start(); QTRY_COMPARE(done.size(),2); QCOMPARE(n.mutations,writes); QCOMPARE(done.last()[3].toBool(),mode!=27);
		}
		if(mode==29) { client.start("token",f.reference,A,choiceFile,choiceVersion,cpFile,file,ticket,f.head,[&]{return n.guard;}); QTRY_COMPARE(errors.size(),1); QCOMPARE(n.mutations,mutations); }
		if(mode==30) {
			const auto ordinary=dir.filePath("ordinary.json"); CloudWorkspaceUploadIntent forged(ordinary,f.reference,CloudSyncManifest::sha256(A),A.size(),ticket,f.head); QVERIFY(forged.prepare()); QVERIFY(forged.beginRegistration()); QVERIFY(forged.registerTicket()); QVERIFY(forged.submit()); QVERIFY(forged.identify(t["object"].toString())); QVERIFY(forged.verify(t["version"].toString())); QVERIFY(forged.upload());
			client.start("token",f.reference,A,choiceFile,choiceVersion,cpFile,ordinary,ticket,f.head,[&]{return n.guard;}); QTRY_COMPARE(errors.size(),1); QCOMPARE(n.mutations,mutations);
		}
	}
	if(mode==18) { const auto count=requests; start(); QCOMPARE(errors.size(),2); QCOMPARE(requests,count); QCOMPARE(creates,1); }
	if(mode==14 || mode==32) { CloudWorkspaceUploadIntent saved(file,f.reference,CloudSyncManifest::sha256(A),A.size(),ticket,f.head,proof); QVERIFY(saved.reload()); QVERIFY(!saved.id().isEmpty()); }
	if(mode==2 || mode==3 || mode==4 || mode==5 || mode==6 || mode==15 || mode==16 || mode==34 || mode==41 || mode==43) QCOMPARE(requests,0);
	if(mode==7 || mode==8 || mode==9 || mode==10 || mode==35) { QCOMPARE(creates,0); QCOMPARE(updates,0); }
	if(mode==11 || mode==12 || mode==13 || mode==14 || mode==31 || mode==32 || mode==33 || mode==36 || mode==39) QCOMPARE(CloudWorkspaceRecord::parse(n.files[2].data).head(),f.head);
	if(mode==24 || mode==25) { const auto root=CloudWorkspaceRecord::parse(n.files[2].data); QVERIFY(root.head()!=f.head); QVERIFY(root.head()!=ticket); QCOMPARE(creates,mode==24 ? 0 : 1); }
	if(mode==33) { auto root=CloudWorkspaceRecord::parse(n.files[2].data); QVERIFY(!root.release(reservation)); QVERIFY(owned.reload()); QCOMPARE(owned.state(),QString("releasing")); }
	if(mode==40) QVERIFY(errors.first()[1].toBool());
	QCOMPARE(read(choiceFile),choiceBytes); QCOMPARE(n.files[0].data,originals[0].data); QCOMPARE(n.files[1].data,originals[1].data); QCOMPARE(n.files[4].data,originals[4].data); QVERIFY(n.allowed); QVERIFY(!injectionFailed); QCOMPARE(n.renames,0); QCOMPARE(lookups,0);
}


namespace {
struct ApplyFixture {
	ReadFixture base;
	QString commit, sourceReservation, reservation;
};
ApplyFixture applyFixture(Network &n, bool google, bool copied = true)
{
	ApplyFixture f; f.base=readFixture(n,google,copied); if(!f.base.root.valid()) return {};
	f.commit=uid(); f.sourceReservation=uid(); f.reservation=uid(); auto &root=f.base.root;
	if(!root.reserve(f.sourceReservation,f.base.commit) || !root.registerUpload(f.commit,f.base.head,CloudSyncManifest::sha256(A),A.size(),f.base.commit,f.sourceReservation)) return {};
	n.add(f.base.reference.payloadName(f.commit),A);
	if(!root.completeUpload(f.commit,google ? n.files.last().id.mid(3) : n.files.last().id,n.files.last().rev) || !root.publish(f.commit,f.base.head)) return {};
	n.files[2].data=root.json(); n.files[2].rev+="-restore"; return f;
}
}
void TestCloudSyncHistory::workspaceApplyIntent_data()
{
	QTest::addColumn<bool>("google"); QTest::addColumn<int>("mode");
	const QStringList cases={"durable-application-and-persistence", "wrong-before", "wrong-after", "foreign-root", "changed-choice", "changed-commit", "changed-reservation", "unowned-before-file", "unrelated-directory-file", "future-format", "unknown-field", "future-state", "corrupt-metadata", "missing-before", "missing-after", "changed-before", "changed-after", "prepared-missing-after-recovery", "prepared-missing-both-recovery", "live-lock-inspection", "stale-head", "released-source", "released-application-reservation", "unowned-after-file", "blocked-directory", "invalid-before-xml", "invalid-after-xml", "dtd-after-xml", "oversized-metadata", "no-state-regression", "wrong-apply-observation", "wrong-persist-observation"};
	for(bool g:{false,true}) for(int mode=0;mode<cases.size();++mode) QTest::newRow(qPrintable(QString("%1-apply-intent-%2").arg(g).arg(cases[mode]))) << g << mode;
}
void TestCloudSyncHistory::workspaceApplyIntent()
{
	QFETCH(bool,google); QFETCH(int,mode); Network n; auto f=applyFixture(n,google,mode!=26 && mode!=27); QVERIFY(f.base.root.valid()); auto root=f.base.root;
	QVERIFY(root.reserve(f.reservation,f.commit)); n.files[2].data=root.json();
	QTemporaryDir temp; QVERIFY(temp.isValid()); const auto dir=temp.filePath("recovery"), choice=uid(), file=QDir(dir).filePath("intent.json");
	const auto read=[](const QString &p) { QFile q(p); return q.open(QIODevice::ReadOnly) ? q.readAll() : QByteArray(); };
	const auto write=[](const QString &p,const QByteArray &b) { QFile q(p); return q.open(QIODevice::WriteOnly) && q.write(b)==b.size(); };
	CloudWorkspaceApplyIntent saved(dir,f.base.reference,choice,f.commit,f.reservation);
	if(mode==7 || mode==8 || mode==23) { QVERIFY(QDir().mkpath(dir)); const auto path=QDir(dir).filePath(mode==7 ? "before.xml" : mode==23 ? "after.xml" : "unrelated.txt"); const auto bytes=mode==7 ? B : mode==23 ? A : QByteArray("keep"); QVERIFY(write(path,bytes)); QVERIFY(!saved.prepare(B,A,root)); QCOMPARE(read(path),bytes); QVERIFY(!QFileInfo::exists(file)); return; }
	if(mode==24) { QVERIFY(write(dir,"keep")); QVERIFY(!saved.prepare(B,A,root)); QCOMPARE(read(dir),QByteArray("keep")); return; }
	if(mode==20) { auto other=uid(); QVERIFY(root.registerUpload(other,root.head(),CloudSyncManifest::sha256(B),B.size())); QVERIFY(root.completeUpload(other,google ? "extra" : "id:extra","extra-version")); QVERIFY(root.publish(other,f.commit)); }
	if(mode==21) QVERIFY(root.release(f.sourceReservation)); if(mode==22) QVERIFY(root.release(f.reservation));
	auto before=B,after=A;
	if(mode>=25 && mode<=27) {
		auto body=root.value; auto j=body["journal"].toObject(); auto tickets=j["tickets"].toObject();
		const auto replace=[&](const QString &id,const QByteArray &bytes) { auto t=tickets[id].toObject(); t["sha256"]=CloudSyncManifest::sha256(bytes); t["bytes"]=bytes.size(); tickets[id]=t; };
		if(mode==25) { before="<divelog><dives>"; replace(f.base.head,before); }
		else { after=mode==26 ? QByteArray("<divelog><dives>") : QByteArray("<!DOCTYPE divelog><divelog><dives/></divelog>"); replace(f.base.commit,after); replace(f.commit,after); }
		j["tickets"]=tickets; body["journal"]=j; root.value=body; QVERIFY(root.valid());
	}
	if(mode==1 || mode==2 || (mode>=20 && mode<=22) || (mode>=25 && mode<=27)) { QVERIFY(!saved.prepare(mode==1 ? A : before,mode==2 ? B : after,root)); QVERIFY(!QFileInfo::exists(file)); return; }
	QVERIFY(saved.prepare(B,A,root)); QCOMPARE(saved.state(),QString("ready")); QCOMPARE(saved.before(),B); QCOMPARE(saved.after(),A); const auto initial=read(file);
	if(mode>=3 && mode<=6) { auto ref=f.base.reference; if(mode==3) ref=CloudWorkspaceReference::create(google ? "google-drive" : "dropbox",Account,ref.value["epoch"].toString(),google ? "other" : "id:other"); CloudWorkspaceApplyIntent other(dir,ref,mode==4 ? uid() : choice,mode==5 ? uid() : f.commit,mode==6 ? uid() : f.reservation); QVERIFY(!other.inspect()); QVERIFY(!other.prepare(B,A,root)); QCOMPARE(read(file),initial); return; }
	if((mode>=9 && mode<=12) || mode==28) { auto bad=QJsonDocument::fromJson(initial).object(); if(mode==9) bad["format"]="future"; if(mode==10) bad["future_safety_field"]=true; if(mode==11) bad["state"]="expired"; const auto bytes=mode==12 ? QByteArray("corrupt") : mode==28 ? QByteArray(16385,'x') : QJsonDocument(bad).toJson(QJsonDocument::Compact); QVERIFY(write(file,bytes)); QVERIFY(!saved.inspect()); QVERIFY(!saved.prepare(B,A,root)); QCOMPARE(read(file),bytes); return; }
	if(mode>=13 && mode<=16) { const auto path=QDir(dir).filePath(mode==13 || mode==15 ? "before.xml" : "after.xml"); if(mode<=14) QVERIFY(QFile::remove(path)); else { auto changed=read(path); changed[changed.size()/2]=char(changed.at(changed.size()/2)^1); QVERIFY(write(path,changed)); } const auto bytes=read(path); QVERIFY(!saved.inspect()); QVERIFY(!saved.beginApply()); QCOMPARE(read(path),bytes); QCOMPARE(read(file),initial); return; }
	if(mode==17 || mode==18) { auto prepared=QJsonDocument::fromJson(initial).object(); prepared["state"]="prepared"; QVERIFY(write(file,QJsonDocument(prepared).toJson(QJsonDocument::Compact))); QVERIFY(QFile::remove(QDir(dir).filePath("after.xml"))); if(mode==18) QVERIFY(QFile::remove(QDir(dir).filePath("before.xml"))); QVERIFY(saved.inspect()); QCOMPARE(saved.state(),QString("prepared")); QVERIFY(saved.prepare(B,A,root)); QCOMPARE(saved.before(),B); QCOMPARE(saved.after(),A); }
	if(mode==19) { QLockFile lock(file+".lock"); QVERIFY(lock.tryLock(0)); QVERIFY(saved.inspect()); QVERIFY(!saved.reload()); QVERIFY(!saved.beginApply()); QCOMPARE(read(file),initial); return; }
	QVERIFY(!saved.persisted(A)); QVERIFY(!saved.applied(A)); QVERIFY(saved.beginApply()); const auto applying=read(file);
	if(mode==30) { QVERIFY(!saved.applied(B)); QCOMPARE(read(file),applying); return; }
	QVERIFY(saved.applied(A)); const auto applied=read(file);
	if(mode==31) { QVERIFY(!saved.persisted(B)); QCOMPARE(read(file),applied); return; }
	if(mode==29) { QVERIFY(!saved.beginApply()); QCOMPARE(read(file),applied); }
	QVERIFY(saved.persisted(A)); const auto complete=read(file); QVERIFY(!saved.beginApply()); QVERIFY(!saved.applied(A)); QVERIFY(saved.persisted(A));
	CloudWorkspaceApplyIntent restart(dir,f.base.reference,choice,f.commit,f.reservation); QVERIFY(restart.inspect()); QCOMPARE(restart.state(),QString("persisted")); QCOMPARE(restart.before(),B); QCOMPARE(restart.after(),A); QVERIFY(restart.prepare(B,A,root)); QCOMPARE(read(file),complete); QVERIFY(!complete.contains(A)); QVERIFY(!complete.contains("token"));
}


void TestCloudSyncHistory::workspaceApplyWorkflow_data()
{
	QTest::addColumn<bool>("google"); QTest::addColumn<int>("mode");
	const QStringList cases={"apply-persist-restart", "application-without-persistence-ack", "apply-callback-failed", "apply-callback-did-not-replace", "later-edit-in-callback", "initial-local-divergence", "local-edit-during-read", "scope-after-reservation", "scope-before-start", "stale-choice", "missing-checkpoint", "corrupt-recovery", "missing-owned-upload", "missing-owned-source", "unconfirmed-upload", "released-owned-source", "changed-owned-source-pin", "ordinary-version-refused", "later-head-before-read", "later-head-during-read", "corrupt-restored-payload", "cancel-read", "cancel-before-persistence", "scope-after-apply-recovery", "choice-aba-after-apply", "restart-applying-with-before", "restart-applying-with-after", "restart-applied-with-before", "restart-applied-with-after", "restart-applied-later-edit", "restart-before-with-later-head", "restart-after-with-later-head", "completed-with-later-local-edit", "completed-after-reservation-release", "completed-backup-corrupt", "missing-ready-backup", "interrupted-recovery-copy", "live-operation-lock", "incomplete-operation-lock", "incomplete-receipt-lock", "cancel-lock-wait", "rejected-token", "missing-choice", "choice-aba-during-read", "local-edit-before-persistence-ack", "root-failure-after-persistence-restart", "head-advance-during-persistence", "local-edit-during-final-observation", "cancel-inside-apply", "cancel-inside-local-snapshot", "prepared-recovery-before-both-copies"};
	for(bool g:{false,true}) for(int mode=0;mode<cases.size();++mode) QTest::newRow(qPrintable(QString("%1-apply-workflow-%2").arg(g).arg(cases[mode]))) << g << mode;
}
void TestCloudSyncHistory::workspaceApplyWorkflow()
{
	QFETCH(bool,google); QFETCH(int,mode); Network n; auto f=applyFixture(n,google); QVERIFY(f.base.root.valid()); const auto originals=n.files;
	const auto provider=QString(google ? "google-drive" : "dropbox"); if(mode==17) f.commit=f.base.head;
	QTemporaryDir temp; QVERIFY(temp.isValid()); const auto choiceFile=temp.filePath("choice.json"), cpFile=temp.filePath("checkpoint.json"), sourceFile=temp.filePath("source.json"), uploadFile=temp.filePath("upload.json"), readFile=temp.filePath("reader.json"), directory=temp.filePath("recovery"), diskFile=temp.filePath("local.xml"), receipt=QDir(directory).filePath("intent.json");
	const auto read=[](const QString &p) { QFile q(p); return q.open(QIODevice::ReadOnly) ? q.readAll() : QByteArray(); };
	const auto write=[](const QString &p,const QByteArray &b) { QFile q(p); return q.open(QIODevice::WriteOnly) && q.write(b)==b.size(); };
	CloudWorkspaceSelection choice(choiceFile,provider,Account); QVERIFY(choice.load()); QVERIFY(choice.select(choice.version(),f.base.reference)); const auto choiceVersion=choice.version(); const auto choiceBytes=read(choiceFile);
	const auto sourcePin=f.base.snapshot; const auto newTicket=f.base.root.ticket(f.commit);
	const QJsonObject newPin={{"name",newTicket["name"]},{"object",newTicket["object"]},{"version",newTicket["version"]},{"sha256",newTicket["sha256"]},{"bytes",newTicket["bytes"]}};
	CloudWorkspaceReadIntent source(sourceFile,f.base.reference,f.base.commit,f.sourceReservation); QVERIFY(source.prepare()); QVERIFY(source.pin(sourcePin)); QVERIFY(source.markReserved()); QVERIFY(source.markVerified());
	const QJsonObject restore={{"source",f.base.commit},{"reservation",f.sourceReservation},{"snapshot",sourcePin}};
	CloudWorkspaceUploadIntent upload(uploadFile,f.base.reference,CloudSyncManifest::sha256(A),A.size(),f.commit,f.base.head,restore);
	if(mode!=17) { QVERIFY(upload.prepare()); QVERIFY(upload.beginRegistration()); QVERIFY(upload.registerTicket()); QVERIFY(upload.submit()); QVERIFY(upload.identify(newTicket["object"].toString())); QVERIFY(upload.verify(newTicket["version"].toString())); QVERIFY(upload.upload()); if(mode!=14) QVERIFY(upload.commit()); }
	CloudWorkspaceReadIntent application(readFile,f.base.reference,f.commit,f.reservation);
	CloudWorkspaceApplyIntent saved(directory,f.base.reference,choiceVersion,f.commit,f.reservation);
	const bool existing=(mode>=25 && mode<=36) || mode==39 || mode==50;
	if(existing) {
		QVERIFY(f.base.root.reserve(f.reservation,f.commit)); n.files[2].data=f.base.root.json(); n.files[2].rev+="-application";
		QVERIFY(application.prepare()); QVERIFY(application.pin(newPin)); QVERIFY(application.markReserved()); QVERIFY(application.markVerified()); QVERIFY(saved.prepare(B,A,f.base.root));
		if(mode!=35 && mode!=36 && mode!=39 && mode!=50) { QVERIFY(saved.beginApply()); if(mode>=27 && mode!=30) QVERIFY(saved.applied(A)); if(mode>=32 && mode<=34) QVERIFY(saved.persisted(A)); }
		if(mode==35) QVERIFY(QFile::remove(QDir(directory).filePath("before.xml")));
		if(mode==34) { auto changed=read(QDir(directory).filePath("after.xml")); changed[changed.size()/2]=char(changed.at(changed.size()/2)^1); QVERIFY(write(QDir(directory).filePath("after.xml"),changed)); }
		if(mode==36 || mode==50) { auto body=QJsonDocument::fromJson(read(receipt)).object(); body["state"]="prepared"; QVERIFY(write(receipt,QJsonDocument(body).toJson(QJsonDocument::Compact))); QVERIFY(QFile::remove(QDir(directory).filePath("after.xml"))); if(mode==50) QVERIFY(QFile::remove(QDir(directory).filePath("before.xml"))); }
		if(mode==33) { QVERIFY(f.base.root.release(f.sourceReservation)); QVERIFY(f.base.root.release(f.reservation)); n.files[2].data=f.base.root.json(); n.files[2].rev+="-released"; QVERIFY(source.beginRelease()); QVERIFY(source.markReleased()); QVERIFY(application.beginRelease()); QVERIFY(application.markReleased()); }
	}
	CloudWorkspaceAuthority checkpoint(cpFile,f.base.reference); QVERIFY(checkpoint.observe(f.base.root,n.files[2].rev));
	QByteArray local=B; const QByteArray edited="<divelog><dives><dive number='999'><notes>Later local edit</notes></dive></dives></divelog>";
	if(mode==26 || mode==28 || mode==31 || (mode>=32 && mode<=34)) local=A;
	if(mode==5 || mode==29 || mode==32) local=edited;
	QVERIFY(write(diskFile,local));
	const auto addOther=[&] { auto root=CloudWorkspaceRecord::parse(n.files[2].data); const auto id=uid(), parent=root.head(); QVERIFY(root.registerUpload(id,parent,CloudSyncManifest::sha256(B),B.size())); n.add(f.base.reference.payloadName(id),B); QVERIFY(root.completeUpload(id,google ? n.files.last().id.mid(3) : n.files.last().id,n.files.last().rev)); QVERIFY(root.publish(id,parent)); n.files[2].data=root.json(); n.files[2].rev+="-later"; };
	if(mode==18 || mode==30 || mode==31) addOther();
	if(mode==8) n.guard=false; if(mode==10) QVERIFY(QFile::remove(cpFile)); if(mode==42) QVERIFY(QFile::remove(choiceFile));
	if(mode==11) { QVERIFY(QDir().mkpath(directory)); QVERIFY(write(receipt,"corrupt")); }
	if(mode==12) QVERIFY(QFile::remove(uploadFile)); if(mode==13) QVERIFY(QFile::remove(sourceFile));
	if(mode==15) { QVERIFY(source.beginRelease()); QVERIFY(source.markReleased()); }
	if(mode==16) { auto body=QJsonDocument::fromJson(read(sourceFile)).object(); auto p=body["snapshot"].toObject(); p["version"]="changed-source-pin"; body["snapshot"]=p; QVERIFY(write(sourceFile,QJsonDocument(body).toJson(QJsonDocument::Compact))); }
	if(mode==20) n.files[5].data=B; if(mode==21) n.deferReads=true; if(mode==41) n.readFailureStatus=401;
	int requests=0,updates=0,creates=0,payloadReads=0,applyCalls=0; bool ackIssued=false,injected=false,injectionFailed=false;
	const auto changeChoice=[&] { CloudWorkspaceSelection changed(choiceFile,provider,Account); QVERIFY(changed.load()); const auto other=CloudWorkspaceReference::create(provider,Account,f.base.reference.value["epoch"].toString(),google ? "other" : "id:other"); QVERIFY(changed.select(changed.version(),other)); QVERIFY(changed.select(changed.version(),f.base.reference)); };
	n.requested=[&](const QNetworkRequest &r) {
		++requests; const auto path=r.url().path(); const auto arg=QJsonDocument::fromJson(r.rawHeader("Dropbox-API-Arg")).object();
		if(path=="/upload/drive/v3/files" || (!google && path=="/2/files/upload" && arg["mode"].toString()=="add")) ++creates;
		if(path.startsWith("/upload/drive/v2/") || (!google && path=="/2/files/upload" && arg["mode"].isObject())) ++updates;
		if((google && path.endsWith("/6") && QUrlQuery(r.url()).queryItemValue("alt")=="media") || (!google && path=="/2/files/download" && arg["path"].toString()=="id:6")) ++payloadReads;
		if(!injected && (mode==6 || mode==43)) { injected=true; if(mode==6) local=edited; else changeChoice(); }
		if(mode==47 && ackIssued && !injected) { injected=true; local=edited; }
	};
	n.wrote=[&](const QString &) { if(updates==1) { if(mode==7) n.guard=false; if(mode==19) addOther(); } };
	CloudWorkspaceApply client(&n); QSignalSpy applied(&client,&CloudWorkspaceApply::applied),done(&client,&CloudWorkspaceApply::finished),errors(&client,&CloudWorkspaceApply::failed);
	QObject::connect(&client,&CloudWorkspaceApply::applied,&client,[&](const QString &) {
		if(mode==1) return;
		if(mode==22) { client.cancel(); return; }
		if(mode==44) { local=edited; client.confirmPersisted(); return; }
		QSaveFile disk(diskFile); if(!disk.open(QIODevice::WriteOnly) || disk.write(local)!=local.size() || !disk.commit() || read(diskFile)!=local) { injectionFailed=true; return; }
		if(mode==45) n.readFailureStatus=503; if(mode==46) addOther(); ackIssued=true; client.confirmPersisted();
	});
	const auto start=[&] {
		client.start("token",f.base.reference,f.commit,f.reservation,choiceFile,mode==9 ? uid() : choiceVersion,cpFile,sourceFile,uploadFile,readFile,directory,existing ? QByteArray() : B,
			[&] { return n.guard; },[&] { if(mode==49 && !injected) { injected=true; client.cancel(); } return local; },[&](const QByteArray &xml) {
				++applyCalls; if(mode==48) { client.cancel(); return false; } if(mode==2) return false; if(mode==3) return true;
				local=xml; if(mode==4) local=edited; if(mode==23) n.guard=false; if(mode==24) changeChoice(); return true;
			});
	};
	std::unique_ptr<QLockFile> liveLock;
	if(mode==37) { liveLock=std::make_unique<QLockFile>(directory+".operation.lock"); QVERIFY(liveLock->tryLock(0)); }
	if(mode==38 || mode==39 || mode==40) { const auto path=mode==39 ? receipt+".lock" : directory+".operation.lock"; QVERIFY(write(path,{})); QTimer::singleShot(40,&client,[&,path] { injectionFailed=!QFile::remove(path); }); if(mode==40) QTimer::singleShot(20,&client,[&] { client.cancel(); }); }
	start();
	if(mode==21) { QTRY_VERIFY(n.deferred); client.cancel(); if(n.deferred) n.deferred->complete(); n.deferReads=false; QTest::qWait(20); QCOMPARE(done.size()+errors.size()+applied.size(),0); }
	else if(mode==1 || mode==22) { QTRY_COMPARE(applied.size(),1); QCOMPARE(done.size()+errors.size(),0); QVERIFY(saved.inspect()); QCOMPARE(saved.state(),QString("applied")); QCOMPARE(read(diskFile),B); client.cancel(); }
	else if(mode==40 || mode==48 || mode==49) { if(mode==48) QTRY_COMPARE(applyCalls,1); QTest::qWait(200); QCOMPARE(done.size()+errors.size()+applied.size(),0); }
	else {
		QTRY_COMPARE(done.size()+errors.size(),1);
		const bool success=mode==0 || (mode>=25 && mode<=28) || mode==31 || mode==32 || mode==33 || mode==36 || mode==38 || mode==39 || mode==46 || mode==47 || mode==50;
		QCOMPARE(done.size(),int(success));
	}
	if(mode==23 || mode==45) { n.guard=true; n.readFailureStatus=0; const auto oldApplies=applyCalls, oldUpdates=updates; start(); QTRY_COMPARE(done.size(),1); QCOMPARE(applyCalls,oldApplies); QCOMPARE(updates,oldUpdates); }
	if(!done.isEmpty()) {
		QVERIFY(saved.inspect()); QCOMPARE(saved.state(),QString("persisted")); QCOMPARE(saved.before(),B); QCOMPARE(saved.after(),A); const auto root=CloudWorkspaceRecord::parse(n.files[2].data); QVERIFY(root.valid()); QVERIFY(root.follows(f.base.root)); QCOMPARE(root.ticket(f.base.pending),f.base.root.ticket(f.base.pending)); QCOMPARE(root.copySource(f.base.commit).json(),f.base.root.copySource(f.base.commit).json());
		QCOMPARE(done.last()[3].toBool(),mode!=31 && mode!=46); QCOMPARE(done.last()[4].toBool(),mode!=32 && mode!=47); QCOMPARE(local,(mode==32 || mode==47) ? edited : A); QCOMPARE(creates,0);
		if(mode==26 || mode==28 || mode==31 || mode==32 || mode==33) QCOMPARE(applyCalls,0); else QCOMPARE(applyCalls,1);
		if(mode==0 || mode==33) { const auto oldApplies=applyCalls, oldUpdates=updates, oldReads=payloadReads; const auto meta=read(receipt); start(); QTRY_COMPARE(done.size(),2); QCOMPARE(applyCalls,oldApplies); QCOMPARE(updates,oldUpdates); QCOMPARE(payloadReads,oldReads); QCOMPARE(read(receipt),meta); }
	}
	if(mode==5 || mode==6 || mode==4 || mode==29 || mode==44) QCOMPARE(local,edited);
	if(mode==8 || mode==9 || mode==10 || mode==11 || mode==29 || mode==32 || mode==34 || mode==35 || mode==37 || mode==40 || mode==42 || mode==49) QCOMPARE(updates,0);
	if(mode==41) QVERIFY(errors.first()[1].toBool());
	if(mode==48) { QVERIFY(saved.inspect()); QCOMPARE(saved.state(),QString("applying")); QCOMPARE(local,B); }
	if(mode==24) { QVERIFY(saved.inspect()); QCOMPARE(saved.state(),QString("applying")); QCOMPARE(local,A); }
	if(mode!=24 && mode!=42 && mode!=43) QCOMPARE(read(choiceFile),choiceBytes);
	QCOMPARE(n.files[0].data,originals[0].data); QCOMPARE(n.files[1].data,originals[1].data); QCOMPARE(n.files[3].data,originals[3].data); QCOMPARE(n.files[4].data,originals[4].data); QVERIFY(n.allowed); QVERIFY(!injectionFailed); QCOMPARE(creates,0); QCOMPARE(n.renames,0);
}

QTEST_GUILESS_MAIN(TestCloudSyncHistory)
