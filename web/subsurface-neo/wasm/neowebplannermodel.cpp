// SPDX-License-Identifier: GPL-2.0
#include "neowebplannermodel.h"

#include <QtMath>

NeoWebPlannerModel::NeoWebPlannerModel(QObject *parent) : QObject(parent) { rebuild(); }
double NeoWebPlannerModel::depthMeters() const { return m_depthMeters; }
int NeoWebPlannerModel::bottomTimeMinutes() const { return m_bottomTimeMinutes; }
double NeoWebPlannerModel::ascentRate() const { return m_ascentRate; }
QString NeoWebPlannerModel::gas() const { return m_gas; }
bool NeoWebPlannerModel::valid() const { return m_valid; }
QString NeoWebPlannerModel::summary() const { return m_summary; }
QString NeoWebPlannerModel::warning() const { return m_warning; }
QVariantList NeoWebPlannerModel::waypoints() const { return m_waypoints; }

void NeoWebPlannerModel::setDepthMeters(double value) { if (qFuzzyCompare(m_depthMeters, value)) return; m_depthMeters = value; rebuild(); emit changed(); }
void NeoWebPlannerModel::setBottomTimeMinutes(int value) { if (m_bottomTimeMinutes == value) return; m_bottomTimeMinutes = value; rebuild(); emit changed(); }
void NeoWebPlannerModel::setAscentRate(double value) { if (qFuzzyCompare(m_ascentRate, value)) return; m_ascentRate = value; rebuild(); emit changed(); }
void NeoWebPlannerModel::setGas(const QString &value) { const QString normalized = value.trimmed(); if (m_gas == normalized) return; m_gas = normalized; rebuild(); emit changed(); }

void NeoWebPlannerModel::reset()
{
	m_depthMeters = 18.0;
	m_bottomTimeMinutes = 40;
	m_ascentRate = 9.0;
	m_gas = QStringLiteral("Air");
	rebuild();
	emit changed();
}

void NeoWebPlannerModel::rebuild()
{
	m_valid = m_depthMeters >= 1.0 && m_depthMeters <= 150.0 && m_bottomTimeMinutes >= 1 &&
		m_bottomTimeMinutes <= 600 && m_ascentRate >= 1.0 && m_ascentRate <= 30.0 && !m_gas.isEmpty();
	m_waypoints.clear();
	if (!m_valid) {
		m_summary = tr("Enter a depth from 1–150 m, a duration from 1–600 min, an ascent rate from 1–30 m/min, and a gas.");
		m_warning = tr("The draft cannot be evaluated until every value is valid.");
		return;
	}
	const double descentMinutes = m_depthMeters / 18.0;
	const double ascentMinutes = m_depthMeters / m_ascentRate;
	m_waypoints = {
		QVariantMap{{QStringLiteral("timeMinutes"), 0.0}, {QStringLiteral("depthMeters"), 0.0}, {QStringLiteral("label"), tr("Surface")}},
		QVariantMap{{QStringLiteral("timeMinutes"), descentMinutes}, {QStringLiteral("depthMeters"), m_depthMeters}, {QStringLiteral("label"), tr("Reach depth")}},
		QVariantMap{{QStringLiteral("timeMinutes"), descentMinutes + m_bottomTimeMinutes}, {QStringLiteral("depthMeters"), m_depthMeters}, {QStringLiteral("label"), tr("Leave bottom")}},
		QVariantMap{{QStringLiteral("timeMinutes"), descentMinutes + m_bottomTimeMinutes + ascentMinutes}, {QStringLiteral("depthMeters"), 0.0}, {QStringLiteral("label"), tr("Surface")}}
	};
	m_summary = tr("%1 m for %2 min on %3; draft runtime %4 min.")
		.arg(m_depthMeters, 0, 'f', 1).arg(m_bottomTimeMinutes).arg(m_gas)
		.arg(qCeil(descentMinutes + m_bottomTimeMinutes + ascentMinutes));
	m_warning = (m_depthMeters > 30.0 || m_bottomTimeMinutes > 30) ?
		tr("This draft may require decompression. Validate it with Subsurface's native planner before diving.") :
		tr("This is a browser draft, not a decompression schedule. Validate it with the native planner before diving.");
}
