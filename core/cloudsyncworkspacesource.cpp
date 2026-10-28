// SPDX-License-Identifier: GPL-2.0
#include "cloudsyncworkspacesource.h"
#include "cloudsynchistory.h"
#include "cloudsyncmanifest.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QLockFile>
#include <QRegularExpression>
#include <QSaveFile>

namespace {
constexpr int ManifestLimit = 16384, ProofLimit = 32768;
constexpr qint64 PayloadLimit = 64 * 1024 * 1024;
const QString Legacy = QStringLiteral("subsurface-neo-sync.json");
QString text(const QJsonObject &o, const char *key) { return o.value(QLatin1String(key)).toString(); }
bool fields(const QJsonObject &o, const QStringList &keys) {
	if (o.size() != keys.size()) return false;
	for (const auto &key : keys) if (!o.contains(key)) return false;
	return true;
}
bool revision(const QString &v) { return !v.isEmpty() && v.size() <= 512 && !v.contains(QChar::Null) && !v.contains('\r') && !v.contains('\n'); }
bool identity(const QString &provider, const QString &id) {
	return id.size() <= 512 && QRegularExpression(provider == "google-drive" ? "^[A-Za-z0-9_-]+$" : "^id:[A-Za-z0-9_-]+$").match(id).hasMatch();
}
bool legacyManifest(const QByteArray &bytes) {
	if (bytes.isEmpty() || bytes.size() > ManifestLimit) return false;
	const auto object = QJsonDocument::fromJson(bytes).object(); const auto manifest = CloudSyncManifest::fromJson(bytes);
	QStringList keys = {"schema", "revision", "sha256", "created_at"};
	if (object.contains("parent_sha256")) keys.append("parent_sha256");
	if (manifest.schemaVersion == 2) keys.append("payload_file");
	const QRegularExpression hash("^[a-f0-9]{64}$");
	return fields(object, keys) && object["schema"] == QJsonValue(manifest.schemaVersion) && manifest.isValid() &&
		CloudHistoryRecord::validEpoch(manifest.revisionId) && hash.match(manifest.payloadSha256).hasMatch() && text(object, "created_at").size() <= 64 &&
		(!object.contains("parent_sha256") || (object["parent_sha256"].isString() && (manifest.parentSha256.isEmpty() || hash.match(manifest.parentSha256).hasMatch())));
}
}
QByteArray CloudWorkspaceCopySource::json() const { return QJsonDocument(value).toJson(QJsonDocument::Compact); }
QString CloudWorkspaceCopySource::key() const { return valid() ? CloudSyncManifest::sha256(json()) : QString(); }
CloudWorkspaceCopySource CloudWorkspaceCopySource::parse(const QByteArray &bytes)
{
	CloudWorkspaceCopySource result;
	if (bytes.size() <= ProofLimit) result.value = QJsonDocument::fromJson(bytes).object();
	if (!result.valid()) result.value = {};
	return result;
}
bool CloudWorkspaceCopySource::valid() const
{
	const auto provider = text(value, "provider");
	if (!fields(value, {"schema", "format", "provider", "account", "manifest", "payload"}) || value["schema"] != QJsonValue(1) ||
	    text(value, "format") != "neo-workspace-copy-source-development-1" || (provider != "google-drive" && provider != "dropbox") ||
	    !QRegularExpression("^[a-f0-9]{64}$").match(text(value, "account")).hasMatch() || json().size() > ProofLimit) return false;
	const auto m = value["manifest"].toObject(), p = value["payload"].toObject();
	const auto encoded = text(m, "body_base64").toLatin1(), body = QByteArray::fromBase64(encoded);
	const auto manifest = CloudSyncManifest::fromJson(body);
	const auto size = p["bytes"].toDouble();
	return fields(m, {"name", "id", "version", "sha256", "body_base64"}) && fields(p, {"name", "id", "version", "sha256", "bytes"}) &&
		text(m, "name") == Legacy && identity(provider, text(m, "id")) && identity(provider, text(p, "id")) && text(m, "id") != text(p, "id") &&
		revision(text(m, "version")) && revision(text(p, "version")) && body.size() <= ManifestLimit && body.toBase64() == encoded &&
		CloudSyncManifest::sha256(body) == text(m, "sha256") && legacyManifest(body) && manifest.payloadFileName() == text(p, "name") &&
		manifest.payloadSha256 == text(p, "sha256") && p["bytes"].isDouble() && size >= 1 && size <= PayloadLimit && size == double(qint64(size));
}
bool CloudWorkspaceCopySource::matches(const QString &provider, const QString &account) const
{
	return valid() && text(value, "provider") == provider && text(value, "account") == account;
}
bool CloudWorkspaceCopySource::accepts(const QByteArray &payload) const
{
	const auto p = value["payload"].toObject();
	return valid() && payload.size() == qint64(p["bytes"].toDouble()) && CloudSyncManifest::sha256(payload) == text(p, "sha256") && CloudHistoryRecord::validXml(payload);
}
CloudWorkspaceCopySource CloudWorkspaceCopySource::load(const QString &fileName)
{
	const QFileInfo info(fileName);
	if (!info.isFile() || info.isSymLink() || info.size() > ProofLimit) return {};
	QFile file(fileName); if (!file.open(QIODevice::ReadOnly)) return {};
	const auto bytes = file.read(ProofLimit + 1); return file.error() == QFileDevice::NoError ? parse(bytes) : CloudWorkspaceCopySource();
}
bool CloudWorkspaceCopySource::preserve(const QString &fileName) const
{
	if (!valid() || fileName.isEmpty() || !QDir().mkpath(QFileInfo(fileName).absolutePath())) return false;
	QLockFile lock(fileName + ".lock"); if (!lock.tryLock(0)) return false;
	const QFileInfo info(fileName);
	if (info.isSymLink()) return false;
	if (info.exists()) { const auto saved = load(fileName); return saved.valid() && saved.json() == json(); }
	QSaveFile file(fileName); const auto bytes = json();
	return file.open(QIODevice::WriteOnly) && file.write(bytes) == bytes.size() && file.commit() && load(fileName).json() == bytes;
}
CloudWorkspaceSourceReview::CloudWorkspaceSourceReview(QNetworkAccessManager *network, QObject *parent) : QObject(parent), files(network, this)
{
	files.setStrictReads(true); files.setResponseLimit(PayloadLimit); deadline.setSingleShot(true); deadline.setInterval(180000);
	connect(&deadline, &QTimer::timeout, this, [this] { fail(); });
	connect(&files, &CloudSyncFileStore::versionedDownloadFinished, this,
		[this](CloudSyncProviderType, const QString &name, const QByteArray &bytes, const QString &version, const QString &id) { received(name, bytes, version, id); });
	connect(&files, &CloudSyncFileStore::operationError, this, [this](CloudSyncProviderType, const QString &, const QString &, bool rejected) { fail(rejected); });
	connect(&files, &CloudSyncFileStore::writeConflict, this, [this](CloudSyncProviderType, const QString &) { fail(); });
	connect(&files, &CloudSyncFileStore::fileNotFound, this, [this](CloudSyncProviderType actual, const QString &name) {
		if (!current()) return;
		if (!allowMissing || actual != provider || name != Legacy) { fail(); return; }
		if (step == Step::Manifest) {
			step = Step::MissingConfirm; files.downloadVersioned(provider, token, Legacy);
		} else if (step == Step::MissingConfirm) {
			cancel(); emit absent();
		} else fail();
	});
}
CloudWorkspaceSourceReview::~CloudWorkspaceSourceReview() { cancel(); }
void CloudWorkspaceSourceReview::cancel()
{
	step = Step::Idle; deadline.stop(); files.cancel(provider); scopeCurrent = {};
	token.clear(); providerId.clear(); account.clear(); manifestId.clear(); manifestVersion.clear(); payloadId.clear(); payloadVersion.clear(); payloadName.clear(); manifest.clear(); payload.clear();
	allowMissing = false;
}
void CloudWorkspaceSourceReview::fail(bool rejected)
{
	if (step == Step::Idle) return;
	cancel(); emit failed(tr("The existing cloud source could not be verified. Its files and your local log were kept; review it again before copying."), rejected);
}
bool CloudWorkspaceSourceReview::current()
{
	if (step == Step::Idle) return false;
	if (!scopeCurrent || !scopeCurrent()) { fail(); return false; }
	return true;
}
void CloudWorkspaceSourceReview::start(const QString &accessToken, const QString &id, const QString &hash, const std::function<bool()> &guard, bool missingAllowed)
{
	cancel(); step = Step::Manifest; token = accessToken; providerId = id; account = hash; scopeCurrent = guard; allowMissing = missingAllowed;
	if (token.isEmpty() || (id != "google-drive" && id != "dropbox") || !QRegularExpression("^[a-f0-9]{64}$").match(hash).hasMatch()) { fail(); return; }
	provider = id == "google-drive" ? CloudSyncProviderType::GoogleDrive : CloudSyncProviderType::Dropbox;
	if (current()) { deadline.start(); files.setResponseLimit(ManifestLimit); files.downloadVersioned(provider, token, Legacy); }
}
void CloudWorkspaceSourceReview::received(const QString &name, const QByteArray &bytes, const QString &version, const QString &id)
{
	if (!current()) return;
	if (!identity(providerId, id) || !revision(version)) { fail(); return; }
	if (step == Step::Manifest) {
		const auto parsed = CloudSyncManifest::fromJson(bytes);
		if (name != Legacy || !legacyManifest(bytes)) { fail(); return; }
		manifest = bytes; manifestId = id; manifestVersion = version; payloadName = parsed.payloadFileName();
		step = Step::Payload; files.setResponseLimit(PayloadLimit); files.downloadVersioned(provider, token, payloadName); return;
	}
	if (step == Step::Payload) {
		if (name != payloadName || id == manifestId || !CloudHistoryRecord::validXml(bytes) || CloudSyncManifest::sha256(bytes) != CloudSyncManifest::fromJson(manifest).payloadSha256) { fail(); return; }
		payload = bytes; payloadId = id; payloadVersion = version; step = Step::ExactManifest;
		files.setResponseLimit(ManifestLimit); files.downloadExact(provider, token, Legacy, manifestId, manifestVersion); return;
	}
	const bool checkManifest = step == Step::ExactManifest || step == Step::AliasManifest;
	if (name != (checkManifest ? Legacy : payloadName) || id != (checkManifest ? manifestId : payloadId) ||
	    version != (checkManifest ? manifestVersion : payloadVersion) || bytes != (checkManifest ? manifest : payload)) { fail(); return; }
	if (step == Step::ExactManifest) { step = Step::ExactPayload; files.setResponseLimit(PayloadLimit); files.downloadExact(provider, token, payloadName, payloadId, payloadVersion); }
	else if (step == Step::ExactPayload) { step = Step::AliasManifest; files.setResponseLimit(ManifestLimit); files.downloadVersioned(provider, token, Legacy); }
	else if (step == Step::AliasManifest) { step = Step::AliasPayload; files.setResponseLimit(PayloadLimit); files.downloadVersioned(provider, token, payloadName); }
	else if (step == Step::AliasPayload) {
		CloudWorkspaceCopySource source; source.value = {{"schema", 1}, {"format", "neo-workspace-copy-source-development-1"}, {"provider", providerId}, {"account", account},
			{"manifest", QJsonObject{{"name", Legacy}, {"id", manifestId}, {"version", manifestVersion}, {"sha256", CloudSyncManifest::sha256(manifest)}, {"body_base64", QString::fromLatin1(manifest.toBase64())}}},
			{"payload", QJsonObject{{"name", payloadName}, {"id", payloadId}, {"version", payloadVersion}, {"sha256", CloudSyncManifest::sha256(payload)}, {"bytes", payload.size()}}}};
		if (!source.accepts(payload) || !current()) { fail(); return; }
		const auto result = source.value; const auto body = payload; cancel(); emit finished(result, body);
	} else fail();
}
