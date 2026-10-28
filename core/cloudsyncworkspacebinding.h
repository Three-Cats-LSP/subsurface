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
	bool ownsActivation(const CloudWorkspaceRecord &root, const CloudWorkspaceRecord &checkpoint, const QByteArray &raw,
		const QByteArray &model, const QString &revision, const QString &target);
	bool advanceOwnedUpload(const QString &expectedRevision, const CloudWorkspaceRecord &root, const CloudWorkspaceRecord &checkpoint,
		const QByteArray &raw, const QByteArray &model, const QString &uploadIntentFile, const QString &revision);
	bool advanceReceived(const QString &expectedRevision, const CloudWorkspaceRecord &root, const CloudWorkspaceRecord &checkpoint,
		CloudWorkspaceReceiveReceipt &receipt, CloudWorkspaceReadIntent &reader, const QByteArray &currentModel, const QString &revision);
	// Recognize a persisted receive after the binding was saved, including
	// interrupted reservation release. This does not authorize a new sync.
	bool ownsReceived(const CloudWorkspaceRecord &root, const CloudWorkspaceRecord &checkpoint,
		CloudWorkspaceReceiveReceipt &receipt, CloudWorkspaceReadIntent &reader);
	// Recognize an already saved owned upload even if a later head exists.
	// This is recovery evidence only, not authority for another sync.
	bool ownsUpload(const CloudWorkspaceRecord &root, const CloudWorkspaceRecord &checkpoint,
		const QString &expectedPreviousRevision, const QString &ticket, const QString &uploadIntentFile, const QByteArray &uploadedModel);
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
