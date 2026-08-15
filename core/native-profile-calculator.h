// SPDX-License-Identifier: GPL-2.0
#ifndef NATIVE_PROFILE_CALCULATOR_H
#define NATIVE_PROFILE_CALCULATOR_H

#include <QString>

struct native_divelog_summary;

// Enrich one parsed native dive with values produced by Subsurface's mature
// create_plot_info_new pipeline. No decompression calculations live here.
bool calculate_native_profile(native_divelog_summary &summary, int diveIndex, QString *error = nullptr);

#endif // NATIVE_PROFILE_CALCULATOR_H
