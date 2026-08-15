// SPDX-License-Identifier: GPL-2.0
#ifndef NATIVE_DIVELOG_SUMMARY_H
#define NATIVE_DIVELOG_SUMMARY_H

#include <QDateTime>
#include <QString>
#include <QVector>

class QIODevice;

struct native_sample_summary {
	int time_seconds = 0;
	double depth_m = 0.0;
	double temperature_c = 0.0;
	double pressure_bar = 0.0;
	int ndl_seconds = 0;
	int tts_seconds = 0;
	double stop_depth_m = 0.0;
	int stop_time_seconds = 0;
	double cns_percent = 0.0;
	double setpoint_bar = 0.0;
	bool has_depth = false;
	bool has_temperature = false;
	bool has_pressure = false;
	bool has_ndl = false;
	bool has_tts = false;
	bool has_stop_depth = false;
	bool has_stop_time = false;
	bool has_cns = false;
	bool has_setpoint = false;
	bool in_deco = false;
	double current_gf_percent = 0.0;
	double surface_gf_percent = 0.0;
	double calculated_ceiling_m = 0.0;
	int calculated_ndl_seconds = 0;
	int calculated_tts_seconds = 0;
	bool has_current_gf = false;
	bool has_surface_gf = false;
	bool has_calculated_ceiling = false;
	bool has_calculated_ndl = false;
	bool has_calculated_tts = false;
};

struct native_dive_summary {
	int number = 0;
	QDateTime when;
	int duration_seconds = 0;
	double max_depth_m = 0.0;
	double water_temperature_c = 0.0;
	bool has_max_depth = false;
	bool has_water_temperature = false;
	QString location;
	QString buddy;
	QString notes;
	QString suit;
	QString gas;
	QString mode = QStringLiteral("OC");
	QString gear;
	QVector<native_sample_summary> samples;
	bool profile_calculated = false;
};

struct native_divelog_summary {
	QVector<native_dive_summary> dives;
	QString error;
	bool ok = false;
};

// Read the native XML format emitted by core/save-xml.cpp. This deliberately
// does not guess at foreign formats: those continue to use the mature desktop
// import pipeline and will be added to WebAssembly as their dependencies are
// made browser-safe.
native_divelog_summary read_native_divelog_summary(QIODevice &device, const QString &sourceName);

#endif // NATIVE_DIVELOG_SUMMARY_H
