// SPDX-License-Identifier: GPL-2.0
#ifndef CLOUD_SYNC_WORKSPACE_RECEIVE_PLAN_H
#define CLOUD_SYNC_WORKSPACE_RECEIVE_PLAN_H
#include "cloudsyncworkspacereceipt.h"

// Durable local ownership and before-model recovery for one receive attempt.
// No cloud or live-model changes occur here. Completion requires the exact
// persisted receipt, saved binding, and fully released reservation.
class CloudWorkspaceReceivePlan {
public:
	CloudWorkspaceReceivePlan(const QString &directory, const CloudWorkspaceReference &workspace,
		const QString &choiceVersion, const QString &bindingRevision, const QString &nextRevision,
		const QString &baseline, const QString &target, const QString &reservation, const QString &operation,
		const QJsonObject &reviewedBinding = {});
	bool reload();
	bool prepare(const QByteArray &beforeModel);
	bool complete(CloudWorkspaceBinding &binding, const CloudWorkspaceRecord &root, const CloudWorkspaceRecord &checkpoint,
		CloudWorkspaceReceiveReceipt &receipt, CloudWorkspaceReadIntent &reader);
	QString state() const;
	QByteArray before() const;
	QJsonObject proof() const { return saved; }
private:
	bool read();
	bool write(const QJsonObject &body);
	QString directory;
	QJsonObject scope, saved;
};
#endif
