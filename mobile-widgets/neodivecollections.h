// SPDX-License-Identifier: GPL-2.0
#ifndef NEODIVECOLLECTIONS_H
#define NEODIVECOLLECTIONS_H

#include <QObject>
#include <QStringList>
#include <QVariantList>

class NeoDiveCollections : public QObject
{
	Q_OBJECT
	Q_PROPERTY(QStringList names READ names NOTIFY collectionsChanged)
public:
	explicit NeoDiveCollections(QObject *parent = nullptr);
	QStringList names() const;
	Q_INVOKABLE QVariantList diveIds(const QString &name) const;
	Q_INVOKABLE void create(const QString &name);
	Q_INVOKABLE void remove(const QString &name);
	Q_INVOKABLE void addDive(const QString &name, int diveId);
	Q_INVOKABLE void removeDive(const QString &name, int diveId);
signals:
	void collectionsChanged();
private:
	QVariantList m_collections;
	void load();
	void save() const;
	int indexOf(const QString &name) const;
};

#endif
