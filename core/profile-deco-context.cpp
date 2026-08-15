// SPDX-License-Identifier: GPL-2.0
#include "deco.h"
#include "dive.h"
#include "divelist.h"
#include "interpolate.h"
#include "pref.h"
#include "range.h"
#include "sample.h"

#include <string>

static void add_dive_to_deco(struct deco_state *ds, const struct dive &dive, bool in_planner)
{
	const struct divecomputer &dc = dive.dcs[0];
	gasmix_loop loop_gas(dive, dc);
	divemode_loop loop_mode(dc);
	for (auto [psample, sample]: pairwise_range(dc.samples)) {
		const int t0 = psample.time.seconds;
		const int t1 = sample.time.seconds;
		for (int second = t0; second < t1; ++second) {
			depth_t depth = interpolate(psample.depth, sample.depth, second - t0, t1 - t0);
			[[maybe_unused]] auto [divemode, _cylinder_index, gasmix] =
				get_dive_status_at(dive, dc, second, &loop_mode, &loop_gas);
			add_segment(ds, dive.depth_to_bar(depth), *gasmix, 1, sample.setpoint.mbar,
				    divemode, dive.sac, in_planner);
		}
	}
}

int dive_table::init_decompression(struct deco_state *ds, const struct dive *dive, bool in_planner) const
{
	int surface_time = 48 * 60 * 60;
	timestamp_t last_endtime = 0, last_starttime = 0;
	bool deco_init = false;
	if (!dive)
		return false;

	const int nr_dives = static_cast<int>(size());
	const size_t divenr = owning_table<struct dive>::get_idx(dive);
	int i = divenr != std::string::npos ? static_cast<int>(divenr) : nr_dives;
	while (i + 1 < nr_dives) {
		if ((*this)[i]->when > dive->when)
			break;
		++i;
	}
	while (i > 0) {
		if ((*this)[i - 1]->when < dive->when)
			break;
		--i;
	}
	last_starttime = dive->when;
	while (i--) {
		if (static_cast<size_t>(i) == divenr && i > 0)
			--i;
		const struct dive &previous = *(*this)[i];
		if (dive->divetrip && previous.divetrip != dive->divetrip)
			continue;
		if (previous.when >= dive->when || previous.endtime() + 48 * 60 * 60 < last_starttime)
			break;
		last_starttime = previous.when;
	}

	while (++i < nr_dives) {
		const struct dive &previous = *(*this)[i];
		if (dive->divetrip && dive->divetrip != previous.divetrip)
			continue;
		if (previous.when >= dive->when)
			break;
		if (static_cast<size_t>(i) == divenr)
			continue;

		const double surface_pressure = previous.get_surface_pressure().mbar / 1000.0;
		if (!deco_init) {
			clear_deco(ds, surface_pressure, in_planner);
			deco_init = true;
		} else {
			surface_time = previous.when - last_endtime;
			if (surface_time < 0)
				return surface_time;
			add_segment(ds, surface_pressure, gasmix_air, surface_time, 0, OC, prefs.decosac, in_planner);
		}
		add_dive_to_deco(ds, previous, in_planner);
		last_starttime = previous.when;
		last_endtime = previous.endtime();
		clear_vpmb_state(ds);
	}

	const double surface_pressure = dive->get_surface_pressure().mbar / 1000.0;
	if (!deco_init) {
		clear_deco(ds, surface_pressure, in_planner);
	} else {
		surface_time = dive->when - last_endtime;
		if (surface_time < 0)
			return surface_time;
		add_segment(ds, surface_pressure, gasmix_air, surface_time, 0, OC, prefs.decosac, in_planner);
	}
	tissue_tolerance_calc(ds, dive, surface_pressure, in_planner);
	return surface_time;
}
