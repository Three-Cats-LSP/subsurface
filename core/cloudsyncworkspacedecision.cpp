// SPDX-License-Identifier: GPL-2.0
#include "cloudsyncworkspacedecision.h"
#include <QJsonArray>

namespace {
QString text(const QJsonObject &o, const char *key) { return o.value(QLatin1String(key)).toString(); }
QJsonObject pin(const QJsonObject &ticket)
{
	return {{"name", ticket.value("name")}, {"object", ticket.value("object")}, {"version", ticket.value("version")},
		{"sha256", ticket.value("sha256")}, {"bytes", ticket.value("bytes")}};
}
}
CloudWorkspaceSyncDecision::Action CloudWorkspaceSyncDecision::evaluate(const CloudWorkspaceBinding &binding,
	const CloudWorkspaceRecord &root, const CloudWorkspaceRecord &checkpoint, const QByteArray &localModel,
	const CloudWorkspaceCopySource &observedLegacy, bool observedMissing)
{
	const auto saved = binding.value(); const auto ref = root.reference(); const auto base = binding.commit();
	if (saved.isEmpty() || !root.valid() || !checkpoint.valid() || !ref.valid() || !root.follows(checkpoint) ||
	    checkpoint.reference().value != ref.value || saved.value("workspace") != ref.value ||
	    !CloudHistoryRecord::validEpoch(binding.revision()) || !CloudHistoryRecord::validEpoch(base) ||
	    !CloudHistoryRecord::validEpoch(binding.acceptance()) || localModel.isEmpty() || localModel.size() > 64 * 1024 * 1024 ||
	    !CloudHistoryRecord::validXml(localModel) || (observedMissing && !observedLegacy.value.isEmpty())) return Action::Invalid;
	if (saved.value("raw").toObject() != pin(root.ticket(base)) || saved.value("raw").toObject() != pin(checkpoint.ticket(base)) ||
	    text(root.ticket(base), "state") != "committed") return Action::Invalid;
	const auto accepted = root.latestLegacyAcceptance();
	if (text(accepted, "id") != binding.acceptance() || text(checkpoint.latestLegacyAcceptance(), "id") != binding.acceptance()) return Action::LegacyChanged;
	if (text(accepted, "kind") == "absent") {
		if (!observedMissing) return Action::LegacyChanged;
	} else {
		const auto copy = root.copySource(text(accepted, "ticket"));
		if (observedMissing || !observedLegacy.valid() || !copy.valid() || observedLegacy.json() != copy.json()) return Action::LegacyChanged;
	}
	const auto history = root.value.value("journal").toObject().value("history").toArray();
	int baseIndex = -1, headIndex = -1;
	for (int i = 0; i < history.size(); ++i) {
		if (history[i].toString() == base) baseIndex = i;
		if (history[i].toString() == root.head()) headIndex = i;
	}
	if (baseIndex < 0 || headIndex < baseIndex || text(root.ticket(root.head()), "state") != "committed") return Action::Invalid;
	const bool localChanged = !binding.matchesModel(localModel), remoteChanged = root.head() != base;
	if (localChanged && remoteChanged) return Action::Conflict;
	if (localChanged) return Action::Upload;
	if (remoteChanged) return Action::Receive;
	return Action::InSync;
}
