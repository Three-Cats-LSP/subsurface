// SPDX-License-Identifier: GPL-2.0
#include "neowebplannermodel.h"

#include <QtMath>

NeoWebPlannerModel::NeoWebPlannerModel(QObject *parent) : QObject(parent) { rebuild(); }
double NeoWebPlannerModel::depthMeters() const { return m_depthMeters; }
int NeoWebPlannerModel::bottomTimeMinutes() const { return m_bottomTimeMinutes; }
double NeoWebPlannerModel::ascentRate() const { return m_ascentRate; }
QString NeoWebPlannerModel::gas() const { return m_gas; }
double NeoWebPlannerModel::sacRate() const { return m_sacRate; }
double NeoWebPlannerModel::cylinderVolume() const { return m_cylinderVolume; }
int NeoWebPlannerModel::startPressure() const { return m_startPressure; }
int NeoWebPlannerModel::reservePressure() const { return m_reservePressure; }
bool NeoWebPlannerModel::safetyStop() const { return m_safetyStop; }
bool NeoWebPlannerModel::valid() const { return m_valid; }
bool NeoWebPlannerModel::gasAdequate() const { return m_gasAdequate; }
QString NeoWebPlannerModel::summary() const { return m_summary; }
QString NeoWebPlannerModel::gasSummary() const { return m_gasSummary; }
QString NeoWebPlannerModel::warning() const { return m_warning; }
double NeoWebPlannerModel::estimatedGasLiters() const { return m_estimatedGasLiters; }
double NeoWebPlannerModel::availableGasLiters() const { return m_availableGasLiters; }
QVariantList NeoWebPlannerModel::waypoints() const { return m_waypoints; }

void NeoWebPlannerModel::setDepthMeters(double value) { if (qFuzzyCompare(m_depthMeters, value)) return; m_depthMeters = value; rebuild(); emit changed(); }
void NeoWebPlannerModel::setBottomTimeMinutes(int value) { if (m_bottomTimeMinutes == value) return; m_bottomTimeMinutes = value; rebuild(); emit changed(); }
void NeoWebPlannerModel::setAscentRate(double value) { if (qFuzzyCompare(m_ascentRate, value)) return; m_ascentRate = value; rebuild(); emit changed(); }
void NeoWebPlannerModel::setGas(const QString &value) { const QString normalized = value.trimmed(); if (m_gas == normalized) return; m_gas = normalized; rebuild(); emit changed(); }
void NeoWebPlannerModel::setSacRate(double value) { if (qFuzzyCompare(m_sacRate, value)) return; m_sacRate = value; rebuild(); emit changed(); }
void NeoWebPlannerModel::setCylinderVolume(double value) { if (qFuzzyCompare(m_cylinderVolume, value)) return; m_cylinderVolume = value; rebuild(); emit changed(); }
void NeoWebPlannerModel::setStartPressure(int value) { if (m_startPressure == value) return; m_startPressure = value; rebuild(); emit changed(); }
void NeoWebPlannerModel::setReservePressure(int value) { if (m_reservePressure == value) return; m_reservePressure = value; rebuild(); emit changed(); }
void NeoWebPlannerModel::setSafetyStop(bool value) { if (m_safetyStop == value) return; m_safetyStop = value; rebuild(); emit changed(); }

void NeoWebPlannerModel::reset()
{
	m_depthMeters = 18.0;
	m_bottomTimeMinutes = 40;
	m_ascentRate = 9.0;
	m_gas = QStringLiteral("Air");
	m_sacRate = 18.0;
	m_cylinderVolume = 12.0;
	m_startPressure = 200;
	m_reservePressure = 50;
	m_safetyStop = true;
	rebuild();
	emit changed();
}

