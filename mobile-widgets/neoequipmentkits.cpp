// SPDX-License-Identifier: GPL-2.0
#include "neoequipmentkits.h"
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QSettings>

NeoEquipmentKits::NeoEquipmentKits(QObject *parent) : QObject(parent)
{
	load();
}

QVariantList NeoEquipmentKits::kits() const { return m_kits; }
QStringList NeoEquipmentKits::recentKitNames() const { return m_recentKitNames; }
QString NeoEquipmentKits::defaultKit() const { return m_defaultKit; }

QVariantMap NeoEquipmentKits::kit(const QString &name) const
{
	for (const QVariant &item : m_kits) {
		const QVariantMap data = item.toMap();
		if (data.value("name").toString() == name)
			return data;
	}
	return {};
}

void NeoEquipmentKits::useKit(const QString &name)
{
	if (kit(name).isEmpty())
		return;
	m_recentKitNames.removeAll(name);
	m_recentKitNames.prepend(name);
	while (m_recentKitNames.size() > 3)
		m_recentKitNames.removeLast();
	save();
	emit recentKitNamesChanged();
}

void NeoEquipmentKits::saveKit(const QString &name, const QVariantMap &data)
{
	if (name.trimmed().isEmpty())
		return;
	QVariantMap kitData = data;
	kitData["name"] = name.trimmed();
	for (int i = 0; i < m_kits.size(); ++i) {
		if (m_kits[i].toMap().value("name").toString() == kitData.value("name").toString()) {
			m_kits[i] = kitData;
			save();
			emit kitsChanged();
			return;
		}
	}
	m_kits.append(kitData);
	save();
	emit kitsChanged();
}

void NeoEquipmentKits::removeKit(const QString &name)
{
	for (int i = 0; i < m_kits.size(); ++i) {
		if (m_kits[i].toMap().value("name").toString() == name) {
			m_kits.removeAt(i);
			m_recentKitNames.removeAll(name);
			if (m_defaultKit == name)
				setDefaultKit({});
			save();
			emit kitsChanged();
			emit recentKitNamesChanged();
			return;
		}
	}
}

void NeoEquipmentKits::setDefaultKit(const QString &name)
{
	if (m_defaultKit == name)
		return;
	m_defaultKit = name;
	save();
	emit defaultKitChanged();
}

void NeoEquipmentKits::load()
{
	QSettings settings;
	const QJsonDocument document = QJsonDocument::fromJson(settings.value("subsurface-neo/equipment-kits").toByteArray());
	if (document.isObject()) {
		m_kits = document.object().value("kits").toArray().toVariantList();
		m_defaultKit = document.object().value("defaultKit").toString();
		m_recentKitNames = document.object().value("recentKitNames").toVariant().toStringList();
	}
}

void NeoEquipmentKits::save() const
{
	QSettings settings;
	QJsonObject object;
	object["kits"] = QJsonArray::fromVariantList(m_kits);
	object["defaultKit"] = m_defaultKit;
	object["recentKitNames"] = QJsonArray::fromStringList(m_recentKitNames);
	settings.setValue("subsurface-neo/equipment-kits", QJsonDocument(object).toJson(QJsonDocument::Compact));
}
