// SPDX-License-Identifier: GPL-2.0
#ifndef NATIVE_DIVELOG_SUMMARY_H
#define NATIVE_DIVELOG_SUMMARY_H

#include <QDateTime>
#include <QString>
#include <QVector>

class QIODevice;

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
