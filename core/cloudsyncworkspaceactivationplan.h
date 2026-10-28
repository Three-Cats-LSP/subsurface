// SPDX-License-Identifier: GPL-2.0
#ifndef CLOUD_SYNC_WORKSPACE_ACTIVATION_PLAN_H
#define CLOUD_SYNC_WORKSPACE_ACTIVATION_PLAN_H
#include "cloudsyncworkspaceroute.h"
#include "cloudsyncworkspaceread.h"

// Immutable activation identity and private local snapshot. Completion requires
// the exact saved route/binding and a fully released reserved head read.
class CloudWorkspaceActivationPlan {
public:
	CloudWorkspaceActivationPlan(const QString &directory, const CloudWorkspaceReference &workspace, const QString &choiceVersion,
		const QString &revision, const QString &target, const QString &reservation, const QString &operation, const QString &acceptance);
	bool reload();
	bool prepare(const QByteArray &model);
	bool complete(CloudWorkspaceRoute &route, CloudWorkspaceBinding &binding, const CloudWorkspaceRecord &root,
		const CloudWorkspaceRecord &checkpoint, CloudWorkspaceReadIntent &reader);
	QString state() const;
	QByteArray model() const;
	QJsonObject proof() const { return saved; }
private:
	bool read();
	bool write(const QJsonObject &body);
	QString directory;
	CloudWorkspaceReference reference;
	QJsonObject scope, saved;
};
#endif
