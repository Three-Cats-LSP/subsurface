// SPDX-License-Identifier: GPL-2.0
#include "planner.h"
#include "pref.h"

int ascent_velocity(depth_t depth, depth_t avg_depth, int)
{
	/* Keep the mature profile/planner ascent-rate behavior in one small,
	 * browser-safe translation unit shared by every target. */
	if (depth.mm * 4 > avg_depth.mm * 3)
		return prefs.ascrate75;
	if (depth.mm * 2 > avg_depth.mm)
		return prefs.ascrate50;
	return depth.mm > 6000 ? prefs.ascratestops : prefs.ascratelast6m;
}
