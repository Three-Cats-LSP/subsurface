// SPDX-License-Identifier: GPL-2.0
#include "cloudsyncworkspacesyncreview.h"

namespace {
QString text(const QJsonObject &o, const char *key) { return o.value(QLatin1String(key)).toString(); }
}
CloudWorkspaceSyncReview::CloudWorkspaceSyncReview(QNetworkAccessManager *network, QObject *parent) :
	QObject(parent), opener(network, this), sourceReview(network, this)
{
	deadline.setSingleShot(true); deadline.setInterval(360000);
	connect(&deadline, &QTimer::timeout, this, [this] { fail(); });
	connect(&opener, &CloudWorkspaceOpen::failed, this, [this](const QString &, bool rejected) { fail(rejected); });
	connect(&opener, &CloudWorkspaceOpen::finished, this, &CloudWorkspaceSyncReview::observedRoot);
	connect(&sourceReview, &CloudWorkspaceSourceReview::failed, this, [this](const QString &, bool rejected) { fail(rejected); });
	connect(&sourceReview, &CloudWorkspaceSourceReview::absent, this, [this] { observedSource({}, true); });
	connect(&sourceReview, &CloudWorkspaceSourceReview::finished, this, [this](const QJsonObject &value, const QByteArray &) { observedSource(value, false); });
}
CloudWorkspaceSyncReview::~CloudWorkspaceSyncReview() { cancel(); }
void CloudWorkspaceSyncReview::cancel()
{
	++generation; step = Step::Idle; deadline.stop(); opener.cancel(); sourceReview.cancel();
	reference.value = {}; root.value = {}; firstSource.value = {}; firstMissing = false;
	token.clear(); choiceFile.clear(); choiceVersion.clear(); checkpointFile.clear(); bindingFile.clear(); rootVersion.clear(); bindingRevision.clear(); localModel.clear();
	scopeCurrent = {}; localSnapshot = {};
}
void CloudWorkspaceSyncReview::fail(bool rejected)
{
	if (step == Step::Idle) return;
	cancel(); emit failed(tr("The workspace sync review could not be completed. Local data and cloud versions were kept."), rejected);
}
void CloudWorkspaceSyncReview::start(const QString &accessToken, const CloudWorkspaceReference &workspace,
	const QString &choice, const QString &choiceVer, const QString &checkpoint, const QString &binding,
	const QByteArray &expectedModel, const std::function<bool()> &guard, const std::function<QByteArray()> &snapshot)
{
	cancel(); token = accessToken; reference = workspace; choiceFile = choice; choiceVersion = choiceVer;
	checkpointFile = checkpoint; bindingFile = binding; localModel = expectedModel; scopeCurrent = guard; localSnapshot = snapshot;
	step = Step::FirstRoot;
	if (token.isEmpty() || !reference.valid() || !CloudHistoryRecord::validEpoch(choiceVersion) || choiceFile.isEmpty() ||
	    checkpointFile.isEmpty() || bindingFile.isEmpty() || localModel.isEmpty() || localModel.size() > 64 * 1024 * 1024 ||
	    !CloudHistoryRecord::validXml(localModel) || !scopeCurrent || !localSnapshot || !current()) { fail(); return; }
	CloudWorkspaceBinding saved(bindingFile, reference, choiceVersion); CloudWorkspaceAuthority authority(checkpointFile, reference);
	if (!saved.load() || saved.value().isEmpty() || !authority.load() || !authority.record().valid()) { fail(); return; }
	bindingRevision = saved.revision();
	deadline.start(); readRoot(Step::FirstRoot);
}
bool CloudWorkspaceSyncReview::current()
{
	if (step == Step::Idle) return false;
	const auto serial = generation; const auto guard = scopeCurrent; const bool allowed = guard && guard();
	if (serial != generation || step == Step::Idle) return false;
	if (!allowed) { fail(); return false; }
	CloudWorkspaceSelection choice(choiceFile, text(reference.value, "provider"), text(reference.value, "account"));
	if (!choice.load() || choice.version() != choiceVersion || choice.selected().value != reference.value) { fail(); return false; }
	if (!bindingRevision.isEmpty()) {
		CloudWorkspaceBinding binding(bindingFile, reference, choiceVersion);
		if (!binding.load() || binding.revision() != bindingRevision) { fail(); return false; }
	}
	const auto snapshot = localSnapshot; const auto actual = snapshot(); if (serial != generation || step == Step::Idle) return false;
	if (actual != localModel) { fail(); return false; }
	return true;
}
void CloudWorkspaceSyncReview::readRoot(Step next)
{
	if (!current()) return;
	CloudWorkspaceAuthority authority(checkpointFile, reference);
	if (!authority.load() || !authority.record().valid()) { fail(); return; }
	step = next; opener.start(token, text(reference.value, "provider"), text(reference.value, "account"), reference, [this] { return current(); }, authority.record());
}
void CloudWorkspaceSyncReview::observedRoot(const QJsonObject &value, const QString &version)
{
	if (!current() || (step != Step::FirstRoot && step != Step::FinalRoot)) { fail(); return; }
	CloudWorkspaceRecord observed; observed.value = value; CloudWorkspaceAuthority authority(checkpointFile, reference);
	if (!observed.valid() || observed.reference().value != reference.value || !authority.observe(observed, version, true) || !current()) { fail(); return; }
	root = observed; rootVersion = version;
	readSource(step == Step::FirstRoot ? Step::FirstSource : Step::FinalSource);
}
void CloudWorkspaceSyncReview::readSource(Step next)
{
	if (!current()) return;
	step = next; sourceReview.start(token, text(reference.value, "provider"), text(reference.value, "account"), [this] { return current(); }, true);
}
void CloudWorkspaceSyncReview::observedSource(const QJsonObject &value, bool missing)
{
	if (!current() || (step != Step::FirstSource && step != Step::FinalSource)) { fail(); return; }
	CloudWorkspaceCopySource observed; observed.value = value;
	if ((missing && !value.isEmpty()) || (!missing && !observed.matches(text(reference.value, "provider"), text(reference.value, "account")))) { fail(); return; }
	if (step == Step::FirstSource) { firstSource = observed; firstMissing = missing; readRoot(Step::FinalRoot); return; }
	CloudWorkspaceAuthority authority(checkpointFile, reference); CloudWorkspaceBinding binding(bindingFile, reference, choiceVersion);
	if (!authority.load() || !binding.load() || !current()) { fail(); return; }
	const auto action = firstMissing != missing || firstSource.json() != observed.json() ? CloudWorkspaceSyncDecision::Action::LegacyChanged :
		CloudWorkspaceSyncDecision::evaluate(binding, root, authority.record(), localModel, observed, missing);
	if (action == CloudWorkspaceSyncDecision::Action::Invalid) { fail(); return; }
	const auto result = root.value; const auto version = rootVersion, revision = bindingRevision; cancel(); emit finished(int(action), result, version, revision);
}
