// SPDX-License-Identifier: GPL-2.0
#ifndef CLOUD_SYNC_WORKSPACE_UPLOAD_PLAN_H
#define CLOUD_SYNC_WORKSPACE_UPLOAD_PLAN_H
#include "cloudsyncworkspacebinding.h"

// Retained private upload snapshot and immutable operation identity. This
// object performs no network operations and never deletes recovery files.
class CloudWorkspaceUploadPlan {
public:
	CloudWorkspaceUploadPlan(const QString &directory, const CloudWorkspaceReference &workspace,
		const QString &choiceVersion, const QString &bindingRevision, const QString &nextRevision,
		const QString &baseline, const QString &ticket, const QString &operation);
	bool reload();
	bool prepare(const QByteArray &model);
	bool complete(CloudWorkspaceBinding &binding, const CloudWorkspaceRecord &root, const CloudWorkspaceRecord &checkpoint,
		const QString &uploadIntentFile);
	QString state() const;
	QByteArray payload() const;
	QJsonObject proof() const { return saved; }
private:
	bool read();
	bool write(const QJsonObject &body);
	QString directory;
	QJsonObject scope, saved;
};
#endif
