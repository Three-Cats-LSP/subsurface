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
	Q_PROPERTY(QString totalTimeHours READ totalTimeHours NOTIFY changed)
	Q_PROPERTY(QString maxDepth READ maxDepth NOTIFY changed)
	Q_PROPERTY(QString maxDepthUnit READ maxDepthUnit NOTIFY changed)
	Q_PROPERTY(QVariantList recentDives READ recentDives NOTIFY changed)

public:
	explicit NeoDashboardModel(QObject *parent = nullptr);

	int diveCount() const { return m_diveCount; }
	QString totalTimeHours() const { return m_totalTimeHours; }
	QString maxDepth() const { return m_maxDepth; }
	QString maxDepthUnit() const { return m_maxDepthUnit; }
	QVariantList recentDives() const { return m_recentDives; }

public slots:
	void refresh();

signals:
	void changed();

private:
	int m_diveCount = 0;
	QString m_totalTimeHours = QStringLiteral("0.0");
	QString m_maxDepth;
	QString m_maxDepthUnit;
	QVariantList m_recentDives;
};

#endif // NEODASHBOARDMODEL_H
