// SPDX-License-Identifier: GPL-2.0
#ifndef CLOUD_SYNC_WORKSPACE_ROUTE_H
#define CLOUD_SYNC_WORKSPACE_ROUTE_H
#include "cloudsyncworkspacebinding.h"

// Profile/provider-wide fail-closed route marker. Preparing is durable before
// binding creation; both preparing and active block legacy fallback. Caller
// must freshly verify account, workspace and choice before preparing or
// activating. This object never writes cloud data or changes assignments.
class CloudWorkspaceRoute {
public:
	CloudWorkspaceRoute(const QString &file, const QString &provider);
	bool load();
	// The caller durably records markerRequired before starting activation.
	// A missing marker then blocks fallback instead of looking like a new setup.
	bool legacyAllowed(bool markerRequired = false);
	bool prepare(const CloudWorkspaceReference &reference, const QString &choiceVersion,
		const QString &bindingRevision, const QString &operation);
	bool activate(CloudWorkspaceBinding &binding);
	QString state() const;
	QJsonObject value() const { return saved; }
private:
	bool read();
	bool write(const QJsonObject &body);
	QString fileName, provider;
	QJsonObject saved;
};
#endif
