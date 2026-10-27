// SPDX-License-Identifier: GPL-2.0
// Developer-only native provider probe. Token input uses stdin, never argv/files.
// Operates only on explicitly named synthetic fixtures, with a hash/revision
// precondition before writes. Archive mode admits only marked synthetic fences.
// No credential persistence, object creation or deletion API.
#include "core/cloudsyncfilestore.h"
#include "core/cloudsyncmanifest.h"
#include "core/cloudsynchistory.h"
#include <QCoreApplication>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QRegularExpression>
#include <QTimer>

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
	const QRegularExpression fixture(QStringLiteral("^subsurface-neo-history-[a-f0-9]{8}(?:-[a-f0-9]{4}){3}-[a-f0-9]{12}-exact-id-[a-z0-9-]+\\.json$"));
	const QRegularExpression rootFixture(QStringLiteral("^subsurface-neo-history-[a-f0-9]{8}(?:-[a-f0-9]{4}){3}-[a-f0-9]{12}\\.json$"));
	if (!valid || (!google && get("provider") != "dropbox") || !(archival ? rootFixture : fixture).match(name).hasMatch() ||
	    (mode != "read" && mode != "write" && !archival) || token.isEmpty() || token.size() > 8192 ||
	    !QRegularExpression(QStringLiteral("^[a-f0-9]{64}$")).match(get("before_sha256")).hasMatch() ||
	    (archival && (!google || version.isEmpty() || body.size() > 8192)) ||
	    (mode == "write" && (version.isEmpty() || !body.startsWith("neo-identity-probe:") || body.size() > 4096)))
		return output({{"ok", false}, {"phase", "input"}}, 2);
	QNetworkAccessManager network;
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
