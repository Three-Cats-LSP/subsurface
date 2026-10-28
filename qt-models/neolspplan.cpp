// SPDX-License-Identifier: GPL-2.0
#include "diveplannermodel.h"
#include "lspplanner.h"
#include "neoplanmetadata.h"
#include "commands/command.h"
#include "core/dive.h"
#include "core/divelog.h"
#include "core/event.h"
#include "core/equipment.h"
#include "core/format.h"
#include "core/pref.h"
#include "core/qthelper.h"
#include "core/sample.h"
#include "core/string-format.h"
#include <QCryptographicHash>
#include <QElapsedTimer>
#include <QJsonDocument>
#include <QRegularExpression>
#include <QTimeZone>
#include <cmath>
#include <set>

namespace {
QString circuitName(int mode)
{
	return mode == CCR ? QStringLiteral("CCR") : mode == PSCR ? QStringLiteral("pSCR") : QStringLiteral("OC");
}

QVariantMap refused(const QString &message, bool validDate = true)
{
	return {{"engine", LspPlanner::identity().toVariantMap()}, {"dateTimeValid", validDate}, {"notes", message},
		{"planSaveAllowed", false}, {"exceedsNDL", false}, {"newDiveId", -1}, {"profile", QVariantList()},
		{"timeline", QVariantList()}, {"schedule", QVariantList()}, {"gasAnalysis", QVariantList()}};
}

double barPerMetre(int kind, double salinity)
{
	return kind == 0 ? 0.1 : kind == 1 ? 1.0 / 10.33 : kind == 2 ? 1.0 / 10.08 : salinity * 9.80665 / 1000000.0;
}

QJsonObject environment(double pressure, double gradient, double vapor, bool acclimatized = true)
{
	return {{"altSurfaceP", pressure}, {"barPerMetre", gradient}, {"waterVapor", vapor},
		{"altAcclimatized", acclimatized}, {"allowO2AtMOD", true}};
}

// Carry the connected sampled history; never silently replace an unavailable
// prior state with fresh tissues. A seven-day surface gap closes the chain.
bool priorProfiles(qint64 start, double vapor, QJsonArray &profiles, QString &error)
{
	std::vector<const dive *> previous;
	for (const auto &entry : divelog.dives) {
		if (!entry->is_planned() && entry->when < start)
			previous.push_back(entry.get());
	}
	std::sort(previous.begin(), previous.end(), [](const dive *a, const dive *b) { return a->when < b->when; });
	size_t first = previous.size();
	qint64 nextStart = start;
	while (first > 0) {
		const dive *entry = previous[first - 1];
		if (nextStart - entry->endtime() > 7 * 24 * 3600) break;
		if (entry->endtime() > nextStart) { error = QStringLiteral("Prior dive profiles overlap this calculation."); return false; }
		nextStart = entry->when; --first;
	}
	if (previous.size() - first > 128) { error = QStringLiteral("The connected prior dive chain exceeds the LSP calculation bound."); return false; }
	int totalSamples = 0;
	for (size_t index = first; index < previous.size(); ++index) {
		const dive *entry = previous[index];
		if (entry->dcs.empty() || entry->dcs[0].samples.size() < 2) {
			error = QStringLiteral("A recent dive has no complete sampled profile. Use the existing Subsurface profile until that history is available."); return false;
		}
		const auto &dc = entry->dcs[0];
		const auto &samples = dc.samples;
		if (samples.front().time.seconds != 0 || samples.front().depth.mm > SURFACE_THRESHOLD ||
		    samples.back().depth.mm > SURFACE_THRESHOLD || samples.back().time.seconds != entry->endtime() - entry->when) {
			error = QStringLiteral("A recent dive lacks a complete sampled beginning or return to the surface."); return false;
		}
		totalSamples += static_cast<int>(samples.size());
		if (totalSamples > 32768) { error = QStringLiteral("The sampled history exceeds the bounded LSP calculation size."); return false; }
		QJsonArray segments;
		gasmix_loop gas(*entry, dc);
		divemode_loop mode(dc);
		for (size_t si = 1; si < samples.size(); ++si) {
			const auto &a = samples[si - 1], &b = samples[si];
			if (b.time.seconds <= a.time.seconds) { error = QStringLiteral("A prior profile has non-increasing sample times."); return false; }
			std::set<int> boundaries{a.time.seconds,b.time.seconds};
			for(const auto &event:dc.events)if(event.time.seconds>a.time.seconds&&event.time.seconds<b.time.seconds&&
				(event.is_gaschange()||event.is_divemodechange()||event.type==SAMPLE_EVENT_PO2))boundaries.insert(event.time.seconds);
			for(auto cut=boundaries.begin(), next=std::next(cut);next!=boundaries.end();++cut,++next){
				auto [activeMode, cylinder, mix] = get_dive_status_at(*entry, dc, *cut, &mode, &gas); Q_UNUSED(cylinder)
				if (!mix || (activeMode != OC && activeMode != CCR)) { error = QStringLiteral("A recent dive has unsupported or unknown breathing-state history."); return false; }
				int setpoint=a.setpoint.mbar>0?a.setpoint.mbar:b.setpoint.mbar;
				for(const auto &event:dc.events)if(event.type==SAMPLE_EVENT_PO2&&event.time.seconds>=a.time.seconds&&event.time.seconds<=*cut)setpoint=event.value;
				auto depthAt=[&](int time){return (a.depth.mm+(b.depth.mm-a.depth.mm)*(time-a.time.seconds)/double(b.time.seconds-a.time.seconds))/1000.0;};
				segments.append(QJsonObject{{"from", depthAt(*cut)}, {"to", depthAt(*next)},
					{"time", (*next-*cut)/60.0}, {"o2", get_o2(*mix)/10.0}, {"he", get_he(*mix)/10.0},
					{"circuit", circuitName(activeMode)}, {"setpoint", setpoint/1000.0}});
			}
		}
		const int salinity = entry->get_salinity();
		const int kind = salinity == 10000 ? 1 : salinity == 10200 ? 2 : salinity == 10300 || salinity == 0 ? 0 : 3;
		profiles.append(QJsonObject{{"start", static_cast<double>(entry->when)}, {"end", static_cast<double>(entry->endtime())},
			{"environment", environment(entry->get_surface_pressure().mbar / 1000.0, barPerMetre(kind, salinity), vapor)}, {"segments", segments}});
	}
	return true;
}
}

