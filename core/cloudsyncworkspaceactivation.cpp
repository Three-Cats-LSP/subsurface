// SPDX-License-Identifier: GPL-2.0
#include "cloudsyncworkspaceactivation.h"
#include <QDir>
#include <QFileInfo>
#include <QLockFile>

namespace {
QString text(const QJsonObject &o, const char *key) { return o.value(QLatin1String(key)).toString(); }
QJsonObject pin(const QJsonObject &ticket)
{
	return {{"name", ticket.value("name")}, {"object", ticket.value("object")}, {"version", ticket.value("version")},
		{"sha256", ticket.value("sha256")}, {"bytes", ticket.value("bytes")}};
}
bool matchesLegacy(const CloudWorkspaceRecord &root, const QString &acceptance, const CloudWorkspaceCopySource &source, bool missing)
{
	const auto event = root.latestLegacyAcceptance();
	if (text(event, "id") != acceptance || (missing && !source.value.isEmpty())) return false;
	if (text(event, "kind") == "absent") return missing;
	const auto copy = root.copySource(text(event, "ticket"));
	return !missing && source.valid() && copy.valid() && source.json() == copy.json();
}
}
CloudWorkspaceActivation::CloudWorkspaceActivation(QNetworkAccessManager *network, QObject *parent) :
	QObject(parent), opener(network, this), sourceReview(network, this), reader(network, this)
{
	deadline.setSingleShot(true); deadline.setInterval(600000);
	connect(&deadline, &QTimer::timeout, this, [this] { fail(); });
	connect(&opener, &CloudWorkspaceOpen::failed, this, [this](const QString &, bool rejected) { fail(rejected); });
	connect(&opener, &CloudWorkspaceOpen::finished, this, &CloudWorkspaceActivation::opened);
	connect(&sourceReview, &CloudWorkspaceSourceReview::failed, this, [this](const QString &, bool rejected) { fail(rejected); });
	connect(&sourceReview, &CloudWorkspaceSourceReview::absent, this, [this] { observedSource({}, true); });
	connect(&sourceReview, &CloudWorkspaceSourceReview::finished, this, [this](const QJsonObject &value, const QByteArray &) { observedSource(value, false); });
	connect(&reader, &CloudWorkspaceRead::failed, this, [this](const QString &, bool rejected) { fail(rejected); });
	connect(&reader, &CloudWorkspaceRead::finished, this, &CloudWorkspaceActivation::readPayload);
	connect(&reader, &CloudWorkspaceRead::released, this, &CloudWorkspaceActivation::released);
}
CloudWorkspaceActivation::~CloudWorkspaceActivation() { cancel(); }
void CloudWorkspaceActivation::cancel()
{
	++generation; step = Step::Idle; deadline.stop(); opener.cancel(); sourceReview.cancel(); reader.cancel(); plan.reset(); operationLock.reset();
	reference.value = {}; root.value = {}; firstSource.value = {}; firstMissing = false; routePrepared = false; routeRequiredSet = false;
	token.clear(); choiceFile.clear(); choiceVersion.clear(); checkpointFile.clear(); bindingFile.clear(); routeFile.clear(); revision.clear();
	target.clear(); reservation.clear(); operation.clear(); acceptance.clear(); planDirectory.clear(); readFile.clear(); rootVersion.clear(); model.clear();
	requireRoute = {}; routeRequired = {}; scopeCurrent = {}; localSnapshot = {};
}
void CloudWorkspaceActivation::fail(bool rejected)
{
	if (step == Step::Idle) return;
	cancel(); emit failed(tr("Workspace activation needs review. Local data, cloud versions and recovery records were kept."), rejected);
}
void CloudWorkspaceActivation::start(const QString &accessToken, const CloudWorkspaceReference &workspace, const QString &choice,
	const QString &choiceVer, const QString &checkpoint, const QString &binding, const QString &route, const QString &newRevision,
	const QString &commit, const QString &readReservation, const QString &operationId, const QString &acceptedEvent,
	const QString &directory, const QString &readIntent, const QByteArray &snapshot, const std::function<bool()> &require, const std::function<bool()> &requiredCheck,
	const std::function<bool()> &guard, const std::function<QByteArray()> &local)
{
	cancel(); token = accessToken; reference = workspace; choiceFile = choice; choiceVersion = choiceVer; checkpointFile = checkpoint;
	bindingFile = binding; routeFile = route; revision = newRevision; target = commit; reservation = readReservation; operation = operationId;
	acceptance = acceptedEvent; planDirectory = directory; readFile = readIntent; model = snapshot; requireRoute = require; routeRequired = requiredCheck; scopeCurrent = guard; localSnapshot = local;
	step = Step::InitialRoot;
	if (token.isEmpty() || !reference.valid() || !CloudHistoryRecord::validEpoch(choiceVersion) || !CloudHistoryRecord::validEpoch(revision) ||
	    !CloudHistoryRecord::validEpoch(target) || !CloudHistoryRecord::validEpoch(reservation) || !CloudHistoryRecord::validEpoch(operation) ||
	    !CloudHistoryRecord::validEpoch(acceptance) || choiceFile.isEmpty() || checkpointFile.isEmpty() || bindingFile.isEmpty() || routeFile.isEmpty() ||
	    planDirectory.isEmpty() || readFile.isEmpty() || model.isEmpty() || model.size() > 64 * 1024 * 1024 || !CloudHistoryRecord::validXml(model) ||
	    !requireRoute || !routeRequired || !scopeCurrent || !localSnapshot || QFileInfo(planDirectory).isSymLink() || !QDir().mkpath(planDirectory)) { fail(); return; }
	operationLock = std::make_unique<QLockFile>(planDirectory + ".operation.lock"); if (!operationLock->tryLock(0)) { fail(); return; }
	plan = std::make_unique<CloudWorkspaceActivationPlan>(planDirectory, reference, choiceVersion, revision, target, reservation, operation, acceptance);
	if (!plan->prepare(model) || !current()) { fail(); return; }
	const auto serial = generation; const auto requireFn = requireRoute; const bool required = requireFn(); if (serial != generation || step == Step::Idle) return;
	if (!required) { fail(); return; }
	routeRequiredSet = true; if (!current()) return;
	CloudWorkspaceRoute routeRecord(routeFile, text(reference.value, "provider"));
	if (!routeRecord.prepare(reference, choiceVersion, revision, operation)) { fail(); return; }
	routePrepared = true; if (!current()) return;
	deadline.start(); readRoot(Step::InitialRoot);
}
bool CloudWorkspaceActivation::current()
{
	if (step == Step::Idle) return false;
	const auto serial = generation; const auto guard = scopeCurrent; const bool allowed = guard && guard();
	if (serial != generation || step == Step::Idle) return false;
	if (!allowed) { fail(); return false; }
	if (routeRequiredSet) {
		const auto check = routeRequired; const bool required = check && check();
		if (serial != generation || step == Step::Idle) return false;
		if (!required) { fail(); return false; }
	}
	CloudWorkspaceSelection choice(choiceFile, text(reference.value, "provider"), text(reference.value, "account"));
	if (!choice.load() || choice.version() != choiceVersion || choice.selected().value != reference.value || !plan || !plan->reload() || plan->model() != model) { fail(); return false; }
	if (routePrepared) {
		CloudWorkspaceRoute routeRecord(routeFile, text(reference.value, "provider"));
		if (!routeRecord.load() || routeRecord.value().value("workspace") != reference.value ||
		    text(routeRecord.value(), "choice_version") != choiceVersion || text(routeRecord.value(), "binding_revision") != revision ||
		    text(routeRecord.value(), "operation") != operation) { fail(); return false; }
	}
	const auto snapshot = localSnapshot; const auto actual = snapshot(); if (serial != generation || step == Step::Idle) return false;
	if (actual != model) { fail(); return false; }
	return true;
}
void CloudWorkspaceActivation::readRoot(Step next)
{
	if (!current()) return;
	CloudWorkspaceAuthority authority(checkpointFile, reference);
	if (!authority.load() || !authority.record().valid()) { fail(); return; }
	step = next; opener.start(token, text(reference.value, "provider"), text(reference.value, "account"), reference, [this] { return current(); }, authority.record());
}
void CloudWorkspaceActivation::opened(const QJsonObject &value, const QString &version)
{
	if (!current() || (step != Step::InitialRoot && step != Step::FinalRoot)) { fail(); return; }
	CloudWorkspaceRecord observed; observed.value = value; CloudWorkspaceAuthority authority(checkpointFile, reference);
	if (!observed.valid() || observed.reference().value != reference.value || !authority.observe(observed, version, true) ||
	    text(observed.latestLegacyAcceptance(), "id") != acceptance || !current()) { fail(); return; }
	root = observed; rootVersion = version;
	if (step == Step::InitialRoot) { readSource(Step::InitialSource); return; }
	CloudWorkspaceReadIntent owned(readFile, reference, target, reservation);
	CloudWorkspaceBinding binding(bindingFile, reference, choiceVersion); CloudWorkspaceRoute routeRecord(routeFile, text(reference.value, "provider"));
	const auto r = root.value.value("journal").toObject().value("reservations").toObject().value(reservation).toObject();
	if (root.head() != target || !matchesLegacy(root, acceptance, firstSource, firstMissing) ||
	    text(r, "commit") != target || r.value("active") != QJsonValue(true) ||
	    !owned.reload() || owned.state() != "verified" || owned.snapshot() != pin(root.ticket(target)) || !authority.load() || authority.record().json() != root.json() ||
	    !binding.activate(root, authority.record(), model, model, revision) || !routeRecord.activate(binding) || !current()) { fail(); return; }
	release();
}
void CloudWorkspaceActivation::readSource(Step next)
{
	if (!current()) return;
	step = next; sourceReview.start(token, text(reference.value, "provider"), text(reference.value, "account"), [this] { return current(); }, true);
}
void CloudWorkspaceActivation::observedSource(const QJsonObject &value, bool missing)
{
	if (!current() || (step != Step::InitialSource && step != Step::VerifySource)) { fail(); return; }
	CloudWorkspaceCopySource observed; observed.value = value;
	if (!matchesLegacy(root, acceptance, observed, missing)) { fail(); return; }
	if (step == Step::VerifySource) {
		if (firstMissing != missing || firstSource.json() != observed.json()) { fail(); return; }
		readRoot(Step::FinalRoot); return;
	}
	firstSource = observed; firstMissing = missing;
	CloudWorkspaceAuthority authority(checkpointFile, reference); CloudWorkspaceBinding binding(bindingFile, reference, choiceVersion);
	CloudWorkspaceReadIntent owned(readFile, reference, target, reservation);
	if (!authority.load() || !binding.load()) { fail(); return; }
	if (!binding.value().isEmpty()) {
		if (!binding.ownsActivation(root, authority.record(), model, model, revision, target) || !owned.reload() ||
		    (owned.state() != "verified" && owned.state() != "releasing" && owned.state() != "released")) { fail(); return; }
		CloudWorkspaceRoute routeRecord(routeFile, text(reference.value, "provider"));
		if (!routeRecord.activate(binding) || !current()) { fail(); return; }
		if (owned.state() == "released") { finish(root, rootVersion); return; }
		release(); return;
	}
	if (root.head() != target) { fail(); return; }
	step = Step::Reading;
	reader.start(CloudWorkspaceRead::Action::Read, token, reference, target, reservation, choiceFile, choiceVersion, checkpointFile, readFile, [this] { return current(); });
}
void CloudWorkspaceActivation::readPayload(const QJsonObject &value, const QString &version, const QString &commit, const QString &id, const QByteArray &payload)
{
	if (step != Step::Reading || commit != target || id != reservation || payload != model || !current()) { fail(); return; }
	CloudWorkspaceRecord observed; observed.value = value;
	if (!observed.valid() || observed.reference().value != reference.value || observed.head() != target ||
	    !matchesLegacy(observed, acceptance, firstSource, firstMissing)) { fail(); return; }
	root = observed; rootVersion = version; readSource(Step::VerifySource);
}
void CloudWorkspaceActivation::release()
{
	if (!current()) return;
	step = Step::Releasing;
	reader.start(CloudWorkspaceRead::Action::Release, token, reference, target, reservation, choiceFile, choiceVersion, checkpointFile, readFile, [this] { return current(); });
}
void CloudWorkspaceActivation::released(const QJsonObject &value, const QString &version, const QString &id)
{
	if (step != Step::Releasing || id != reservation || !current()) { fail(); return; }
	CloudWorkspaceRecord observed; observed.value = value; finish(observed, version);
}
void CloudWorkspaceActivation::finish(const CloudWorkspaceRecord &observed, const QString &version)
{
	if (!current() || !observed.valid()) { fail(); return; }
	CloudWorkspaceAuthority authority(checkpointFile, reference); CloudWorkspaceBinding binding(bindingFile, reference, choiceVersion);
	CloudWorkspaceRoute routeRecord(routeFile, text(reference.value, "provider")); CloudWorkspaceReadIntent owned(readFile, reference, target, reservation);
	if (!authority.load() || !binding.load() || !plan->complete(routeRecord, binding, observed, authority.record(), owned)) { fail(); return; }
	const auto result = observed.value; const auto v = version, commit = target; const bool head = observed.head() == target;
	cancel(); emit finished(result, v, commit, head);
}
