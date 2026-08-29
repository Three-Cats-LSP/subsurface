// SPDX-License-Identifier: GPL-2.0
#ifndef NEO_WEB_PLANNER_MODEL_H
#define NEO_WEB_PLANNER_MODEL_H

#include <QObject>
#include <QVariantList>

class NeoWebPlannerModel : public QObject {
	Q_OBJECT
	Q_PROPERTY(double depthMeters READ depthMeters WRITE setDepthMeters NOTIFY changed)
	Q_PROPERTY(int bottomTimeMinutes READ bottomTimeMinutes WRITE setBottomTimeMinutes NOTIFY changed)
	Q_PROPERTY(double ascentRate READ ascentRate WRITE setAscentRate NOTIFY changed)
	Q_PROPERTY(QString gas READ gas WRITE setGas NOTIFY changed)
	Q_PROPERTY(bool valid READ valid NOTIFY changed)
	Q_PROPERTY(QString summary READ summary NOTIFY changed)
	Q_PROPERTY(QString warning READ warning NOTIFY changed)
	Q_PROPERTY(QVariantList waypoints READ waypoints NOTIFY changed)

public:
	explicit NeoWebPlannerModel(QObject *parent = nullptr);
	double depthMeters() const;
	int bottomTimeMinutes() const;
	double ascentRate() const;
	QString gas() const;
	bool valid() const;
	QString summary() const;
	QString warning() const;
	QVariantList waypoints() const;
	void setDepthMeters(double value);
	void setBottomTimeMinutes(int value);
	void setAscentRate(double value);
	void setGas(const QString &value);
	Q_INVOKABLE void reset();

signals:
	void changed();

private:
	double m_depthMeters = 18.0;
	int m_bottomTimeMinutes = 40;
	double m_ascentRate = 9.0;
	QString m_gas = QStringLiteral("Air");
	bool m_valid = true;
	QString m_summary;
	QString m_warning;
	QVariantList m_waypoints;
	void rebuild();
};

#endif
