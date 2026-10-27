// SPDX-License-Identifier: GPL-2.0
#include "cloudsyncworkspace.h"
#include "cloudsyncmanifest.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QLockFile>
#include <QRegularExpression>
#include <QSaveFile>

namespace {
constexpr int ReferenceLimit = 2048, SelectionLimit = 64 * 1024, WorkspaceLimit = 32;
bool providerValid(const QString &s) { return s == "google-drive" || s == "dropbox"; }
bool hash(const QString &s) { return QRegularExpression(QStringLiteral("^[a-f0-9]{64}$")).match(s).hasMatch(); }
bool uuid(const QString &s) { return QRegularExpression(QStringLiteral("^[a-f0-9]{8}(?:-[a-f0-9]{4}){3}-[a-f0-9]{12}$")).match(s).hasMatch(); }
bool fields(const QJsonObject &o, const QStringList &keys) {
	if (o.size() != keys.size()) return false;
	for (const auto &key : keys) if (!o.contains(key)) return false;
	return true;
}
QString string(const QJsonObject &o, const char *key) { return o.value(QLatin1String(key)).toString(); }
}

CloudWorkspaceReference CloudWorkspaceReference::create(const QString &provider, const QString &account, const QString &epoch, const QString &rootId)
{
	CloudWorkspaceReference result;
	result.value = {{"schema", 1}, {"format", "neo-workspace-reference-development-1"}, {"provider", provider},
		{"account", account}, {"epoch", epoch}, {"root_id", rootId}};
	if (!result.valid()) result.value = {};
	return result;
}
CloudWorkspaceReference CloudWorkspaceReference::parse(const QByteArray &bytes)
{
	CloudWorkspaceReference result;
	if (bytes.size() > ReferenceLimit) return result;
	result.value = QJsonDocument::fromJson(bytes).object();
	if (!result.valid()) result.value = {};
	return result;
}
bool CloudWorkspaceReference::valid() const
{
	const auto provider = string(value, "provider"), id = string(value, "root_id");
	return fields(value, {"schema", "format", "provider", "account", "epoch", "root_id"}) && value.value("schema") == QJsonValue(1) &&
		string(value, "format") == "neo-workspace-reference-development-1" && providerValid(provider) && hash(string(value, "account")) &&
		uuid(string(value, "epoch")) && id.size() <= 512 && QRegularExpression(provider == "google-drive" ? QStringLiteral("^[A-Za-z0-9_-]+$") :
		QStringLiteral("^id:[A-Za-z0-9_-]+$")).match(id).hasMatch() && json().size() <= ReferenceLimit;
}
QByteArray CloudWorkspaceReference::json() const { return QJsonDocument(value).toJson(QJsonDocument::Compact); }
QString CloudWorkspaceReference::key() const { return valid() ? CloudSyncManifest::sha256(json()) : QString(); }
QString CloudWorkspaceReference::rootName() const { return valid() ? "subsurface-neo-workspace-" + string(value, "epoch") + ".json" : QString(); }
QString CloudWorkspaceReference::payloadName(const QString &ticket) const
{
	return valid() && uuid(ticket) ? "subsurface-neo-workspace-" + string(value, "epoch") + "-" + key() + "-" + ticket + ".xml" : QString();
}
bool CloudWorkspaceReference::matches(const QString &provider, const QString &account) const
{
	return valid() && string(value, "provider") == provider && string(value, "account") == account;
}
CloudWorkspaceSelection::CloudWorkspaceSelection(const QString &path, const QString &provider, const QString &account) :
	fileName(path), provider(provider), account(account) {}
CloudWorkspaceReference CloudWorkspaceSelection::selected() const
{
	CloudWorkspaceReference result; result.value = references.value(selectedKey).toObject(); return result;
}
bool CloudWorkspaceSelection::readUnlocked()
{
	selectedKey.clear(); references = {};
	if (!providerValid(provider) || !hash(account)) return false;
	const QFileInfo info(fileName);
	if (!info.exists()) return !info.isSymLink();
	if (!info.isFile() || info.isSymLink() || info.size() > SelectionLimit) return false;
	QFile file(fileName); if (!file.open(QIODevice::ReadOnly)) return false;
	const auto bytes = file.read(SelectionLimit + 1);
	if (bytes.size() > SelectionLimit || file.error() != QFileDevice::NoError) return false;
	const auto root = QJsonDocument::fromJson(bytes).object();
	if (!fields(root, {"schema", "format", "provider", "account", "selected", "references"}) || root.value("schema") != QJsonValue(1) ||
	    string(root, "format") != "neo-workspace-selection-development-1" || string(root, "provider") != provider || string(root, "account") != account ||
	    !root.value("references").isObject() || !hash(string(root, "selected"))) return false;
	const auto known = root.value("references").toObject();
	if (known.isEmpty() || known.size() > WorkspaceLimit || !known.contains(string(root, "selected"))) return false;
	for (auto it = known.begin(); it != known.end(); ++it) {
		CloudWorkspaceReference reference; reference.value = it.value().toObject();
		if (!reference.matches(provider, account) || reference.key() != it.key()) return false;
	}
	references = known; selectedKey = string(root, "selected"); return true;
}
bool CloudWorkspaceSelection::load()
{
	selectedKey.clear(); references = {};
	if (fileName.isEmpty()) return false;
	if (!QFileInfo(QFileInfo(fileName).absolutePath()).exists()) return readUnlocked();
	QLockFile lock(fileName + ".lock"); return lock.tryLock(0) && readUnlocked();
}
bool CloudWorkspaceSelection::select(const QString &expectedKey, const CloudWorkspaceReference &reference)
{
	if (fileName.isEmpty() || !reference.matches(provider, account) || (!expectedKey.isEmpty() && !hash(expectedKey)) ||
	    !QDir().mkpath(QFileInfo(fileName).absolutePath())) return false;
	QLockFile lock(fileName + ".lock"); if (!lock.tryLock(0) || !readUnlocked()) return false;
	const auto key = reference.key();
	if (selectedKey == key) return true; // Reconcile an already completed local choice without rewriting it.
	if (selectedKey != expectedKey || (!references.contains(key) && references.size() == WorkspaceLimit)) return false;
	auto next = references; next[key] = reference.value;
	const auto bytes = QJsonDocument(QJsonObject{{"schema", 1}, {"format", "neo-workspace-selection-development-1"}, {"provider", provider},
		{"account", account}, {"selected", key}, {"references", next}}).toJson(QJsonDocument::Compact);
	if (bytes.size() > SelectionLimit) return false;
	QSaveFile file(fileName);
	if (!file.open(QIODevice::WriteOnly) || file.write(bytes) != bytes.size() || !file.commit()) return false;
	return readUnlocked() && selectedKey == key && references == next;
}
