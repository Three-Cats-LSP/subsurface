// SPDX-License-Identifier: GPL-2.0
#include "neologmetadata.h"
#include <QJsonArray>
#include <QJsonDocument>
#include <QSettings>
#include <QXmlStreamReader>

std::optional<QJsonObject> NeoLogMetadata::parse(const QByteArray &bytes)
{
	if (bytes.isEmpty() || bytes.size() > Limit) return {};
	QJsonParseError error; const auto document = QJsonDocument::fromJson(bytes, &error); const auto body = document.object();
	if (error.error != QJsonParseError::NoError || !document.isObject() || body.value("schema") != QJsonValue(1) ||
	    body.value("format") != QJsonValue("neo-log-metadata-1") || !body.value("equipmentKits").isObject() ||
	    !body.value("collections").isArray() || !body.value("plannerPresets").isArray()) return {};
	return body;
}
std::optional<QByteArray> NeoLogMetadata::fromLog(const QByteArray &xml)
{
	if (xml.isEmpty() || xml.size() > 64 * 1024 * 1024) return {};
	QXmlStreamReader reader(xml); QStringList path; QByteArray metadata; bool found = false, root = false;
	while (!reader.atEnd()) {
		reader.readNext();
		if (reader.isDTD() || reader.isEntityReference()) return {};
		if (reader.isStartElement()) {
			if (path.isEmpty()) {
				if (root || reader.name() != QLatin1String("divelog") || !reader.namespaceUri().isEmpty()) return {};
				root = true;
			}
			if (reader.name() == QLatin1String("neo-metadata")) {
				if (found || path != QStringList({"divelog", "settings"}) || !reader.namespaceUri().isEmpty() || !reader.attributes().isEmpty()) return {};
				found = true; const auto text = reader.readElementText(QXmlStreamReader::ErrorOnUnexpectedElement).toUtf8();
				const auto body = parse(text); if (reader.hasError() || !body) return {};
				metadata = QJsonDocument(*body).toJson(QJsonDocument::Compact);
			} else path.append(reader.namespaceUri().isEmpty() ? reader.qualifiedName().toString() : "{" + reader.namespaceUri().toString() + "}" + reader.name().toString());
		} else if (reader.isEndElement()) {
			if (path.isEmpty()) return {};
			path.removeLast();
		}
	}
	return !reader.hasError() && root && path.isEmpty() ? std::optional<QByteArray>(metadata) : std::nullopt;
}
std::optional<QByteArray> NeoLogMetadata::captureProfile(QSettings &settings, const QByteArray &preserved)
{
	QJsonObject body{{"schema", 1}, {"format", "neo-log-metadata-1"}, {"equipmentKits", QJsonObject()},
		{"collections", QJsonArray()}, {"plannerPresets", QJsonArray()}};
	if (!preserved.isEmpty()) { const auto previous = parse(preserved); if (!previous) return {}; body = *previous; }
	settings.sync(); if (settings.status() != QSettings::NoError) return {};
	for (const auto &key : {QStringLiteral("equipment-kits"), QStringLiteral("dive-collections")}) {
		const auto name = "subsurface-neo/" + key; if (!settings.contains(name)) continue;
		const auto bytes = settings.value(name).toByteArray(); if (bytes.isEmpty() || bytes.size() > Limit) return {};
		QJsonParseError error; const auto value = QJsonDocument::fromJson(bytes, &error);
		if (error.error != QJsonParseError::NoError || (key == "equipment-kits" ? !value.isObject() : !value.isArray())) return {};
		body[key == "equipment-kits" ? "equipmentKits" : "collections"] = key == "equipment-kits" ? QJsonValue(value.object()) : QJsonValue(value.array());
	}
	const auto presets = QStringLiteral("subsurface-neo/planner/presets");
	if (settings.contains(presets)) { const auto value = QJsonValue::fromVariant(settings.value(presets)); if (!value.isArray()) return {}; body["plannerPresets"] = value; }
	const auto bytes = QJsonDocument(body).toJson(QJsonDocument::Compact); return parse(bytes) ? std::optional<QByteArray>(bytes) : std::nullopt;
}
bool NeoLogMetadata::projectProfile(QSettings &settings, const QByteArray &bytes)
{
	const auto body = parse(bytes); if (!body) return false;
	settings.sync(); if (settings.status() != QSettings::NoError) return false;
	settings.setValue("subsurface-neo/equipment-kits", QJsonDocument(body->value("equipmentKits").toObject()).toJson(QJsonDocument::Compact));
	settings.setValue("subsurface-neo/dive-collections", QJsonDocument(body->value("collections").toArray()).toJson(QJsonDocument::Compact));
	settings.setValue("subsurface-neo/planner/presets", body->value("plannerPresets").toVariant());
	settings.sync(); if (settings.status() != QSettings::NoError) return false;
	const auto observed = captureProfile(settings, bytes); return observed && *observed == QJsonDocument(*body).toJson(QJsonDocument::Compact);
}