QVariantMap DivePlannerPointsModel::calculateNeoLspPlan(const QVariantMap &input, bool shouldSave)
{
	QDateTime when = QDateTime::fromString(input.value("date").toString() + QLatin1Char(' ') + input.value("time").toString(), QStringLiteral("yyyy-MM-dd hh:mm:ss"));
	if (!when.isValid()) return refused(tr("Enter a valid planned start date and time."), false);
#if QT_VERSION >= QT_VERSION_CHECK(6, 9, 0)
	when = QDateTime(when.date(), when.time(), QTimeZone(QTimeZone::UTC));
#else
	when = QDateTime(when.date(), when.time(), Qt::UTC);
#endif
	const int mode = input.value("diveMode").toInt();
	if (mode < OC || mode > PSCR) return refused(tr("Select OC, CCR or pSCR for the LSP planner."));
	const auto cylinderData = input.value("cylinders").toList(), segmentData = input.value("segments").toList();
	if (cylinderData.empty() || cylinderData.size() > 32 || segmentData.empty() || segmentData.size() > 64)
		return refused(tr("Enter a bounded profile with known cylinders and levels."));
	const auto presets = LspPlanner::presets();
	if (presets.isEmpty()) return refused(tr("The verified LSP preset library is unavailable."));
	auto parameters = presets.at(0).toObject().value("values").toObject();
	const auto custom = QJsonObject::fromVariantMap(input.value("lspParameters").toMap());
	for (auto it = custom.begin(); it != custom.end(); ++it) parameters[it.key()] = it.value();
	auto number = [&](const char *key, double fallback = 0.0) {
		const auto value = parameters.value(QLatin1String(key));
		bool ok = false;
		const double result = value.isDouble() ? value.toDouble() : value.toString().toDouble(&ok);
		return value.isDouble() || ok ? result : fallback;
	};
	const double pressure = input.value("surfacePressureBar").toDouble();
	const int water = input.value("waterType").toInt();
	const int salinity = water == 0 ? 10300 : water == 1 ? 10000 : water == 2 ? 10200 : input.value("customSalinity").toInt();
	if (water < 0 || water > 3 || !std::isfinite(pressure) || pressure < 0.3 || pressure > 1.2 || salinity < 8000 || salinity > 13000)
		return refused(tr("Enter a supported surface pressure and water density."));
	QJsonObject settings{{"metric", true}, {"circuit", circuitName(mode)}, {"gfLo", number("gfLowInput")}, {"gfHi", number("gfHighInput")},
		{"descentRate", number("descentRate")}, {"ascentRate", number("ascentRate")}, {"decoAscentRate", number("decoAscentRate")},
		{"surfaceAscentRate", number("surfaceAscentRate")}, {"stepSize", number("decoStep")}, {"lastStop", number("lastDecoStop")},
		{"minStopTime", number("minStopTime")}, {"ppO2Bottom", number("ppo2Bottom")}, {"ppO2Deco", number("ppo2Deco")},
		{"wholeMinStops", parameters.value("stopRounding").toString() == QLatin1String("wholeminute")},
		{"mdCompatMode", parameters.value("decoTransitMode").toString() == QLatin1String("multideco")},
		{"heHalfTimeMode", parameters.value("heHalfTimeMode")}, {"shallowGradient", parameters.value("shallowGradient").toString() == QLatin1String("on")},
		{"conservatism", number("conservatism")}, {"bailout", parameters.value("bailout").toBool()},
		{"setpoint", number("bottomSetpoint", 1.3)}, {"bottomSetpoint", number("bottomSetpoint", 1.3)},
		{"descentSetpoint", number("descentSetpoint", 0.7)}, {"decoSetpoint", number("decoSetpoint", 1.3)},
		{"scrLoopVolume", number("scrLoopVolume", 8)}, {"scrMetabolicO2", number("scrMetabolicO2", 1)},
		{"stressTimeMin", number("stressTimeMin", 10)}, {"problemSolveMin", number("problemSolveMin", 3)},
		{"sacStress", number("sacStress", 50)}, {"diluentUseAsBailout", parameters.value("diluentUseAsBailout").toBool(false)},
		{"minDecoProfile", QJsonObject{{"enabled", parameters.value("minDecoProfileEnable").toString() == QLatin1String("yes")},
			{"m9", number("minDeco9m")}, {"m6", number("minDeco6m")}, {"isMetric", true}}}};
	std::vector<cylinder_t> supplied;
	QJsonArray cylinders;
	static const QRegularExpression mixPattern(QStringLiteral("^\\s*(\\d+(?:\\.\\d+)?)\\s*/\\s*(\\d+(?:\\.\\d+)?)\\s*$"));
	for (const auto &value : cylinderData) {
		const auto row = value.toMap();
		QString mix = row.value("mix").toString();
		if (mix.compare(QLatin1String("Air"), Qt::CaseInsensitive) == 0) mix = QStringLiteral("21/0");
		if (mix == QLatin1String("100%")) mix = QStringLiteral("100/0");
		const auto parsed = mixPattern.match(mix);
		if (!parsed.hasMatch()) return refused(tr("Enter explicit oxygen/helium percentages for every gas."));
		const double oxygen = parsed.captured(1).toDouble(), helium = parsed.captured(2).toDouble();
		if (oxygen <= 0 || helium < 0 || oxygen + helium > 100) return refused(tr("Gas fractions must be valid and total no more than 100%."));
		const int role = row.value("use").toInt();
		if (role < OC_GAS || role >= NUM_GAS_USE || role == TRAVEL_OC)
			return refused(tr("Explicit travel-gas geometry remains available with the Subsurface profile."));
		cylinder_t cylinder;
		const auto type = get_tank_info_data(tank_info_table, row.value("type").toString().toStdString());
		cylinder.type.size = type.first;
		cylinder.type.description = row.value("type").toString().toStdString();
		const double fill = row.value("pressure").toDouble();
		cylinder.start.mbar = prefs.units.pressure == units::BAR ? std::lround(fill * 1000.0) : psi_to_mbar(fill);
		cylinder.type.workingpressure = cylinder.start;
		cylinder.end = cylinder.start;
		cylinder.cylinder_use = static_cast<cylinderuse>(role);
		cylinder.gasmix.o2.permille = std::lround(oxygen * 10);
		cylinder.gasmix.he.permille = std::lround(helium * 10);
		if (role != NOT_USED && (cylinder.type.size.mliter <= 0 || cylinder.start.mbar <= 0)) return refused(tr("Select a known cylinder size and positive fill pressure."));
		cylinders.append(QJsonObject{{"o2", oxygen}, {"he", helium}, {"role", role}, {"sizeLitres", cylinder.type.size.mliter / 1000.0}, {"startBar", cylinder.start.mbar / 1000.0}});
		supplied.push_back(std::move(cylinder));
	}
	QJsonArray levels, decoGases;
	std::vector<int> desiredTimes;
	for (const auto &value : segmentData) {
		const auto row = value.toMap();
		const int gas = row.value("gas").toInt();
		const double depth = prefs.units.length == units::METERS ? row.value("depth").toDouble() : row.value("depth").toDouble() * 0.3048;
		const int runtime = std::lround(row.value("duration").toDouble() * 60.0);
		if (gas < 0 || gas >= static_cast<int>(supplied.size()) || supplied[gas].cylinder_use == NOT_USED ||
		    supplied[gas].cylinder_use == OXYGEN || !std::isfinite(depth) || depth <= 0 || runtime <= 0 ||
		    (!desiredTimes.empty() && runtime <= desiredTimes.back())) return refused(tr("Use increasing runtime waypoints with available breathing gases."));
		if (mode == CCR && std::abs(row.value("setpoint").toDouble() - settings.value("bottomSetpoint").toDouble() * 1000.0) > 0.1)
			return refused(tr("This LSP profile uses one bottom setpoint. Use the Subsurface profile for independently changing level setpoints."));
		if (row.value("divemode", mode).toInt() != mode) return refused(tr("This LSP profile uses one circuit. Use the Subsurface profile for mixed-circuit waypoints."));
		const auto gasData = cylinders.at(gas).toObject();
		const double duration = desiredTimes.empty() ? runtime / 60.0 : std::max(1.0 / 60.0, (runtime - desiredTimes.back()) / 60.0);
		levels.append(QJsonObject{{"depth", depth}, {"time", duration}, {"o2", gasData.value("o2")}, {"he", gasData.value("he")}});
		desiredTimes.push_back(runtime);
	}
	if (mode == OC && levels.at(0).toObject().value("o2").toDouble() * pressure / 100.0 < 0.16)
		return refused(tr("A hypoxic surface gas needs a verified travel-gas profile. Use the Subsurface profile for this input."));
	for (int i = 0; i < static_cast<int>(supplied.size()); ++i) {
		if (supplied[i].cylinder_use != OC_GAS) continue;
		const auto gas = cylinders.at(i).toObject();
		decoGases.append(QJsonObject{{"o2", gas.value("o2")}, {"he", gas.value("he")}});
	}
	QJsonArray prior;
	QString historyError;
	if (!priorProfiles(when.toSecsSinceEpoch(), number("waterVapor"), prior, historyError)) return refused(historyError);
	const QByteArray priorWire = QJsonDocument(prior).toJson(QJsonDocument::Compact);
	const auto model = parameters.value("model").toString(QStringLiteral("ZHLC_GF"));
	QJsonObject request{{"schema", 1}, {"model", model}, {"levels", levels}, {"decoGases", decoGases}, {"settings", settings},
		{"environment", environment(pressure, barPerMetre(water, salinity), number("waterVapor"), parameters.value("acclimatized").toBool(true))}};
	QJsonObject data{{"schema", 1}, {"engineRequest", request}, {"cylinders", cylinders}, {"priorProfiles", prior},
		{"start", static_cast<double>(when.toSecsSinceEpoch())}, {"bottomSacLpm", number("sacBottom")}, {"decoSacLpm", number("sacDeco")},
		{"noDecompression", parameters.value("recreational").toBool()}};
	QJsonObject projection, response;
	QElapsedTimer elapsed; elapsed.start();
	// Continuation levels in LSP are holds, while Neo edits end-runtime
	// waypoints. Resolve each arrival with LSP itself, never subtract an assumed
	// ascent or silently move an entered endpoint past intervening deco.
	for (int pass = 0; pass < levels.size(); ++pass) {
		if (elapsed.elapsed() > 20000) return refused(tr("Waypoint normalization exceeded its bounded calculation time."));
		request["levels"] = levels; data["engineRequest"] = request;
		response = LspPlanner::project(data);
		if (!response.value("ok").toBool()) {
			const auto detail = response.value("result").toObject().value("error").toString(response.value("error").toString());
			return refused(detail.isEmpty() ? tr("LSP could not generate this profile.") : detail);
		}
		projection = response.value("result").toObject();
		if (pass + 1 >= levels.size()) break;
		QJsonArray holds;
		for (const auto &row : projection.value("timeline").toArray()) if (row.toObject().value("phase").toString() == QLatin1String("level")) holds.append(row);
		if (holds.size() != levels.size()) return refused(tr("LSP did not preserve the entered level sequence."));
		const auto hold = holds.at(pass + 1).toObject();
		const int arrival = hold.value("runTime").toInt() - hold.value("duration").toInt();
		if (arrival >= desiredTimes[pass + 1]) return refused(tr("The requested level runtime cannot contain its required ascent and decompression. Increase that runtime."));
		auto level = levels.at(pass + 1).toObject(); level["time"] = (desiredTimes[pass + 1] - arrival) / 60.0; levels[pass + 1] = level;
	}
	QJsonArray holds;
	for (const auto &row : projection.value("timeline").toArray()) if (row.toObject().value("phase").toString() == QLatin1String("level")) holds.append(row);
	if (holds.size() != static_cast<int>(desiredTimes.size())) return refused(tr("LSP did not preserve all entered waypoints."));
	for (int i = 0; i < holds.size(); ++i) if (std::abs(holds.at(i).toObject().value("runTime").toInt() - desiredTimes[i]) > 1)
		return refused(tr("LSP could not prove the entered runtime baseline."));
	auto candidate = std::make_unique<dive>();
	make_planner_dc(&candidate->dcs[0]);
	candidate->when = when.toSecsSinceEpoch(); candidate->dcs[0].when = candidate->when;
	candidate->dcs[0].divemode = static_cast<divemode_t>(mode);
	candidate->surface_pressure.mbar = std::lround(pressure * 1000); candidate->dcs[0].surface_pressure = candidate->surface_pressure;
	candidate->salinity = salinity; candidate->dcs[0].salinity = salinity;
	QVariantList gasAnalysis;
	const auto volumes = projection.value("gasVolumes").toArray();
	const double reserve = number("reserveBar", 40);
	for (int i = 0; i < static_cast<int>(supplied.size()); ++i) {
		auto cylinder = supplied[i];
		const auto volume = volumes.at(i).toObject();
		const double used = volume.value("usedLitres").toDouble(), deco = volume.value("decoLitres").toDouble();
		const double remaining = std::max(0.0, cylinder.type.size.mliter / 1000.0 * cylinder.start.mbar / 1000.0 - used);
		const double endBar = cylinder.type.size.mliter > 0 ? cylinder.start.mbar / 1000.0 - used / (cylinder.type.size.mliter / 1000.0) : 0;
		if(!std::isfinite(used)||!std::isfinite(deco)||used*1000>INT_MAX||deco*1000>INT_MAX||std::abs(endBar)*1000>INT_MAX)
			return refused(tr("This plan exceeds canonical cylinder-volume storage bounds."));
		cylinder.end.mbar = std::lround(endBar * 1000.0); cylinder.gas_used.mliter = std::lround(used * 1000.0); cylinder.deco_gas_used.mliter = std::lround(deco * 1000.0);
		candidate->cylinders.add(i, cylinder);
		if (cylinder.cylinder_use == NOT_USED) continue;
		const auto gas = cylinders.at(i).toObject();
		const QString mix = QStringLiteral("%1/%2").arg(gas.value("o2").toDouble()).arg(gas.value("he").toDouble());
		gasAnalysis.append(QVariantMap{{"mix", mix}, {"bottomMod", get_depth_string(depth_t{.mm=static_cast<int>(std::lround(volume.value("bottomModMetres").toDouble()*1000))}, true)},
			{"decoSwitch", get_depth_string(depth_t{.mm=static_cast<int>(std::lround(volume.value("decoModMetres").toDouble()*1000))}, true)}, {"used", get_volume_string(volume_t{.mliter=static_cast<int>(std::lround(used*1000))}, true)},
			{"decoUsed", get_volume_string(volume_t{.mliter=static_cast<int>(std::lround(deco*1000))}, true)}, {"remaining", get_volume_string(volume_t{.mliter=static_cast<int>(std::lround(remaining*1000))}, true)},
			{"startPressure", get_pressure_string(cylinder.start, true)}, {"endPressure", get_pressure_string(cylinder.end, true)}, {"belowMinimum", endBar<=10}, {"belowReserve", endBar>10&&endBar<reserve}});
	}
	for (const auto &value : projection.value("profile").toArray()) {
		const auto point = value.toObject(); struct sample sample;
		sample.time.seconds = point.value("time").toInt(); sample.depth.mm = point.value("depth").toInt();
		sample.tts.seconds = point.value("tts").toInt(); sample.in_deco = point.value("inDeco").toBool();
		sample.setpoint.mbar = point.value("setpoint").toInt(); sample.cns = std::lround(point.value("cns").toDouble());
		if (!candidate->dcs[0].samples.empty() && candidate->dcs[0].samples.back().time.seconds == sample.time.seconds) candidate->dcs[0].samples.back() = sample;
		else candidate->dcs[0].samples.push_back(sample);
	}
	for (const auto &value : projection.value("timeline").toArray()) {
		const auto row = value.toObject();
		if (row.value("gasSwitch").toBool()) add_gas_switch_event(candidate.get(), &candidate->dcs[0], row.value("runTime").toInt()-row.value("duration").toInt(), row.value("cylinder").toInt());
	}
	candidate->fixup_dive();
	int maximumDepth=0; for(const auto &sample:candidate->dcs[0].samples) maximumDepth=std::max(maximumDepth,sample.depth.mm);
	candidate->maxdepth.mm=maximumDepth;candidate->dcs[0].maxdepth=candidate->maxdepth;
	candidate->duration.seconds = projection.value("runtimeSeconds").toInt(); candidate->dcs[0].duration = candidate->duration;
	candidate->otu = projection.value("otu").toInt(); candidate->cns = std::lround(projection.value("cns").toDouble());
	QStringList header{QStringLiteral("Engine: LSP D-Planner+ %1").arg(LspPlanner::identity().value("version").toString()),
		QStringLiteral("Source: %1").arg(LspPlanner::identity().value("sourceCommit").toString()),
		QStringLiteral("Model: %1").arg(model), QStringLiteral("GF: %1/%2").arg(number("gfLowInput")).arg(number("gfHighInput")),
		QStringLiteral("Planned start: %1 %2").arg(input.value("date").toString(), input.value("time").toString()),
		QStringLiteral("Resolved LSP settings: %1").arg(QString::fromUtf8(QJsonDocument(projection.value("engineRequest").toObject()).toJson(QJsonDocument::Compact)))};
	QStringList footer{QStringLiteral("CNS: %1%  OTU: %2").arg(projection.value("cns").toDouble()).arg(candidate->otu)};
	for (const auto &warning : projection.value("warnings").toArray()) footer.append(warning.toString());
	for (const auto &gas : gasAnalysis) {
		const auto g = gas.toMap(); footer.append(QStringLiteral("%1: used %2; remaining %3; end %4").arg(g.value("mix").toString(),g.value("used").toString(),g.value("remaining").toString(),g.value("endPressure").toString()));
	}
	QVariantMap metadata = projection.toVariantMap(); metadata.remove("raw"); metadata.remove("error"); metadata.remove("gasVolumes");
	metadata["version"] = 2; metadata["engine"] = LspPlanner::identity().toVariantMap(); metadata["input"] = input;
	metadata["priorInputsSha256"] = QString::fromLatin1(QCryptographicHash::hash(priorWire,QCryptographicHash::Sha256).toHex());
	metadata["reportHeader"] = header.join(QLatin1Char('\n')); metadata["reportFooter"] = footer.join(QLatin1Char('\n')); metadata["gasAnalysis"] = gasAnalysis;
	const QString notes = header.join(QLatin1Char('\n')) + QLatin1Char('\n') + footer.join(QLatin1Char('\n'));
	candidate->notes = (QStringLiteral("<pre>")+notes.toHtmlEscaped()+QStringLiteral("</pre>")+neoPlanMetadataMarker(metadata)).toStdString();
	QVariantMap result = projection.toVariantMap(); result.remove("raw"); result["notes"] = notes; result["engine"] = LspPlanner::identity().toVariantMap();
	result["gasAnalysis"] = gasAnalysis; result["dateTimeValid"] = true; result["newDiveId"] = -1;
	if (shouldSave && projection.value("planSaveAllowed").toBool()) {
		auto saved = std::make_unique<dive>(); copy_dive(candidate.get(), saved.get()); result["newDiveId"] = saved->id;
		Command::addDive(std::move(saved), false, true);
	}
	delete d; d = candidate.release(); dcNr = 0; diveplan.dp.clear();
	cylindersModel()->updateDive(d, dcNr);
	emit planPreviewChanged();
	return result;
}
