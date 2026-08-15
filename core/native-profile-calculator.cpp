// SPDX-License-Identifier: GPL-2.0
#include "native-profile-calculator.h"

#include "dive.h"
#include "divelist.h"
#include "native-divelog-summary.h"
#include "pref.h"
#include "profile.h"
#include "sample.h"

#include <QMutex>
#include <QMutexLocker>

#include <algorithm>
#include <cmath>
#include <memory>
#include <vector>

namespace {

divemode_t diveMode(const QString &mode)
{
	if (mode.compare(QStringLiteral("CCR"), Qt::CaseInsensitive) == 0)
		return CCR;
	if (mode.compare(QStringLiteral("pSCR"), Qt::CaseInsensitive) == 0)
		return PSCR;
	if (mode.compare(QStringLiteral("Freedive"), Qt::CaseInsensitive) == 0)
		return FREEDIVE;
	return OC;
}

gasmix parsedGas(const QString &name)
{
	gasmix mix = gasmix_air;
	QString gas = name.trimmed();
	if (gas.startsWith(QStringLiteral("EAN"), Qt::CaseInsensitive)) {
		bool ok = false;
		const int oxygen = gas.mid(3).toInt(&ok);
		if (ok)
			mix.o2.permille = std::clamp(oxygen * 10, 0, 1000);
	} else if (gas.startsWith(QStringLiteral("Tx"), Qt::CaseInsensitive)) {
		const QStringList parts = gas.mid(2).split(QLatin1Char('/'));
		bool oxygenOk = false;
		bool heliumOk = false;
		const int oxygen = parts.value(0).toInt(&oxygenOk);
		const int helium = parts.value(1).toInt(&heliumOk);
		if (oxygenOk && heliumOk) {
			mix.o2.permille = std::clamp(oxygen * 10, 0, 1000);
			mix.he.permille = std::clamp(helium * 10, 0, 1000 - mix.o2.permille);
		}
	}
	return mix;
}

std::unique_ptr<dive> canonicalDive(const native_dive_summary &source)
{
	auto result = std::make_unique<dive>();
	result->number = source.number;
	result->when = source.when.isValid() ? source.when.toSecsSinceEpoch() : 0;
	result->duration.seconds = source.duration_seconds;
	result->maxdepth.mm = std::lround(source.max_depth_m * 1000.0);
	if (source.has_water_temperature)
		result->watertemp.mkelvin = C_to_mkelvin(source.water_temperature_c);

	result->cylinders.emplace_back();
	result->cylinders[0].gasmix = parsedGas(source.gas);
	result->cylinders[0].type.description = source.gear.toStdString();

	divecomputer &dc = result->dcs[0];
	dc.when = result->when;
	dc.duration.seconds = source.duration_seconds;
	dc.maxdepth = result->maxdepth;
	dc.watertemp = result->watertemp;
	dc.divemode = diveMode(source.mode);
	dc.model = "Subsurface native log";
	dc.samples.reserve(source.samples.size());

	long long depthTime = 0;
	int previousTime = 0;
	int previousDepth = 0;
	for (const native_sample_summary &input : source.samples) {
		sample output;
		output.time.seconds = input.time_seconds;
		if (input.has_depth)
			output.depth.mm = std::lround(input.depth_m * 1000.0);
		if (input.has_temperature)
			output.temperature.mkelvin = C_to_mkelvin(input.temperature_c);
		if (input.has_pressure) {
			output.pressure[0].mbar = std::lround(input.pressure_bar * 1000.0);
			output.sensor[0] = 0;
		}
		if (input.has_ndl)
			output.ndl.seconds = input.ndl_seconds;
		if (input.has_tts)
			output.tts.seconds = input.tts_seconds;
		if (input.has_stop_depth)
			output.stopdepth.mm = std::lround(input.stop_depth_m * 1000.0);
		if (input.has_stop_time)
			output.stoptime.seconds = input.stop_time_seconds;
		if (input.has_cns)
			output.cns = std::lround(input.cns_percent);
		if (input.has_setpoint)
			output.setpoint.mbar = std::lround(input.setpoint_bar * 1000.0);
		output.in_deco = input.in_deco;
		dc.samples.push_back(output);

		const int delta = std::max(0, input.time_seconds - previousTime);
		depthTime += static_cast<long long>(previousDepth + output.depth.mm) * delta / 2;
		previousTime = input.time_seconds;
		previousDepth = output.depth.mm;
	}
	if (previousTime > 0)
		dc.meandepth.mm = static_cast<int>(depthTime / previousTime);
	result->meandepth = dc.meandepth;
	return result;
}

const plot_data *nearestPlotEntry(const plot_info &plot, int seconds)
{
	if (plot.entry.empty())
		return nullptr;
	auto best = plot.entry.begin();
	int bestDistance = std::abs(best->sec - seconds);
	for (auto it = std::next(plot.entry.begin()); it != plot.entry.end(); ++it) {
		const int distance = std::abs(it->sec - seconds);
		if (distance < bestDistance) {
			best = it;
			bestDistance = distance;
		}
	}
	return &*best;
}

QMutex profileCalculationMutex;

} // namespace

