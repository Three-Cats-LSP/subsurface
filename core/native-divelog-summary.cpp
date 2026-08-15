// SPDX-License-Identifier: GPL-2.0
#include "native-divelog-summary.h"

#include <QHash>
#include <QIODevice>
#include <QStringList>
#include <QXmlStreamAttributes>
#include <QXmlStreamReader>

#include <algorithm>
#include <optional>

namespace {

QString normalizedId(const QString &value)
{
	return value.trimmed().toLower();
}

double numberPart(QString value, bool *ok)
{
	value = value.trimmed();
	int end = 0;
	while (end < value.size() && (value.at(end).isDigit() || value.at(end) == QLatin1Char('.') ||
		value.at(end) == QLatin1Char('-') || value.at(end) == QLatin1Char('+') ||
		value.at(end) == QLatin1Char(',')))
		++end;
	return value.left(end).replace(QLatin1Char(','), QLatin1Char('.')).toDouble(ok);
}

double depthInMeters(const QString &value, bool *ok)
{
	const double depth = numberPart(value, ok);
	if (!*ok)
		return 0.0;
	return value.contains(QStringLiteral("ft"), Qt::CaseInsensitive) ? depth * 0.3048 : depth;
}

double temperatureInCelsius(const QString &value, bool *ok)
{
	const double temperature = numberPart(value, ok);
	if (!*ok)
		return 0.0;
	return value.contains(QLatin1Char('F'), Qt::CaseInsensitive) ? (temperature - 32.0) * 5.0 / 9.0 : temperature;
}

int durationInSeconds(QString value)
{
	value = value.section(QLatin1Char(' '), 0, 0).trimmed();
	const QStringList parts = value.split(QLatin1Char(':'));
	bool ok = false;
	if (parts.size() == 3) {
		const int hours = parts.at(0).toInt(&ok);
		if (!ok)
			return 0;
		return hours * 3600 + parts.at(1).toInt() * 60 + parts.at(2).toInt();
	}
	if (parts.size() == 2) {
		const int minutes = parts.at(0).toInt(&ok);
		if (!ok)
			return 0;
		return minutes * 60 + parts.at(1).toInt();
	}
	const double minutes = value.toDouble(&ok);
	return ok ? qRound(minutes * 60.0) : 0;
}

double percentage(const QString &value, bool *ok)
{
	return numberPart(value, ok);
}

QString gasName(const QXmlStreamAttributes &attributes)
{
	bool o2Ok = false;
	bool heOk = false;
	const double o2 = percentage(attributes.value(QStringLiteral("o2")).toString(), &o2Ok);
	const double he = percentage(attributes.value(QStringLiteral("he")).toString(), &heOk);
	if (heOk && he > 0.0)
		return QStringLiteral("Tx%1/%2").arg(qRound(o2Ok ? o2 : 0.0)).arg(qRound(he));
	if (o2Ok && qAbs(o2 - 21.0) > 0.5)
		return QStringLiteral("EAN%1").arg(qRound(o2));
	return QStringLiteral("Air");
}

QString modeName(const QString &value)
{
	if (value.compare(QStringLiteral("CCR"), Qt::CaseInsensitive) == 0)
		return QStringLiteral("CCR");
	if (value.compare(QStringLiteral("PSCR"), Qt::CaseInsensitive) == 0)
		return QStringLiteral("pSCR");
	if (value.compare(QStringLiteral("Freedive"), Qt::CaseInsensitive) == 0)
		return QStringLiteral("Freedive");
	if (value.compare(QStringLiteral("Gauge"), Qt::CaseInsensitive) == 0)
		return QStringLiteral("Gauge");
	return QStringLiteral("OC");
}

void updateDepth(native_dive_summary &dive, const QString &value)
{
	bool ok = false;
	const double depth = depthInMeters(value, &ok);
	if (ok && (!dive.has_max_depth || depth > dive.max_depth_m)) {
		dive.max_depth_m = depth;
		dive.has_max_depth = true;
	}
}

void updateTemperature(native_dive_summary &dive, const QString &value)
{
	bool ok = false;
	const double temperature = temperatureInCelsius(value, &ok);
	if (ok && !dive.has_water_temperature) {
		dive.water_temperature_c = temperature;
		dive.has_water_temperature = true;
	}
}

QDateTime diveDateTime(const QXmlStreamAttributes &attributes)
{
	const QDate date = QDate::fromString(attributes.value(QStringLiteral("date")).toString(), Qt::ISODate);
	QTime time = QTime::fromString(attributes.value(QStringLiteral("time")).toString(), QStringLiteral("HH:mm:ss"));
	if (!time.isValid())
		time = QTime(0, 0);
	return date.isValid() ? QDateTime(date, time, Qt::UTC) : QDateTime();
}

} // namespace

