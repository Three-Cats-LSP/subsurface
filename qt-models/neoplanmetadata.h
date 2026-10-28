// SPDX-License-Identifier: GPL-2.0
#ifndef NEOPLANMETADATA_H
#define NEOPLANMETADATA_H

#include <QByteArray>
#include <QCoreApplication>
#include <QStringList>
#include <QJsonDocument>
#include <QVariantMap>

#include <algorithm>
#include <cmath>
#include <string>

// Neo planner-only fields do not have canonical counterparts in the Subsurface
// dive schema. Keep a compact, invisible payload in notes so exact run/bottom/
// deco times and the rendered schedule survive save, sync and restart.
inline QString neoPlanMetadataMarker(const QVariantMap &metadata)
{
	const QByteArray json = QJsonDocument::fromVariant(metadata).toJson(QJsonDocument::Compact);
	const QByteArray encoded = json.toBase64(QByteArray::Base64UrlEncoding | QByteArray::OmitTrailingEquals);
	return QStringLiteral("\n<!--NEO_PLAN:%1-->").arg(QString::fromLatin1(encoded));
}

inline QVariantMap neoPlanMetadata(const std::string &notes)
{
	const QString value = QString::fromStdString(notes);
	const QString prefix = QStringLiteral("<!--NEO_PLAN:");
	const qsizetype begin = value.lastIndexOf(prefix);
	if (begin < 0)
		return {};
	const qsizetype payloadBegin = begin + prefix.size();
	const qsizetype end = value.indexOf(QStringLiteral("-->"), payloadBegin);
	if (end < 0)
		return {};
	const QByteArray encoded = value.mid(payloadBegin, end - payloadBegin).toLatin1();
	const QByteArray json = QByteArray::fromBase64(encoded, QByteArray::Base64UrlEncoding);
	const QJsonDocument document = QJsonDocument::fromJson(json);
	return document.isObject() ? document.toVariant().toMap() : QVariantMap();
}

inline QString neoPlanNotesWithoutMetadata(const std::string &notes)
{
	QString value = QString::fromStdString(notes);
	const qsizetype begin = value.lastIndexOf(QStringLiteral("<!--NEO_PLAN:"));
	if (begin >= 0)
		value.truncate(begin);
	return value.trimmed();
}

inline QString neoPlanClock(int seconds)
{
	seconds = std::max(0, seconds);
	return QStringLiteral("%1:%2").arg(seconds / 60).arg(seconds % 60, 2, 10, QLatin1Char('0'));
}

