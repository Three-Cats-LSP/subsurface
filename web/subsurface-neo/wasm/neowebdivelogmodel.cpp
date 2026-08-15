// SPDX-License-Identifier: GPL-2.0
#include "neowebdivelogmodel.h"

#include "core/native-divelog-summary.h"

#include <QFile>
#include <QFileInfo>
#include <QLocale>
#include <QVariantMap>

#include <algorithm>

namespace {

QString durationText(int seconds)
{
	const int minutes = qMax(0, seconds / 60);
	if (minutes >= 60)
		return QStringLiteral("%1 h %2 min").arg(minutes / 60).arg(minutes % 60);
	return QStringLiteral("%1 min").arg(minutes);
}

QString depthText(double meters)
{
	return QStringLiteral("%1 m").arg(QLocale().toString(meters, 'f', 1));
}

QString temperatureText(double celsius)
{
	return QStringLiteral("%1 °C").arg(QLocale().toString(celsius, 'f', 1));
}

QString localPathForUrl(const QUrl &url)
{
	const QString local = url.toLocalFile();
	return local.isEmpty() ? url.path() : local;
}

} // namespace

NeoWebDiveLogModel::NeoWebDiveLogModel(QObject *parent) : QObject(parent)
{
}

int NeoWebDiveLogModel::diveCount() const
{
	return m_diveCount;
}

QString NeoWebDiveLogModel::totalTime() const
{
	const double hours = m_totalSeconds / 3600.0;
	return QStringLiteral("%1 h").arg(QLocale().toString(hours, 'f', 1));
}

QString NeoWebDiveLogModel::maxDepth() const
{
	return m_diveCount > 0 ? depthText(m_maxDepthMeters) : QStringLiteral("—");
}

QVariantList NeoWebDiveLogModel::recentDives() const
{
	return m_recentDives;
}

QString NeoWebDiveLogModel::fileStatus() const
{
	return m_fileStatus;
}

bool NeoWebDiveLogModel::loaded() const
{
	return m_loaded;
}

bool NeoWebDiveLogModel::error() const
{
	return m_error;
}

void NeoWebDiveLogModel::openLocalFile(const QUrl &url)
{
	const QString localPath = localPathForUrl(url);
	QFile file(localPath);
	const QFileInfo info(file);
	if (info.size() > 64 * 1024 * 1024) {
		m_error = true;
		m_loaded = false;
		m_fileStatus = tr("%1 is larger than the 64 MB browser import limit.").arg(info.fileName());
		emit changed();
		return;
	}
	if (!file.open(QIODevice::ReadOnly)) {
		m_error = true;
		m_loaded = false;
		m_fileStatus = tr("The selected file could not be opened.");
		emit changed();
		return;
	}

	const native_divelog_summary summary = read_native_divelog_summary(file, info.fileName());
	if (!summary.ok) {
		m_error = true;
		m_loaded = false;
		m_fileStatus = summary.error;
		emit changed();
		return;
	}

	m_diveCount = summary.dives.size();
	m_totalSeconds = 0;
	m_maxDepthMeters = 0.0;
	m_recentDives.clear();
	for (const native_dive_summary &dive : summary.dives) {
		m_totalSeconds += dive.duration_seconds;
		if (dive.has_max_depth)
			m_maxDepthMeters = std::max(m_maxDepthMeters, dive.max_depth_m);
	}
	for (int index = summary.dives.size() - 1; index >= 0 && m_recentDives.size() < 5; --index) {
		const native_dive_summary &dive = summary.dives.at(index);
		QVariantMap item;
		item.insert(QStringLiteral("number"), dive.number);
		item.insert(QStringLiteral("date"), dive.when.isValid() ?
			QLocale().toString(dive.when.date(), QLocale::ShortFormat) : tr("Date unavailable"));
		item.insert(QStringLiteral("location"), dive.location.isEmpty() ? tr("Unnamed dive site") : dive.location);
		item.insert(QStringLiteral("duration"), durationText(dive.duration_seconds));
		item.insert(QStringLiteral("depth"), dive.has_max_depth ? depthText(dive.max_depth_m) : QStringLiteral("—"));
		item.insert(QStringLiteral("temperature"), dive.has_water_temperature ?
			temperatureText(dive.water_temperature_c) : QStringLiteral("—"));
		item.insert(QStringLiteral("gas"), dive.gas);
		item.insert(QStringLiteral("mode"), dive.mode);
		item.insert(QStringLiteral("gear"), dive.gear);
		item.insert(QStringLiteral("buddy"), dive.buddy);
		item.insert(QStringLiteral("notes"), dive.notes);
		m_recentDives.push_back(item);
	}

	m_error = false;
	m_loaded = true;
	m_fileStatus = tr("Loaded %1 dives from %2 using Subsurface's native XML reader.")
		.arg(m_diveCount)
		.arg(info.fileName());
	emit changed();
}
