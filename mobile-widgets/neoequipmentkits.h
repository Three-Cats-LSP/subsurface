// SPDX-License-Identifier: GPL-2.0
#ifndef NEOEQUIPMENTKITS_H
#define NEOEQUIPMENTKITS_H

#include <QObject>
#include <QVariantList>

class NeoEquipmentKits : public QObject
{
	Q_OBJECT
	Q_PROPERTY(QVariantList kits READ kits NOTIFY kitsChanged)
	Q_PROPERTY(QString defaultKit READ defaultKit WRITE setDefaultKit NOTIFY defaultKitChanged)
public:
	explicit NeoEquipmentKits(QObject *parent = nullptr);
	QVariantList kits() const;
	QString defaultKit() const;
	Q_INVOKABLE QVariantMap kit(const QString &name) const;
	Q_INVOKABLE void saveKit(const QString &name, const QVariantMap &data);
	Q_INVOKABLE void removeKit(const QString &name);
	void setDefaultKit(const QString &name);
signals:
	void kitsChanged();
	void defaultKitChanged();
private:
	QVariantList m_kits;
	QString m_defaultKit;
	void load();
	void save() const;
};

#endif
