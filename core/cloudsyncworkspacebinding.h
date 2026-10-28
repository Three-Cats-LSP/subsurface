// SPDX-License-Identifier: GPL-2.0
#ifndef CLOUD_SYNC_WORKSPACE_BINDING_H
#define CLOUD_SYNC_WORKSPACE_BINDING_H

#include "cloudsyncworkspace.h"
class CloudWorkspaceReadIntent;
class CloudWorkspaceReceiveReceipt;

// Durable per-workspace sync baseline. This stores metadata only; caller must
// freshly verify the provider/account, selected choice, root, accepted legacy
// observation and exact payload before activation or advancement.
class CloudWorkspaceBinding {
public:
	CloudWorkspaceBinding(const QString &file, const CloudWorkspaceReference &workspace, const QString &choiceVersion);
	bool inspect();
	bool load();
	bool activate(const CloudWorkspaceRecord &root, const CloudWorkspaceRecord &checkpoint, const QByteArray &raw, const QByteArray &model, const QString &revision);
	bool advanceOwnedUpload(const QString &expectedRevision, const CloudWorkspaceRecord &root, const CloudWorkspaceRecord &checkpoint,
		const QByteArray &raw, const QByteArray &model, const QString &uploadIntentFile, const QString &revision);
	bool advanceReceived(const QString &expectedRevision, const CloudWorkspaceRecord &root, const CloudWorkspaceRecord &checkpoint,
		CloudWorkspaceReceiveReceipt &receipt, CloudWorkspaceReadIntent &reader, const QByteArray &currentModel, const QString &revision);
	QJsonObject value() const { return saved; }
	QString revision() const;
	QString commit() const;
	QString acceptance() const;
	bool matchesModel(const QByteArray &model) const;
private:
	bool read();
	bool write(const QJsonObject &value);
	QJsonObject next(const CloudWorkspaceRecord &root, const QByteArray &raw, const QByteArray &model, const QString &revision) const;
	QJsonObject nextAt(const CloudWorkspaceRecord &root, const QString &commit, const QByteArray &raw, const QByteArray &model, const QString &revision) const;
	QString fileName;
	CloudWorkspaceReference reference;
	QJsonObject scope, saved;
};
#endif
