// SPDX-License-Identifier: GPL-2.0
#include "neodashboardmodel.h"

#include "core/dive.h"
#include "core/divelog.h"
#include "core/qthelper.h"
#include "core/string-format.h"
#include "core/subsurface-qt/divelistnotifier.h"

#include <QVariantMap>

NeoDashboardModel::NeoDashboardModel(QObject *parent) : QObject(parent)
{
	connect(&diveListNotifier, &DiveListNotifier::dataReset, this, &NeoDashboardModel::refresh);
	connect(&diveListNotifier, &DiveListNotifier::settingsChanged, this, &NeoDashboardModel::refresh);
	connect(&diveListNotifier, &DiveListNotifier::divesImported, this, &NeoDashboardModel::refresh);
	connect(&diveListNotifier, &DiveListNotifier::divesAdded, this, [this]() { refresh(); });
	connect(&diveListNotifier, &DiveListNotifier::divesDeleted, this, [this]() { refresh(); });
	connect(&diveListNotifier, &DiveListNotifier::divesMovedBetweenTrips, this, [this]() { refresh(); });
	connect(&diveListNotifier, &DiveListNotifier::divesChanged, this, [this]() { refresh(); });
	connect(&diveListNotifier, &DiveListNotifier::divesTimeChanged, this, [this]() { refresh(); });
	refresh();
}

void NeoDashboardModel::refresh()
{
	int count = 0;
	int plans = 0;
	qint64 totalSeconds = 0;
	depth_t deepest {};
	QVector<const dive *> validDives;
	QVector<const dive *> savedPlans;

	for (const auto &divePtr : divelog.dives) {
		if (!divePtr || divePtr->invalid)
			continue;
		const dive *d = divePtr.get();
		if (d->is_planned()) {
			++plans;
			savedPlans.push_back(d);
			continue;
		}
		++count;
		totalSeconds += d->duration.seconds;
		if (d->maxdepth.mm > deepest.mm)
			deepest = d->maxdepth;
		validDives.push_back(d);
	}

	QVariantList recent;
	for (int i = validDives.size() - 1; i >= 0 && recent.size() < 3; --i) {
		const dive *d = validDives.at(i);
		QVariantMap item;
		item.insert(QStringLiteral("id"), d->id);
		item.insert(QStringLiteral("number"), d->number);
		item.insert(QStringLiteral("date"), formatDiveDate(d));
		item.insert(QStringLiteral("location"), QString::fromStdString(d->get_location()));
		item.insert(QStringLiteral("depth"), get_depth_string(d->maxdepth, true, true));
		item.insert(QStringLiteral("duration"), formatDiveDuration(d));
		if (d->watertemp.mkelvin)
			item.insert(QStringLiteral("waterTemp"), get_temperature_string(d->watertemp, true));
		recent.push_back(item);
	}
	QVariantList recentPlans;
	for (int i = savedPlans.size() - 1; i >= 0 && recentPlans.size() < 3; --i) {
		const dive *d = savedPlans.at(i);
		QVariantMap item;
		item.insert(QStringLiteral("id"), d->id);
		item.insert(QStringLiteral("date"), formatDiveDate(d));
		item.insert(QStringLiteral("depth"), get_depth_string(d->maxdepth, true, true));
		item.insert(QStringLiteral("duration"), formatDiveDuration(d));
		recentPlans.push_back(item);
	}

	m_diveCount = count;
	m_planCount = plans;
	m_totalTimeHours = QString::number(totalSeconds / 3600.0, 'f', 1);
	m_maxDepth = count > 0 ? get_depth_string(deepest, false, true) : QString();
	m_maxDepthUnit = get_depth_unit();
	m_recentDives = recent;
	m_recentPlans = recentPlans;
	emit changed();
}