bool calculate_native_profile(native_divelog_summary &summary, int diveIndex, QString *error)
{
	if (diveIndex < 0 || diveIndex >= summary.dives.size()) {
		if (error)
			*error = QStringLiteral("The selected dive is outside the native log.");
		return false;
	}
	native_dive_summary &selected = summary.dives[diveIndex];
	if (selected.profile_calculated)
		return true;
	if (selected.samples.size() < 2) {
		if (error)
			*error = QStringLiteral("At least two recorded samples are required for profile calculations.");
		return false;
	}

	dive_table context;
	dive *selectedDive = nullptr;
	std::vector<std::unique_ptr<dive>> canonicalDives;
	canonicalDives.reserve(summary.dives.size());
	for (int index = 0; index < summary.dives.size(); ++index) {
		auto item = canonicalDive(summary.dives[index]);
		if (index == diveIndex)
			selectedDive = item.get();
		canonicalDives.push_back(std::move(item));
	}
	std::sort(canonicalDives.begin(), canonicalDives.end(), [](const auto &left, const auto &right) {
		return left->when < right->when;
	});
	for (auto &item : canonicalDives)
		context.put_back(std::move(item));
	if (!selectedDive) {
		if (error)
			*error = QStringLiteral("The selected dive could not be converted to Subsurface's canonical model.");
		return false;
	}

	QMutexLocker lock(&profileCalculationMutex);
	const bool oldCalcNdlTts = prefs.calcndltts;
	const bool oldCalcCeiling = prefs.calcceiling;
	prefs.calcndltts = true;
	prefs.calcceiling = true;
	plot_info plot = create_plot_info_new(context, selectedDive, &selectedDive->dcs[0], nullptr);
	prefs.calcndltts = oldCalcNdlTts;
	prefs.calcceiling = oldCalcCeiling;

	if (plot.entry.empty()) {
		if (error)
			*error = QStringLiteral("Subsurface's profile pipeline produced no calculated samples.");
		return false;
	}

	for (native_sample_summary &sample : selected.samples) {
		const plot_data *entry = nearestPlotEntry(plot, sample.time_seconds);
		if (!entry)
			continue;
		sample.current_gf_percent = entry->current_gf * 100.0;
		sample.surface_gf_percent = entry->surface_gf;
		sample.calculated_ceiling_m = std::max(0, entry->ceiling.mm) / 1000.0;
		sample.calculated_ndl_seconds = entry->ndl_calc;
		sample.calculated_tts_seconds = entry->tts_calc;
		sample.has_current_gf = true;
		sample.has_surface_gf = true;
		sample.has_calculated_ceiling = true;
		sample.has_calculated_ndl = entry->ndl_calc > 0 && !entry->in_deco_calc;
		sample.has_calculated_tts = entry->tts_calc > 0;
	}
	selected.profile_calculated = true;
	return true;
}
