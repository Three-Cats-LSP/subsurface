// SPDX-License-Identifier: GPL-2.0
#include "cloudsyncmanager.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>

namespace {
QString text(const QJsonObject &o, const char *key) { return o.value(QLatin1String(key)).toString(); }
}

QList<QJsonObject> CloudSyncManager::workspaceAcceptances(const QString &directory, const CloudWorkspaceReference &reference, bool *limited, bool *invalid) const
{
	if (limited) *limited = false; if (invalid) *invalid = false;
	QList<QJsonObject> result; if (!reference.valid()) return result;
	const auto parent = QDir(directory).filePath("acceptances"), scope = QDir(parent).filePath(reference.key());
	if (QFileInfo(directory).isSymLink() || QFileInfo(parent).isSymLink() || QFileInfo(scope).isSymLink()) { if (invalid) *invalid = true; return result; }
	const QDir base(scope);
	const auto entries = base.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot | QDir::NoSymLinks, QDir::Name);
	if (limited) *limited = entries.size() >= 64;
	for (int i = 0; i < qMin(64, int(entries.size())); ++i) {
		const auto event = entries[i].fileName(), path = QDir(entries[i].absoluteFilePath()).filePath("intent.json");
		const QFileInfo info(path); QFile file(path); QJsonObject body;
		if (CloudHistoryRecord::validEpoch(event) && info.isFile() && !info.isSymLink() && info.size() <= 64 * 1024 && file.open(QIODevice::ReadOnly))
			body = QJsonDocument::fromJson(file.read(64 * 1024 + 1)).object();
		CloudWorkspaceCopySource source; source.value = body.value("source").toObject();
		CloudWorkspaceAcceptanceIntent attempt(path, reference, text(body, "choice_version"), source, text(body, "local_sha256"), event,
			text(body, "previous"), text(body, "head"), text(body, "copy"));
		if (body.isEmpty() || !attempt.inspect() || text(body, "event") != event || body.value("workspace").toObject() != reference.value) {
			if (invalid) *invalid = true; continue;
		}
		result.append(body);
	}
	return result;
}
