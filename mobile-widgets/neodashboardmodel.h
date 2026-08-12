// SPDX-License-Identifier: GPL-2.0
#ifndef NEODASHBOARDMODEL_H
#define NEODASHBOARDMODEL_H

#include <QObject>
#include <QString>
#include <QVariantList>

class NeoDashboardModel : public QObject
{
	Q_OBJECT
	Q_PROPERTY(int diveCount READ diveCount NOTIFY changed)
	Q_PROPERTY(int planCount READ planCount NOTIFY changed)
	Q_PROPERTY(QString totalTimeHours READ totalTimeHours NOTIFY changed)
	Q_PROPERTY(QString maxDepth READ maxDepth NOTIFY changed)
	Q_PROPERTY(QString maxDepthUnit READ maxDepthUnit NOTIFY changed)
	Q_PROPERTY(QVariantList recentDives READ recentDives NOTIFY changed)
	Q_PROPERTY(QVariantList recentPlans READ recentPlans NOTIFY changed)

public:
	explicit NeoDashboardModel(QObject *parent = nullptr);

	int diveCount() const { return m_diveCount; }
	int planCount() const { return m_planCount; }
	QString totalTimeHours() const { return m_totalTimeHours; }
	QString maxDepth() const { return m_maxDepth; }
	QString maxDepthUnit() const { return m_maxDepthUnit; }
	QVariantList recentDives() const { return m_recentDives; }
	QVariantList recentPlans() const { return m_recentPlans; }

public slots:
	void refresh();

signals:
	void changed();

private:
	int m_diveCount = 0;
	int m_planCount = 0;
	QString m_totalTimeHours = QStringLiteral("0.0");
	QString m_maxDepth;
	QString m_maxDepthUnit;
	QVariantList m_recentDives;
	QVariantList m_recentPlans;
};

#endif // NEODASHBOARDMODEL_H
