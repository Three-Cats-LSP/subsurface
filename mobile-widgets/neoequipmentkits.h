// SPDX-License-Identifier: GPL-2.0
#ifndef NEOEQUIPMENTKITS_H
#define NEOEQUIPMENTKITS_H

#include <QObject>
#include <QVariantList>
#include <QStringList>

class NeoEquipmentKits : public QObject
{
	Q_OBJECT
	Q_PROPERTY(QVariantList kits READ kits NOTIFY kitsChanged)
	Q_PROPERTY(QVariantList equipmentItems READ equipmentItems NOTIFY equipmentItemsChanged)
	Q_PROPERTY(QStringList recentKitNames READ recentKitNames NOTIFY recentKitNamesChanged)
	Q_PROPERTY(QString defaultKit READ defaultKit WRITE setDefaultKit NOTIFY defaultKitChanged)
public:
	explicit NeoEquipmentKits(QObject *parent = nullptr);
	QVariantList kits() const;
	QVariantList equipmentItems() const;
	QStringList recentKitNames() const;
	QString defaultKit() const;
	Q_INVOKABLE QVariantMap kit(const QString &name) const;
	Q_INVOKABLE void useKit(const QString &name);
	Q_INVOKABLE void saveKit(const QString &name, const QVariantMap &data);
	Q_INVOKABLE void removeKit(const QString &name);
	Q_INVOKABLE void saveEquipmentItem(const QString &name, const QVariantMap &data);
	Q_INVOKABLE void removeEquipmentItem(const QString &name);
	void setDefaultKit(const QString &name);
signals:
	void kitsChanged();
	void equipmentItemsChanged();
	void recentKitNamesChanged();
	void defaultKitChanged();
private:
	QVariantList m_kits;
	QVariantList m_equipmentItems;
	QStringList m_recentKitNames;
	QString m_defaultKit;
	void load();
	void save() const;
};

#endif
