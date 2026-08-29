// SPDX-License-Identifier: GPL-2.0
#include "neowebsyncmodel.h"

NeoWebSyncModel::NeoWebSyncModel(QObject *parent) : QObject(parent) { reset(); }
QString NeoWebSyncModel::state() const { return m_state; }
QString NeoWebSyncModel::status() const { return m_status; }
bool NeoWebSyncModel::conflict() const { return m_state == QStringLiteral("conflict"); }
bool NeoWebSyncModel::actionReady() const { return m_state == QStringLiteral("upload-ready") || m_state == QStringLiteral("download-ready"); }

void NeoWebSyncModel::evaluate(int localRevision, const QString &localChecksum, int remoteRevision, const QString &remoteChecksum)
{
	if (localRevision < 0 || remoteRevision < 0 || localChecksum.trimmed().isEmpty() || remoteChecksum.trimmed().isEmpty()) {
		m_state = QStringLiteral("error");
		m_status = tr("A revision and checksum are required for both copies.");
	} else if (localChecksum.compare(remoteChecksum, Qt::CaseInsensitive) == 0) {
		m_state = QStringLiteral("up-to-date");
		m_status = tr("Local and remote manifests describe the same dive log.");
	} else if (localRevision > remoteRevision) {
		m_state = QStringLiteral("upload-ready");
		m_status = tr("The local revision is newer and can be uploaded after provider authorization.");
	} else if (remoteRevision > localRevision) {
		m_state = QStringLiteral("download-ready");
		m_status = tr("The remote revision is newer and can be checksum-verified before download.");
	} else {
		m_state = QStringLiteral("conflict");
		m_status = tr("Both copies changed at the same revision. Choose which copy to keep; Neo will never overwrite silently.");
	}
	emit changed();
}

void NeoWebSyncModel::keepLocal() { if (!conflict()) return; m_state = QStringLiteral("upload-ready"); m_status = tr("Local copy selected. Provider upload is ready after authorization."); emit changed(); }
void NeoWebSyncModel::keepRemote() { if (!conflict()) return; m_state = QStringLiteral("download-ready"); m_status = tr("Remote copy selected. Checksum verification is required before replacement."); emit changed(); }
void NeoWebSyncModel::reset() { m_state = QStringLiteral("idle"); m_status = tr("No cloud manifest has been compared in this browser session."); emit changed(); }
