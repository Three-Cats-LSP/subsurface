// SPDX-License-Identifier: GPL-2.0
#include "neodivecollections.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSettings>

NeoDiveCollections::NeoDiveCollections(QObject *parent) : QObject(parent)
{
	load();
}

int NeoDiveCollections::indexOf(const QString &name) const
{
	for (int i = 0; i < m_collections.size(); ++i) {
		if (m_collections[i].toMap().value("name").toString() == name)
			return i;
	}
	return -1;
}

QStringList NeoDiveCollections::names() const
{
	QStringList result;
	for (const QVariant &item : m_collections)
		result.append(item.toMap().value("name").toString());
	return result;
}

QVariantList NeoDiveCollections::diveIds(const QString &name) const
{
	const int index = indexOf(name);
	return index >= 0 ? m_collections[index].toMap().value("dives").toList() : QVariantList{};
}

void NeoDiveCollections::create(const QString &name)
{
	const QString trimmed = name.trimmed();
	if (trimmed.isEmpty() || indexOf(trimmed) >= 0)
		return;
	m_collections.append(QVariantMap{{ "name", trimmed }, { "dives", QVariantList{} }});
	save();
	emit collectionsChanged();
}

void NeoDiveCollections::remove(const QString &name)
{
	const int index = indexOf(name);
	if (index < 0)
		return;
	m_collections.removeAt(index);
	save();
	emit collectionsChanged();
}

void NeoDiveCollections::addDive(const QString &name, int diveId)
{
	const int index = indexOf(name);
	if (index < 0 || diveId <= 0)
		return;
	QVariantMap collection = m_collections[index].toMap();
	QVariantList dives = collection.value("dives").toList();
	if (!dives.contains(diveId)) {
		dives.append(diveId);
		collection["dives"] = dives;
		m_collections[index] = collection;
		save();
		emit collectionsChanged();
	}
}

void NeoDiveCollections::removeDive(const QString &name, int diveId)
{
	const int index = indexOf(name);
	if (index < 0)
		return;
	QVariantMap collection = m_collections[index].toMap();
	QVariantList dives = collection.value("dives").toList();
	if (dives.removeAll(diveId) > 0) {
		collection["dives"] = dives;
		m_collections[index] = collection;
		save();
		emit collectionsChanged();
	}
}

void NeoDiveCollections::load()
{
	QSettings settings;
	const QJsonDocument document = QJsonDocument::fromJson(settings.value("subsurface-neo/dive-collections").toByteArray());
	if (document.isArray())
		m_collections = document.array().toVariantList();
}

void NeoDiveCollections::save() const
{
	QSettings settings;
	settings.setValue("subsurface-neo/dive-collections", QJsonDocument(QJsonArray::fromVariantList(m_collections)).toJson(QJsonDocument::Compact));
}
