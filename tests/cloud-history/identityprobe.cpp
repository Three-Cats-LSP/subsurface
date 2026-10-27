// SPDX-License-Identifier: GPL-2.0
// Developer-only native provider probe. Token input uses stdin, never argv/files.
// Operates only on explicitly named synthetic fixtures, with a hash/revision
// precondition before writes. Archive mode admits only marked synthetic fences.
// Creation mode admits only marked fresh workspace intents. No credential
// Source-review mode reads the legacy namespace and saves metadata proofs only.
// persistence, ordinary-log writes or object deletion API.
#include "core/cloudsyncfilestore.h"
#include "core/cloudsyncmanifest.h"
#include "core/cloudsynchistory.h"
#include "core/cloudsyncworkspace.h"
#include "core/cloudsyncworkspacesource.h"
#include "core/cloudsyncworkspacecopy.h"
#include "core/cloudsyncworkspaceupload.h"
#include "core/cloudsyncworkspaceread.h"
#include <QCoreApplication>
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QRegularExpression>
#include <QTimer>

// Count operation kinds without logging headers, tokens, bodies or URLs.
class ProbeNetwork : public QNetworkAccessManager {
public:
	int creates = 0, updates = 0, reads = 0, lookups = 0, otherWrites = 0;
protected:
	QNetworkReply *createRequest(Operation operation, const QNetworkRequest &request, QIODevice *outgoing) override {
		const auto path = request.url().path();
		if (operation == PostOperation && path == "/upload/drive/v3/files") ++creates;
		else if (operation == PutOperation && path.startsWith("/upload/drive/v2/files/")) ++updates;
		else if (operation == PostOperation && path == "/2/files/upload") {
			const auto mode = QJsonDocument::fromJson(request.rawHeader("Dropbox-API-Arg")).object().value("mode");
			if (mode.toString() == "add") ++creates; else ++updates;
		} else if (operation == GetOperation || (operation == PostOperation && path == "/2/files/download")) { ++reads; if (path == "/drive/v3/files") ++lookups; }
		else ++otherWrites;
		return QNetworkAccessManager::createRequest(operation, request, outgoing);
	}
};

