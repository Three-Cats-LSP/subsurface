// SPDX-License-Identifier: GPL-2.0
#ifndef CLOUD_SYNC_WORKSPACE_DECISION_H
#define CLOUD_SYNC_WORKSPACE_DECISION_H
#include "cloudsyncworkspacebinding.h"

// A point-in-time decision only. Callers must recheck account, selected choice,
// legacy observation, local model and exact root before any dependent mutation.
class CloudWorkspaceSyncDecision {
public:
	enum class Action { Invalid, LegacyChanged, InSync, Upload, Receive, Conflict };
	static Action evaluate(const CloudWorkspaceBinding &binding, const CloudWorkspaceRecord &root,
		const CloudWorkspaceRecord &checkpoint, const QByteArray &localModel,
		const CloudWorkspaceCopySource &observedLegacy, bool observedMissing);
};
#endif