void NeoWebPlannerModel::rebuild()
{
	m_valid = m_depthMeters >= 1.0 && m_depthMeters <= 150.0 && m_bottomTimeMinutes >= 1 &&
		m_bottomTimeMinutes <= 600 && m_ascentRate >= 1.0 && m_ascentRate <= 30.0 && !m_gas.isEmpty() &&
		m_sacRate >= 5.0 && m_sacRate <= 100.0 && m_cylinderVolume >= 1.0 && m_cylinderVolume <= 40.0 &&
		m_startPressure >= 50 && m_startPressure <= 400 && m_reservePressure >= 0 && m_reservePressure < m_startPressure;
	m_waypoints.clear();
	if (!m_valid) {
		m_summary = tr("Enter valid depth, time, ascent, gas, cylinder, SAC, start-pressure, and reserve values.");
		m_gasSummary.clear();
		m_warning = tr("The draft cannot be evaluated until every value is valid.");
		m_estimatedGasLiters = 0.0;
		m_availableGasLiters = 0.0;
		m_gasAdequate = false;
		return;
	}
	const double descentMinutes = m_depthMeters / 18.0;
	const bool includeSafetyStop = m_safetyStop && m_depthMeters >= 10.0;
	const double stopDepth = 5.0;
	const double firstAscentDistance = includeSafetyStop ? m_depthMeters - stopDepth : m_depthMeters;
	const double firstAscentMinutes = firstAscentDistance / m_ascentRate;
	const double finalAscentMinutes = includeSafetyStop ? stopDepth / m_ascentRate : 0.0;
	const double safetyStopMinutes = includeSafetyStop ? 3.0 : 0.0;
	const double leaveBottomTime = descentMinutes + m_bottomTimeMinutes;
	const double reachStopTime = leaveBottomTime + firstAscentMinutes;
	const double runtimeMinutes = reachStopTime + safetyStopMinutes + finalAscentMinutes;
	m_waypoints = {
		QVariantMap{{QStringLiteral("timeMinutes"), 0.0}, {QStringLiteral("depthMeters"), 0.0}, {QStringLiteral("label"), tr("Surface")}},
		QVariantMap{{QStringLiteral("timeMinutes"), descentMinutes}, {QStringLiteral("depthMeters"), m_depthMeters}, {QStringLiteral("label"), tr("Reach depth")}},
		QVariantMap{{QStringLiteral("timeMinutes"), leaveBottomTime}, {QStringLiteral("depthMeters"), m_depthMeters}, {QStringLiteral("label"), tr("Leave bottom")}}
	};
	if (includeSafetyStop) {
		m_waypoints.push_back(QVariantMap{{QStringLiteral("timeMinutes"), reachStopTime}, {QStringLiteral("depthMeters"), stopDepth}, {QStringLiteral("label"), tr("Safety stop")}});
		m_waypoints.push_back(QVariantMap{{QStringLiteral("timeMinutes"), reachStopTime + safetyStopMinutes}, {QStringLiteral("depthMeters"), stopDepth}, {QStringLiteral("label"), tr("Leave safety stop")}});
	}
	m_waypoints.push_back(QVariantMap{{QStringLiteral("timeMinutes"), runtimeMinutes}, {QStringLiteral("depthMeters"), 0.0}, {QStringLiteral("label"), tr("Surface")}});

	const double bottomPressure = 1.0 + m_depthMeters / 10.0;
	const double descentGas = m_sacRate * descentMinutes * (1.0 + bottomPressure) / 2.0;
	const double bottomGas = m_sacRate * m_bottomTimeMinutes * bottomPressure;
	const double stopPressure = 1.0 + stopDepth / 10.0;
	const double firstAscentEndPressure = includeSafetyStop ? stopPressure : 1.0;
	const double firstAscentGas = m_sacRate * firstAscentMinutes * (bottomPressure + firstAscentEndPressure) / 2.0;
	const double stopGas = m_sacRate * safetyStopMinutes * stopPressure;
	const double finalAscentGas = m_sacRate * finalAscentMinutes * (stopPressure + 1.0) / 2.0;
	m_estimatedGasLiters = descentGas + bottomGas + firstAscentGas + stopGas + finalAscentGas;
	m_availableGasLiters = m_cylinderVolume * (m_startPressure - m_reservePressure);
	m_gasAdequate = m_estimatedGasLiters <= m_availableGasLiters;
	m_summary = tr("%1 m for %2 min on %3; draft runtime %4 min.")
		.arg(m_depthMeters, 0, 'f', 1).arg(m_bottomTimeMinutes).arg(m_gas)
		.arg(qCeil(runtimeMinutes));
	m_gasSummary = tr("Estimated gas %1 L; usable gas above reserve %2 L; margin %3 L.")
		.arg(qRound(m_estimatedGasLiters)).arg(qRound(m_availableGasLiters))
		.arg(qRound(m_availableGasLiters - m_estimatedGasLiters));
	const QString nativeWarning = (m_depthMeters > 30.0 || m_bottomTimeMinutes > 30) ?
		tr("This draft may require decompression. Validate it with Subsurface's native planner before diving.") :
		tr("This is a browser draft, not a decompression schedule. Validate it with the native planner before diving.");
	m_warning = m_gasAdequate ? nativeWarning :
		tr("Estimated gas exceeds usable gas above the selected reserve. Change the plan or cylinder. %1").arg(nativeWarning);
}