// Presentation only. The exact calculation remains in the metadata marker;
// exported dive settings use the saved request, never today's planner controls.
inline QString neoLspDiveSettings(const QVariantMap &metadata, bool metricDepth = true, bool barPressure = true, bool litreVolume = true)
{
	const auto request = metadata.value("engineRequest").toMap(), input = metadata.value("input").toMap();
	const auto settings = request.value("settings").toMap(), environment = request.value("environment").toMap();
	if (request.value("schema").toInt() != 1 || settings.isEmpty()) return {};
	const auto tr = [](const char *text) { return QCoreApplication::translate("NeoPlannerReport", text); };
	const auto number = [](double value, int places = 2) {
		QString result = QString::number(value, 'f', places);
		if (result.contains('.')) { while (result.endsWith('0')) result.chop(1); if (result.endsWith('.')) result.chop(1); }
		return result;
	};
	const auto depth = [&](double metres) { return number(metricDepth ? metres : metres / 0.3048) + (metricDepth ? " m" : " ft"); };
	const auto rate = [&](double metres) { return depth(metres) + tr("/min"); };
	const auto pressure = [&](double bar) { return number(barPressure ? bar : bar * 14.5037738, 3) + (barPressure ? " bar" : " psi"); };
	const auto sac = [&](double litres) { return number(litreVolume ? litres : litres * 0.0353146667) + (litreVolume ? " L/min" : " cu ft/min"); };
	const auto model = request.value("model").toString();
	const QMap<QString, QString> algorithms{{"ZHLC_GF", tr("Bühlmann ZHL-16C")}, {"VPMA", "VPM-A"}, {"VPMB", "VPM-B"},
		{"VPMBE", "VPM-B/E"}, {"VPMB_GFS", "VPM-B + GFS"}, {"VPMBFBO", "VPM-B/FBO"}};
	QStringList lines{tr("DIVE SETTINGS"), tr("Algorithm: %1").arg(algorithms.value(model, model))};
	if (model == "ZHLC_GF" || model == "VPMB_GFS") lines.append(tr("Gradient factors: %1/%2").arg(number(settings.value("gfLo").toDouble()), number(settings.value("gfHi").toDouble())));
	if (model.startsWith("VPM")) lines.append(tr("Conservatism: %1").arg(number(settings.value("conservatism").toDouble())));
	const auto circuit = settings.value("circuit").toString();
	lines.append(tr("Circuit: %1").arg(circuit == "CCR" ? tr("Closed circuit (CCR)") : circuit == "PSCR" || circuit == "pSCR" ? tr("Semi-closed circuit (pSCR)") : tr("Open circuit")));
	if (!input.value("date").toString().isEmpty()) lines.append(tr("Planned start: %1 %2").arg(input.value("date").toString(), input.value("time").toString()));
	const int water = input.value("waterType").toInt();
	lines.append(tr("Water: %1").arg(water == 1 ? tr("Fresh water") : water == 2 ? tr("EN 13319 water") : water == 3 ? tr("Custom density (%1 kg/m³)").arg(number(input.value("customSalinity").toDouble() / 10)) : tr("Sea water")));
	if (environment.contains("altSurfaceP")) lines.append(tr("Surface pressure: %1").arg(pressure(environment.value("altSurfaceP").toDouble())));
	lines.append(tr("Descent: %1").arg(rate(settings.value("descentRate").toDouble())));
	lines.append(tr("Ascent: %1; between stops: %2; final ascent: %3").arg(rate(settings.value("ascentRate").toDouble()), rate(settings.value("decoAscentRate").toDouble()), rate(settings.value("surfaceAscentRate").toDouble())));
	lines.append(tr("Stop spacing: %1; last stop: %2").arg(depth(settings.value("stepSize").toDouble()), depth(settings.value("lastStop").toDouble())));
	lines.append(tr("Minimum stop: %1 min; rounding: %2").arg(number(settings.value("minStopTime").toDouble()), settings.value("wholeMinStops").toBool() ? tr("whole minutes") : tr("exact seconds")));
	lines.append(tr("Oxygen limits: bottom %1 bar; decompression %2 bar").arg(number(settings.value("ppO2Bottom").toDouble()), number(settings.value("ppO2Deco").toDouble())));
	if (circuit == "CCR" || circuit == "PSCR" || circuit == "pSCR") {
		lines.append(tr("Setpoints: descent %1 bar; bottom %2 bar; decompression %3 bar").arg(number(settings.value("descentSetpoint").toDouble()), number(settings.value("bottomSetpoint").toDouble()), number(settings.value("decoSetpoint").toDouble())));
		if (settings.value("bailout").toBool()) lines.append(tr("Bailout ascent: enabled"));
	}
	if (circuit == "PSCR" || circuit == "pSCR") lines.append(tr("Loop volume: %1 L; metabolic oxygen: %2 L/min").arg(number(settings.value("scrLoopVolume").toDouble()), number(settings.value("scrMetabolicO2").toDouble())));
	auto parameters = input.value("lspParameters").toMap();
	const auto resolved = metadata.value("diveSettings").toMap(); for (auto it = resolved.begin(); it != resolved.end(); ++it) parameters[it.key()] = it.value();
	if (parameters.contains("sacBottom") && parameters.contains("sacDeco")) lines.append(tr("Gas consumption (SAC): bottom %1; decompression %2").arg(sac(parameters.value("sacBottom").toDouble()), sac(parameters.value("sacDeco").toDouble())));
	if (parameters.contains("reserveBar")) lines.append(tr("Gas reserve: %1").arg(pressure(parameters.value("reserveBar").toDouble())));
	lines.append(""); lines.append(tr("GASES"));
	const auto cylinders = input.value("cylinders").toList(), supplies = metadata.value("gasSupplies").toList(), analysis = metadata.value("gasAnalysis").toList();
	int active = 0;
	for (int i = 0; i < cylinders.size(); ++i) {
		const auto cylinder = cylinders[i].toMap(); const int use = cylinder.value("use").toInt();
		const auto supply = i < supplies.size() ? supplies[i].toMap() : QVariantMap();
		QString mix = cylinder.value("mix").toString(), fill;
		const auto parts = mix.split('/');
		if (mix.compare("Air", Qt::CaseInsensitive) == 0 || mix == "21/0") mix = tr("Air (21% oxygen)");
		else if (mix == "100%" || mix == "100/0") mix = tr("Oxygen (100%)");
		else if (parts.size() == 2) mix = parts[1].toDouble() > 0 ? tr("Trimix %1/%2").arg(parts[0], parts[1]) : tr("Nitrox %1 (EAN%1)").arg(parts[0]);
		const auto role = use == 1 ? tr("Diluent") : use == 2 ? tr("Loop oxygen") : use == 3 ? tr("Unused") : circuit == "CCR" || circuit == "PSCR" || circuit == "pSCR" ? tr("Bailout gas") : tr("Breathing gas");
		if (supply.contains("startBar")) fill = pressure(supply.value("startBar").toDouble());
		else if (use != 3 && active < analysis.size()) fill = analysis[active].toMap().value("startPressure").toString().replace("bar", " bar").replace("psi", " psi").simplified();
		if (use != 3) ++active;
		lines.append(tr("%1. %2 — %3; %4%5").arg(i + 1).arg(mix, role, cylinder.value("type").toString(), fill.isEmpty() ? QString() : tr("; fill %1").arg(fill)));
	}
	return lines.join('\n');
}

#endif