int main(int argc, char **argv)
{
	QCoreApplication app(argc, argv);
	const auto output = [](const QJsonObject &result, int code) {
		QFile file; if (file.open(stdout, QIODevice::WriteOnly)) {
			file.write(QJsonDocument(result).toJson(QJsonDocument::Compact) + '\n'); file.flush();
		}
		return code;
	};
	QFile input; if (!input.open(stdin, QIODevice::ReadOnly)) return output({{"ok", false}, {"phase", "input"}}, 2);
	const auto bytes = input.read(16385); const auto config = QJsonDocument::fromJson(bytes).object();
	const QStringList keys = {"provider", "mode", "id", "name", "token", "version", "before_sha256", "body"};
	bool valid = bytes.size() <= 16384 && config.size() == keys.size();
	for (const auto &key : keys) valid &= config.value(key).isString();
	const auto get = [&](const char *key) { return config.value(QLatin1String(key)).toString(); };
	const bool google = get("provider") == "google-drive";
	const auto provider = google ? CloudSyncProviderType::GoogleDrive : CloudSyncProviderType::Dropbox;
	const auto name = get("name"), id = get("id"), token = get("token"), version = get("version"), mode = get("mode");
	const auto body = get("body").toUtf8();
	const bool archival = mode == "archive";
	const bool workspace = mode == "workspace-open";
	const bool creation = mode == "workspace-create";
	const bool sourceReview = mode == "workspace-source";
	const bool copying = mode == "workspace-copy";
	const bool restoring = mode == "workspace-restore";
	const bool uploading = mode == "workspace-upload" || restoring;
	const bool reservedRead = mode == "workspace-reservation";
	const QRegularExpression fixture(QStringLiteral("^subsurface-neo-history-[a-f0-9]{8}(?:-[a-f0-9]{4}){3}-[a-f0-9]{12}-exact-id-[a-z0-9-]+\\.json$"));
	const QRegularExpression rootFixture(QStringLiteral("^subsurface-neo-history-[a-f0-9]{8}(?:-[a-f0-9]{4}){3}-[a-f0-9]{12}\\.json$"));
	const QRegularExpression workspaceFixture(QStringLiteral("^subsurface-neo-workspace-[a-f0-9]{8}(?:-[a-f0-9]{4}){3}-[a-f0-9]{12}\\.json$"));
	const bool validName = sourceReview ? name == "subsurface-neo-sync.json" : ((workspace || creation || copying || uploading || reservedRead) ? workspaceFixture : (archival ? rootFixture : fixture)).match(name).hasMatch();
	if (!valid || (!google && get("provider") != "dropbox") || !validName ||
	    (mode != "read" && mode != "write" && !archival && !workspace && !creation && !sourceReview && !copying && !uploading && !reservedRead) || token.isEmpty() || token.size() > 8192 ||
	    !QRegularExpression(QStringLiteral("^[a-f0-9]{64}$")).match(get("before_sha256")).hasMatch() ||
	    ((archival || workspace || creation || sourceReview || copying || uploading || reservedRead) && body.size() > 8192) || (archival && (!google || version.isEmpty())) ||
	    (mode == "write" && (version.isEmpty() || !body.startsWith("neo-identity-probe:") || body.size() > 4096)))
		return output({{"ok", false}, {"phase", "input"}}, 2);
	ProbeNetwork network;
	if (reservedRead) {
		const auto data = QJsonDocument::fromJson(body).object(); CloudWorkspaceReference ref; ref.value = data.value("reference").toObject();
		const auto epoch = ref.value.value("epoch").toString(), nonce = data.value("nonce").toString(), commit = data.value("commit").toString(), reservation = data.value("reservation").toString(), action = data.value("action").toString();
		const QFileInfo intent(data.value("intent_file").toString()); const QDir parent(intent.absolutePath()); const auto checkpoint = data.value("checkpoint_file").toString(), choice = data.value("choice_file").toString();
		CloudWorkspaceAuthority authority(checkpoint, ref); const auto marker = CloudSyncManifest::sha256(("neo-workspace-create-probe:origin:" + epoch + ":" + nonce).toUtf8());
		if (data.size() != 10 || !ref.matches(get("provider"), data.value("account").toString()) || ref.rootName() != name || ref.value.value("root_id").toString() != id || !version.isEmpty() ||
		    (action != "read" && action != "release") || !CloudHistoryRecord::validEpoch(nonce) || !CloudHistoryRecord::validEpoch(commit) || !CloudHistoryRecord::validEpoch(reservation) ||
		    marker != get("before_sha256") || !intent.isAbsolute() || intent.fileName() != "read-" + reservation + ".json" || parent.dirName() != "neo-workspace-probe-" + epoch + "-" + nonce ||
		    QDir::cleanPath(checkpoint) != parent.filePath("checkpoints/" + ref.key() + ".json") || QDir::cleanPath(choice) != parent.filePath("choice.json") ||
		    !authority.load() || !authority.record().valid() || authority.record().value.value("journal").toObject().value("legacy_sha256").toString() != marker)
			return output({{"ok", false}, {"phase", "input"}}, 2);
		CloudWorkspaceRead reader(&network);
		QObject::connect(&reader, &CloudWorkspaceRead::finished, &app, [&](const QJsonObject &record, const QString &v, const QString &c, const QString &r, const QByteArray &xml) {
			output({{"ok", true}, {"phase", "workspace-read"}, {"record", record}, {"version", v}, {"commit", c}, {"reservation", r}, {"payload_sha256", CloudSyncManifest::sha256(xml)}, {"payload_bytes", double(xml.size())},
				{"createRequests", network.creates}, {"updateRequests", network.updates}, {"otherWrites", network.otherWrites}, {"readRequests", network.reads}, {"nameLookups", network.lookups}}, 0); app.exit(0);
		});
		QObject::connect(&reader, &CloudWorkspaceRead::released, &app, [&](const QJsonObject &record, const QString &v, const QString &r) {
			output({{"ok", true}, {"phase", "workspace-released"}, {"record", record}, {"version", v}, {"reservation", r}, {"createRequests", network.creates}, {"updateRequests", network.updates},
				{"otherWrites", network.otherWrites}, {"readRequests", network.reads}, {"nameLookups", network.lookups}}, 0); app.exit(0);
		});
		QObject::connect(&reader, &CloudWorkspaceRead::failed, &app, [&](const QString &, bool rejected) {
			output({{"ok", false}, {"phase", "workspace-reservation"}, {"tokenRejected", rejected}, {"createRequests", network.creates}, {"updateRequests", network.updates},
				{"otherWrites", network.otherWrites}, {"readRequests", network.reads}, {"nameLookups", network.lookups}}, 2); app.exit(2);
		});
		QTimer::singleShot(0, &app, [&] { reader.start(action == "read" ? CloudWorkspaceRead::Action::Read : CloudWorkspaceRead::Action::Release, token, ref, commit, reservation, choice,
			data.value("choice_version").toString(), checkpoint, intent.absoluteFilePath(), [] { return true; }); });
		return app.exec();
	}
	if (uploading) {
		const auto payload = QJsonDocument::fromJson(body).object(); CloudWorkspaceReference ref; ref.value = payload.value("reference").toObject();
		const auto epoch = ref.value.value("epoch").toString(), nonce = payload.value("nonce").toString(), ticket = payload.value("ticket").toString();
		const QFileInfo intent(payload.value("intent_file").toString()); const QDir parent(intent.absolutePath());
		const auto checkpoint = payload.value("checkpoint_file").toString(), choice = payload.value("choice_file").toString();
		const auto encoded = payload.value("payload_base64").toString().toLatin1(), bytes = QByteArray::fromBase64(encoded);
		CloudWorkspaceAuthority authority(checkpoint, ref);
		const auto marker = CloudSyncManifest::sha256(("neo-workspace-create-probe:origin:" + epoch + ":" + nonce).toUtf8());
		if (payload.size() != (restoring ? 13 : 10) || !ref.matches(get("provider"), payload.value("account").toString()) || ref.rootName() != name || ref.value.value("root_id").toString() != id || !version.isEmpty() ||
		    bytes.isEmpty() || bytes.size() > 2048 || bytes.toBase64() != encoded || !CloudHistoryRecord::validXml(bytes) || !payload.value("parent").isString() ||
		    !CloudHistoryRecord::validEpoch(nonce) || !CloudHistoryRecord::validEpoch(ticket) || marker != get("before_sha256") ||
		    !intent.isAbsolute() || intent.fileName() != (restoring ? "restore-" : "upload-") + ticket + ".json" || parent.dirName() != "neo-workspace-probe-" + epoch + "-" + nonce ||
		    QDir::cleanPath(checkpoint) != parent.filePath("checkpoints/" + ref.key() + ".json") || QDir::cleanPath(choice) != parent.filePath("choice.json") ||
		    !authority.load() || !authority.record().valid() || authority.record().value.value("journal").toObject().value("legacy_sha256").toString() != marker)
			return output({{"ok", false}, {"phase", "input"}}, 2);
		if (restoring && (!CloudHistoryRecord::validEpoch(payload.value("source").toString()) || !CloudHistoryRecord::validEpoch(payload.value("reservation").toString()) ||
		    QDir::cleanPath(payload.value("read_intent_file").toString()) != parent.filePath("read-" + payload.value("reservation").toString() + ".json")))
			return output({{"ok", false}, {"phase", "input"}}, 2);
		CloudWorkspaceUpload upload(&network);
		QObject::connect(&upload, &CloudWorkspaceUpload::finished, &app, [&](const QJsonObject &record, const QString &actualVersion, const QString &uploadedTicket, bool headMatches) {
			output({{"ok", true}, {"phase", restoring ? "workspace-restored" : "workspace-uploaded"}, {"record", record}, {"version", actualVersion}, {"ticket", uploadedTicket}, {"headMatches", headMatches},
				{"createRequests", network.creates}, {"updateRequests", network.updates}, {"otherWrites", network.otherWrites}, {"readRequests", network.reads}, {"nameLookups", network.lookups}}, 0); app.exit(0);
		});
		QObject::connect(&upload, &CloudWorkspaceUpload::failed, &app, [&](const QString &, bool rejected) {
			output({{"ok", false}, {"phase", "workspace-upload"}, {"tokenRejected", rejected}, {"createRequests", network.creates}, {"updateRequests", network.updates},
				{"otherWrites", network.otherWrites}, {"readRequests", network.reads}, {"nameLookups", network.lookups}}, 2); app.exit(2);
		});
		QTimer::singleShot(0, &app, [&] {
			if (restoring) upload.startRestore(token, ref, bytes, choice, payload.value("choice_version").toString(), checkpoint, intent.absoluteFilePath(), ticket, payload.value("parent").toString(),
				payload.value("read_intent_file").toString(), payload.value("source").toString(), payload.value("reservation").toString(), [] { return true; });
			else upload.start(token, ref, bytes, choice, payload.value("choice_version").toString(), checkpoint, intent.absoluteFilePath(), ticket, payload.value("parent").toString(), [] { return true; });
		});
		return app.exec();
	}
	if (copying) {
		const auto payload = QJsonDocument::fromJson(body).object(); CloudWorkspaceReference ref; ref.value = payload.value("reference").toObject();
		CloudWorkspaceCopySource source; source.value = payload.value("source").toObject();
		const auto epoch = ref.value.value("epoch").toString(), nonce = payload.value("nonce").toString(), ticket = payload.value("ticket").toString();
		const QFileInfo intent(payload.value("intent_file").toString()); const QDir parent(intent.absolutePath());
		const auto checkpoint = payload.value("checkpoint_file").toString(), choice = payload.value("choice_file").toString(), proof = payload.value("proof_file").toString();
		CloudWorkspaceAuthority authority(checkpoint, ref);
		const auto marker = CloudSyncManifest::sha256(("neo-workspace-create-probe:origin:" + epoch + ":" + nonce).toUtf8());
		if (payload.size() != 11 || !ref.valid() || ref.rootName() != name || ref.value.value("root_id").toString() != id || !version.isEmpty() ||
		    !source.matches(get("provider"), payload.value("account").toString()) || !CloudHistoryRecord::validEpoch(nonce) || !CloudHistoryRecord::validEpoch(ticket) ||
		    marker != get("before_sha256") || !intent.isAbsolute() || intent.fileName() != "copy-" + ticket + ".json" || parent.dirName() != "neo-workspace-probe-" + epoch + "-" + nonce ||
		    QDir::cleanPath(checkpoint) != parent.filePath("checkpoints/" + ref.key() + ".json") || QDir::cleanPath(choice) != parent.filePath("choice.json") ||
		    QDir::cleanPath(proof) != parent.filePath("copy-source.json") || !authority.load() || !authority.record().valid() ||
		    authority.record().value.value("journal").toObject().value("legacy_sha256").toString() != marker)
			return output({{"ok", false}, {"phase", "input"}}, 2);
		CloudWorkspaceCopy copy(&network);
		QObject::connect(&copy, &CloudWorkspaceCopy::finished, &app, [&](const QJsonObject &record, const QString &actualVersion, const QString &copiedTicket, bool changed) {
			output({{"ok", true}, {"phase", "workspace-copied"}, {"record", record}, {"version", actualVersion}, {"ticket", copiedTicket}, {"legacyChanged", changed},
				{"createRequests", network.creates}, {"updateRequests", network.updates}, {"otherWrites", network.otherWrites}, {"readRequests", network.reads}, {"nameLookups", network.lookups}}, 0); app.exit(0);
		});
		QObject::connect(&copy, &CloudWorkspaceCopy::failed, &app, [&](const QString &, bool rejected) {
			output({{"ok", false}, {"phase", "workspace-copy"}, {"tokenRejected", rejected}, {"createRequests", network.creates}, {"updateRequests", network.updates},
				{"otherWrites", network.otherWrites}, {"readRequests", network.reads}, {"nameLookups", network.lookups}}, 2); app.exit(2);
		});
		QTimer::singleShot(0, &app, [&] { copy.start(token, ref, source, choice, payload.value("choice_version").toString(), checkpoint, intent.absoluteFilePath(), proof,
			ticket, payload.value("parent").toString(), [] { return true; }); });
		return app.exec();
	}
	if (sourceReview) {
		const auto payload = QJsonDocument::fromJson(body).object(); const QFileInfo proof(payload.value("proof_file").toString());
		const auto parent = QDir(proof.absolutePath()).dirName();
		if (payload.size() != 2 || !proof.isAbsolute() || proof.fileName() != "source.json" || !parent.startsWith("neo-workspace-source-probe-") ||
		    !CloudHistoryRecord::validEpoch(parent.mid(27)) || id.isEmpty() || version.isEmpty()) return output({{"ok", false}, {"phase", "input"}}, 2);
		CloudWorkspaceSourceReview review(&network);
		QObject::connect(&review, &CloudWorkspaceSourceReview::finished, &app, [&](const QJsonObject &record, const QByteArray &bytes) {
			CloudWorkspaceCopySource source; source.value = record; const auto manifest = record.value("manifest").toObject(), object = record.value("payload").toObject();
			const bool confirmed = source.accepts(bytes) && manifest.value("id").toString() == id && manifest.value("version").toString() == version &&
				manifest.value("sha256").toString() == get("before_sha256") && network.creates == 0 && network.updates == 0 && network.otherWrites == 0 && source.preserve(proof.absoluteFilePath());
			output({{"ok", confirmed}, {"phase", "source-reviewed"}, {"proof_key", source.key()}, {"manifest_sha256", manifest.value("sha256")},
				{"payload_sha256", object.value("sha256")}, {"bytes", qint64(bytes.size())}, {"createRequests", network.creates}, {"updateRequests", network.updates},
				{"otherWrites", network.otherWrites}, {"readRequests", network.reads}, {"nameLookups", network.lookups}}, confirmed ? 0 : 2); app.exit(confirmed ? 0 : 2);
		});
		QObject::connect(&review, &CloudWorkspaceSourceReview::failed, &app, [&](const QString &, bool rejected) {
			output({{"ok", false}, {"phase", "source-review"}, {"tokenRejected", rejected}, {"createRequests", network.creates}, {"updateRequests", network.updates}, {"otherWrites", network.otherWrites}}, 2); app.exit(2);
		});
		QTimer::singleShot(0, &app, [&] { review.start(token, get("provider"), payload.value("account").toString(), [] { return true; }); });
		return app.exec();
	}
	if (creation) {
		const auto payload = QJsonDocument::fromJson(body).object(); const auto epoch = payload.value("epoch").toString(), nonce = payload.value("nonce").toString();
		const auto intentFile = payload.value("intent_file").toString(), directory = payload.value("checkpoint_directory").toString();
		const QFileInfo info(intentFile); const QDir parent(info.absolutePath());
		const auto marker = CloudSyncManifest::sha256(("neo-workspace-create-probe:origin:" + epoch + ":" + nonce).toUtf8());
		if (payload.size() != 5 || !CloudHistoryRecord::validEpoch(epoch) || !CloudHistoryRecord::validEpoch(nonce) || !id.isEmpty() || !version.isEmpty() ||
		    name != "subsurface-neo-workspace-" + epoch + ".json" || marker != get("before_sha256") || !info.isAbsolute() || info.fileName() != "creation.json" ||
		    parent.dirName() != "neo-workspace-probe-" + epoch + "-" + nonce || QDir::cleanPath(directory) != parent.filePath("checkpoints"))
			return output({{"ok", false}, {"phase", "input"}}, 2);
		CloudWorkspaceCreate create(&network);
		QObject::connect(&create, &CloudWorkspaceCreate::finished, &app, [&](const QJsonObject &root, const QString &actualVersion, const QString &checkpoint) {
			output({{"ok", true}, {"phase", "workspace-created"}, {"record", root}, {"version", actualVersion}, {"checkpoint_file", checkpoint},
				{"createRequests", network.creates}, {"updateRequests", network.updates}, {"readRequests", network.reads}, {"nameLookups", network.lookups}}, 0); app.exit(0);
		});
		QObject::connect(&create, &CloudWorkspaceCreate::failed, &app, [&](const QString &, bool rejected) {
			output({{"ok", false}, {"phase", "workspace-create"}, {"tokenRejected", rejected}, {"createRequests", network.creates},
				{"updateRequests", network.updates}, {"readRequests", network.reads}, {"nameLookups", network.lookups}}, 2); app.exit(2);
		});
		QTimer::singleShot(0, &app, [&] { create.start(intentFile, directory, get("provider"), token, payload.value("account").toString(), epoch, nonce, marker, [] { return true; }); });
		return app.exec();
	}
	if (workspace) {
		const auto payload = QJsonDocument::fromJson(body).object();
		CloudWorkspaceReference ref; ref.value = payload.value("reference").toObject();
		CloudWorkspaceRecord checkpoint; checkpoint.value = payload.value("checkpoint").toObject();
		if (payload.size() != 3 || !payload.value("checkpoint").isObject() || !ref.valid() || ref.value.value("root_id").toString() != id ||
		    ref.rootName() != name) return output({{"ok", false}, {"phase", "input"}}, 2);
		CloudWorkspaceOpen open(&network);
		QObject::connect(&open, &CloudWorkspaceOpen::finished, &app, [&](const QJsonObject &root, const QString &actualVersion) {
			CloudWorkspaceRecord record; record.value = root;
			const auto marker = CloudSyncManifest::sha256(("neo-workspace-probe:legacy:" + ref.value.value("epoch").toString()).toUtf8());
			const auto hash = CloudSyncManifest::sha256(record.json());
			const bool confirmed = hash == get("before_sha256") && (version.isEmpty() || version == actualVersion) &&
				root.value("journal").toObject().value("legacy_sha256").toString() == marker;
			output({{"ok", confirmed}, {"phase", "workspace-verified"}, {"sha256", hash}, {"version", actualVersion}, {"workspace_key", ref.key()}}, confirmed ? 0 : 2);
			app.exit(confirmed ? 0 : 2);
		});
		QObject::connect(&open, &CloudWorkspaceOpen::failed, &app, [&](const QString &, bool rejected) {
			output({{"ok", false}, {"phase", "workspace-open"}, {"tokenRejected", rejected}}, 2); app.exit(2);
		});
		QTimer::singleShot(0, &app, [&] { open.start(token, get("provider"), payload.value("account").toString(), ref, [] { return true; }, checkpoint); });
		return app.exec();
	}
	if (archival) {
		const auto payload = QJsonDocument::fromJson(body).object();
		CloudHistoryFence source, coordinator; source.value = payload.value("source").toObject(); coordinator.value = payload.value("coordinator").toObject();
		const auto epoch = source.record().value.value("epoch").toString();
		const auto marker = CloudSyncManifest::sha256(("neo-archive-probe:legacy:" + epoch).toUtf8());
		if (payload.size() != 4 || !source.valid() || !coordinator.valid() || CloudHistoryRecord::rootName(epoch) != name ||
		    source.record().value.value("legacy_sha256").toString() != marker || coordinator.record().value.value("legacy_sha256").toString() != marker ||
		    CloudSyncManifest::sha256(source.json()) != get("before_sha256")) return output({{"ok", false}, {"phase", "input"}}, 2);
		CloudHistoryArchive archive(&network);
		QObject::connect(&archive, &CloudHistoryArchive::finished, &app, [&](const QJsonObject &target, const QString &targetVersion, const QString &sourceVersion) {
			output({{"ok", true}, {"phase", "archive-confirmed"}, {"source_version", sourceVersion}, {"target_version", targetVersion},
				{"sha256", CloudSyncManifest::sha256(source.json())}, {"coordinator_sha256", CloudSyncManifest::sha256(QJsonDocument(target).toJson(QJsonDocument::Compact))}}, 0); app.exit(0);
		});
		QObject::connect(&archive, &CloudHistoryArchive::failed, &app, [&](const QString &, bool rejected) {
			output({{"ok", false}, {"phase", "archive"}, {"tokenRejected", rejected}}, 2); app.exit(2);
		});
		QTimer::singleShot(0, &app, [&] { archive.start(token, payload.value("account").toString(), id, source, version,
			payload.value("coordinator_id").toString(), coordinator, [] { return true; }); });
		return app.exec();
	}
	CloudSyncFileStore files(&network); files.setStrictReads(true); files.setResponseLimit(1024 * 1024);
	QString phase = "before";
	const auto fail = [&](bool conflict = false, bool rejected = false) {
		output({{"ok", false}, {"phase", phase}, {"conflict", conflict}, {"tokenRejected", rejected}}, 2); app.exit(2);
	};
	QObject::connect(&files, &CloudSyncFileStore::operationError, &app,
		[&](CloudSyncProviderType, const QString &, const QString &, bool rejected) { fail(false, rejected); });
	QObject::connect(&files, &CloudSyncFileStore::writeConflict, &app,
		[&](CloudSyncProviderType, const QString &) { fail(true); });
	QObject::connect(&files, &CloudSyncFileStore::fileNotFound, &app,
		[&](CloudSyncProviderType, const QString &) { fail(); });
	QObject::connect(&files, &CloudSyncFileStore::versionedDownloadFinished, &app,
		[&](CloudSyncProviderType, const QString &, const QByteArray &data, const QString &actualVersion, const QString &actualId) {
			const auto hash = CloudSyncManifest::sha256(data);
			if (actualId != id) { fail(); return; }
			if (phase == "before") {
				if (hash != get("before_sha256") || (!version.isEmpty() && actualVersion != version)) { fail(true); return; }
				if (mode == "write") {
					phase = "write"; files.uploadIdentity(provider, token, name, body, version, id); return;
				}
			} else if (phase != "confirm" || data != body) { fail(); return; }
			output({{"ok", true}, {"phase", phase}, {"sha256", hash}, {"bytes", qint64(data.size())}, {"version", actualVersion}}, 0); app.exit(0);
		});
	QObject::connect(&files, &CloudSyncFileStore::uploadFinished, &app,
		[&](CloudSyncProviderType, const QString &) {
			phase = "confirm"; files.downloadIdentity(provider, token, name, id);
		});
	QTimer::singleShot(180000, &app, [&] { phase = "timeout"; files.cancel(provider); fail(); });
	// Queue startup so even immediate validation failures terminate the event loop.
	QTimer::singleShot(0, &app, [&] { files.downloadIdentity(provider, token, name, id); });
	return app.exec();
}
