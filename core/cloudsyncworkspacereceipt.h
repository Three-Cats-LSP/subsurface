// SPDX-License-Identifier: GPL-2.0
#ifndef CLOUD_SYNC_WORKSPACE_RECEIPT_H
#define CLOUD_SYNC_WORKSPACE_RECEIPT_H

#include "cloudsyncworkspacebinding.h"
#include "cloudsyncworkspaceread.h"

// Private recovery for a remote receive. The caller must freshly verify the
// account/choice and deterministic raw-to-model projection. This object never
// changes the live model, acknowledges persistence, or advances a binding by
// itself. Unknown files and interrupted receipts remain protected.
class CloudWorkspaceReceiveReceipt {
public:
	CloudWorkspaceReceiveReceipt(const QString &directory, const CloudWorkspaceReference &workspace,
		const QString &choiceVersion, const QString &bindingRevision, const QString &baseline,
		const QString &target, const QString &reservation);
	bool reload();
	bool prepare(const CloudWorkspaceBinding &binding, const CloudWorkspaceRecord &root,
		const CloudWorkspaceRecord &checkpoint, CloudWorkspaceReadIntent &readIntent,
		const QByteArray &beforeModel, const QByteArray &targetRaw, const QByteArray &afterModel);
	bool beginApply();
	bool applied(const QByteArray &observedModel);
	bool persisted(const QByteArray &observedModel);
	QString state() const;
	QJsonObject proof() const { return saved; }
	QByteArray before() const;
	QByteArray raw() const;
	QByteArray after() const;
private:
	bool valid(const QJsonObject &body) const;
	bool read();
	bool write(const QJsonObject &body);
	bool transition(const QString &from, const QString &to, const QByteArray &observed = {});
	QString directory;
	CloudWorkspaceReference reference;
	QJsonObject scope, saved;
};
#endif
