// SPDX-License-Identifier: GPL-2.0
#ifndef CLOUD_SYNC_WORKSPACE_H
#define CLOUD_SYNC_WORKSPACE_H
#include <QJsonObject>
#include <QString>

// An explicitly selected workspace is identified by its physical root, not a
// filename/list ordering. A reference is not authorization or a verified root.
class CloudWorkspaceReference {
public:
	QJsonObject value;
	static CloudWorkspaceReference create(const QString &provider, const QString &account,
		const QString &epoch, const QString &rootId);
	static CloudWorkspaceReference parse(const QByteArray &bytes);
	bool valid() const;
	QByteArray json() const;
	QString key() const;
	QString rootName() const;
	QString payloadName(const QString &ticket) const;
	bool matches(const QString &provider, const QString &account) const;
};

// Local user choice only. Callers must verify the provider/account/root before
// selecting and before every operation. This never changes XML, credentials,
// existing cloud history checkpoints or a provider's schema 2 sync baseline.
class CloudWorkspaceSelection {
public:
	CloudWorkspaceSelection(const QString &fileName, const QString &provider, const QString &account);
	bool load();
	bool select(const QString &expectedKey, const CloudWorkspaceReference &reference);
	CloudWorkspaceReference selected() const;
	QJsonObject knownReferences() const { return references; }
private:
	bool readUnlocked();
	QString fileName, provider, account, selectedKey;
	QJsonObject references;
};
#endif