native_divelog_summary read_native_divelog_summary(QIODevice &device, const QString &sourceName)
{
	native_divelog_summary result;
	QXmlStreamReader reader(&device);
	QHash<QString, QString> siteNames;
	std::optional<native_dive_summary> currentDive;
	QString currentSiteId;
	QString textElement;
	QString textValue;
	bool nativeRoot = false;
	bool invalidDive = false;

	while (!reader.atEnd()) {
		const auto token = reader.readNext();
		if (token == QXmlStreamReader::StartElement) {
			const QString name = reader.name().toString();
			const QXmlStreamAttributes attributes = reader.attributes();
			if (name == QLatin1String("divelog")) {
				nativeRoot = true;
			} else if (name == QLatin1String("site") && !currentDive) {
				siteNames.insert(normalizedId(attributes.value(QStringLiteral("uuid")).toString()),
					attributes.value(QStringLiteral("name")).toString().trimmed());
			} else if (name == QLatin1String("dive")) {
				currentDive.emplace();
				currentDive->number = attributes.value(QStringLiteral("number")).toInt();
				currentDive->when = diveDateTime(attributes);
				currentDive->duration_seconds = durationInSeconds(attributes.value(QStringLiteral("duration")).toString());
				currentSiteId = normalizedId(attributes.value(QStringLiteral("divesiteid")).toString());
				invalidDive = attributes.value(QStringLiteral("invalid")) == QLatin1String("1");
			} else if (currentDive) {
				if (name == QLatin1String("depth")) {
					updateDepth(*currentDive, attributes.value(QStringLiteral("max")).toString());
				} else if (name == QLatin1String("temperature") || name == QLatin1String("divetemperature")) {
					updateTemperature(*currentDive, attributes.value(QStringLiteral("water")).toString());
				} else if (name == QLatin1String("sample")) {
					updateDepth(*currentDive, attributes.value(QStringLiteral("depth")).toString());
					updateTemperature(*currentDive, attributes.value(QStringLiteral("temp")).toString());
					currentDive->duration_seconds = std::max(currentDive->duration_seconds,
						durationInSeconds(attributes.value(QStringLiteral("time")).toString()));
				} else if (name == QLatin1String("divecomputer")) {
					if (currentDive->duration_seconds == 0)
						currentDive->duration_seconds = durationInSeconds(attributes.value(QStringLiteral("duration")).toString());
					const QString mode = attributes.value(QStringLiteral("dctype")).toString();
					if (!mode.isEmpty())
						currentDive->mode = modeName(mode);
				} else if (name == QLatin1String("cylinder") && currentDive->gas.isEmpty()) {
					currentDive->gas = gasName(attributes);
					currentDive->gear = attributes.value(QStringLiteral("description")).toString().trimmed();
				} else if (name == QLatin1String("notes") || name == QLatin1String("buddy") ||
					name == QLatin1String("suit") || name == QLatin1String("location")) {
					textElement = name;
					textValue.clear();
				}
			}
		} else if ((token == QXmlStreamReader::Characters || token == QXmlStreamReader::EntityReference) &&
			currentDive && !textElement.isEmpty()) {
			textValue += reader.text().toString();
		} else if (token == QXmlStreamReader::EndElement) {
			const QString name = reader.name().toString();
			if (currentDive && name == textElement) {
				const QString value = textValue.trimmed();
				if (name == QLatin1String("notes"))
					currentDive->notes = value;
				else if (name == QLatin1String("buddy"))
					currentDive->buddy = value;
				else if (name == QLatin1String("suit"))
					currentDive->suit = value;
				else if (name == QLatin1String("location"))
					currentDive->location = value;
				textElement.clear();
				textValue.clear();
			}
			if (name == QLatin1String("dive") && currentDive) {
				if (currentDive->location.isEmpty())
					currentDive->location = siteNames.value(currentSiteId);
				if (currentDive->gas.isEmpty())
					currentDive->gas = QStringLiteral("Air");
				if (!invalidDive)
					result.dives.push_back(std::move(*currentDive));
				currentDive.reset();
				currentSiteId.clear();
				invalidDive = false;
			}
		}
	}

	if (reader.hasError()) {
		result.error = QStringLiteral("%1:%2: %3")
			.arg(sourceName)
			.arg(reader.lineNumber())
			.arg(reader.errorString());
		return result;
	}
	if (!nativeRoot) {
		result.error = QStringLiteral("%1 is not a native Subsurface XML dive log.").arg(sourceName);
		return result;
	}

	std::stable_sort(result.dives.begin(), result.dives.end(), [](const auto &left, const auto &right) {
		return left.when < right.when;
	});
	result.ok = true;
	return result;
}
