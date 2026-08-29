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
	Q_PROPERTY(double sacRate READ sacRate WRITE setSacRate NOTIFY changed)
	Q_PROPERTY(double cylinderVolume READ cylinderVolume WRITE setCylinderVolume NOTIFY changed)
	Q_PROPERTY(int startPressure READ startPressure WRITE setStartPressure NOTIFY changed)
	Q_PROPERTY(int reservePressure READ reservePressure WRITE setReservePressure NOTIFY changed)
	Q_PROPERTY(bool safetyStop READ safetyStop WRITE setSafetyStop NOTIFY changed)
	Q_PROPERTY(bool valid READ valid NOTIFY changed)
	Q_PROPERTY(bool gasAdequate READ gasAdequate NOTIFY changed)
	Q_PROPERTY(QString summary READ summary NOTIFY changed)
	Q_PROPERTY(QString gasSummary READ gasSummary NOTIFY changed)
	Q_PROPERTY(QString warning READ warning NOTIFY changed)
	Q_PROPERTY(double estimatedGasLiters READ estimatedGasLiters NOTIFY changed)
	Q_PROPERTY(double availableGasLiters READ availableGasLiters NOTIFY changed)
	Q_PROPERTY(QVariantList waypoints READ waypoints NOTIFY changed)

public:
	explicit NeoWebPlannerModel(QObject *parent = nullptr);
	double depthMeters() const;
	int bottomTimeMinutes() const;
	double ascentRate() const;
	QString gas() const;
	double sacRate() const;
	double cylinderVolume() const;
	int startPressure() const;
	int reservePressure() const;
	bool safetyStop() const;
	bool valid() const;
	bool gasAdequate() const;
	QString summary() const;
	QString gasSummary() const;
	QString warning() const;
	double estimatedGasLiters() const;
	double availableGasLiters() const;
	QVariantList waypoints() const;
	void setDepthMeters(double value);
	void setBottomTimeMinutes(int value);
	void setAscentRate(double value);
	void setGas(const QString &value);
	void setSacRate(double value);
	void setCylinderVolume(double value);
	void setStartPressure(int value);
	void setReservePressure(int value);
	void setSafetyStop(bool value);
	Q_INVOKABLE void reset();

signals:
	void changed();

private:
	double m_depthMeters = 18.0;
	int m_bottomTimeMinutes = 40;
	double m_ascentRate = 9.0;
	QString m_gas = QStringLiteral("Air");
	double m_sacRate = 18.0;
	double m_cylinderVolume = 12.0;
	int m_startPressure = 200;
	int m_reservePressure = 50;
	bool m_safetyStop = true;
	bool m_valid = true;
	bool m_gasAdequate = true;
	QString m_summary;
	QString m_gasSummary;
	QString m_warning;
	double m_estimatedGasLiters = 0.0;
	double m_availableGasLiters = 0.0;
	QVariantList m_waypoints;
	void rebuild();
};

#endif
