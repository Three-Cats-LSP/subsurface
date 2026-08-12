// SPDX-License-Identifier: GPL-2.
#include "qmlprofile.h"
#include "profilescene.h"
#include "mobile-widgets/qmlmanager.h"
#include "core/divelist.h"
#include "core/errorhelper.h"
#include "core/subsurface-float.h"
#include "core/metrics.h"
#include "core/profile.h"
#include "core/sample.h"
#include "core/subsurface-string.h"
#include "core/units.h"
#include <QTransform>
#include <QScreen>
#include <QElapsedTimer>
#include <algorithm>
#include <cmath>

QMLProfile::QMLProfile(QQuickItem *parent) :
	QQuickPaintedItem(parent),
	m_diveId(0),
	m_dc(0),
	m_devicePixelRatio(1.0),
	m_margin(0),
	m_xOffset(0.0),
	m_yOffset(0.0)
{
	createProfileView();
	setAntialiasing(true);
	setFlags(QQuickItem::ItemClipsChildrenToShape | QQuickItem::ItemHasContents );
	connect(QMLManager::instance(), &QMLManager::sendScreenChanged, this, &QMLProfile::screenChanged);
	connect(this, &QMLProfile::scaleChanged, this, &QMLProfile::triggerUpdate);
	connect(&diveListNotifier, &DiveListNotifier::divesChanged, this, &QMLProfile::divesChanged);
	setDevicePixelRatio(QMLManager::instance()->lastDevicePixelRatio());
}

QMLProfile::~QMLProfile()
{
}

void QMLProfile::createProfileView()
{
	m_profileWidget.reset(new ProfileScene(m_devicePixelRatio * 0.8, false, false));
}

void QMLProfile::rebuildInspectorPlot()
{
	m_inspectorPlot.reset();
	const struct dive *d = divelog.dives.get_by_uniq_id(m_diveId);
	const divecomputer *dc = currentDiveComputer();
	if (!d || !dc)
		return;
	m_inspectorPlot = std::make_unique<plot_info>(create_plot_info_new(d, dc, nullptr));
}

// we need this so we can connect update() to the scaleChanged() signal - which the connect above cannot do
// directly as it chokes on the default parameter for update().
// If the scale changes we may need to change our offsets to ensure that we still only show a subset of
// the profile and not empty space around it, which the paint() method below will take care of, which will
// eventually get called after we call update()
void QMLProfile::triggerUpdate()
{
	update();
}

void QMLProfile::paint(QPainter *painter)
{
	QElapsedTimer timer;
	if (verbose)
		timer.start();

	// let's look at the intended size of the content and scale our scene accordingly
	// for some odd reason the painter transformation is set up to scale by the dpr - which results
	// in applying that dpr scaling twice. So we hard-code it here to be the identity matrix
	QRect painterRect = painter->viewport();
	painter->resetTransform();
	if (m_diveId < 0)
		return;
	struct dive *d = divelog.dives.get_by_uniq_id(m_diveId);
	if (!d)
		return;

	// Apply pan offset for zoomed profile.
	// QML scale is applied at the Item level, so paint() still renders into
	// the original rect. Translating the painter shifts which portion of
	// the profile is visible in the scaled view.
	if (!nearly_0(m_xOffset) || !nearly_0(m_yOffset))
		painter->translate(m_xOffset, m_yOffset);

	m_profileWidget->draw(painter, painterRect, d, m_dc, nullptr, false);
}

void QMLProfile::setMargin(int margin)
{
	m_margin = margin;
}

int QMLProfile::diveId() const
{
	return m_diveId;
}

void QMLProfile::setDiveId(int diveId)
{
	if (m_diveId == diveId)
		return;
	m_diveId = diveId;
	m_dc = 0;
	rebuildInspectorPlot();
	emit numDCChanged();
	emit currentDCChanged();
	triggerUpdate();
}

const divecomputer *QMLProfile::currentDiveComputer() const
{
	const struct dive *d = divelog.dives.get_by_uniq_id(m_diveId);
	if (!d || m_dc < 0 || m_dc >= d->number_of_computers())
		return nullptr;
	return &d->dcs[m_dc];
}

int QMLProfile::currentDC() const
{
	return m_dc;
}

QString QMLProfile::computerName() const
{
	const divecomputer *dc = currentDiveComputer();
	return dc ? QString::fromStdString(dc->model) : QString();
}

QString QMLProfile::computerSerial() const
{
	const divecomputer *dc = currentDiveComputer();
	return dc ? QString::fromStdString(dc->serial) : QString();
}

QString QMLProfile::diveMode() const
{
	const divecomputer *dc = currentDiveComputer();
	if (!dc)
		return QString();

	switch (dc->divemode) {
	case OC:
		return tr("Open circuit");
	case CCR:
		return tr("CCR");
	case PSCR:
		return tr("PSCR");
	case FREEDIVE:
		return tr("Freedive");
	default:
		return tr("Unknown");
	}
}

QVariantMap QMLProfile::sampleAtFraction(qreal fraction) const
{
	QVariantMap result;
	const divecomputer *dc = currentDiveComputer();
	if (!dc || dc->samples.empty())
		return result;

	fraction = std::clamp(fraction, qreal(0.0), qreal(1.0));
	const int lastTime = dc->samples.back().time.seconds;
	const int targetTime = static_cast<int>(std::lround(lastTime * fraction));
	auto it = std::lower_bound(dc->samples.begin(), dc->samples.end(), targetTime,
		[](const sample &s, int time) { return s.time.seconds < time; });
	if (it == dc->samples.end())
		it = std::prev(dc->samples.end());
	else if (it != dc->samples.begin()) {
		auto previous = std::prev(it);
		if (targetTime - previous->time.seconds <= it->time.seconds - targetTime)
			it = previous;
	}

	const sample &s = *it;
	const int totalSeconds = std::max(0, s.time.seconds);
	result["timeSeconds"] = totalSeconds;
	result["time"] = QStringLiteral("%1:%2")
		.arg(totalSeconds / 60)
		.arg(totalSeconds % 60, 2, 10, QLatin1Char('0'));

	int depthDecimals = 0;
	const char *depthUnit = nullptr;
	const double depth = get_depth_units(s.depth, &depthDecimals, &depthUnit);
	result["depth"] = QStringLiteral("%1 %2")
		.arg(depth, 0, 'f', depthDecimals)
		.arg(QString::fromUtf8(depthUnit ? depthUnit : ""));

	if (s.temperature.mkelvin) {
		const char *tempUnit = nullptr;
		const double temp = get_temp_units(s.temperature.mkelvin, &tempUnit);
		result["temperature"] = QStringLiteral("%1 %2")
			.arg(temp, 0, 'f', 1)
			.arg(QString::fromUtf8(tempUnit ? tempUnit : ""));
	}

	if (s.ndl.seconds >= 0) {
		result["ndlSeconds"] = s.ndl.seconds;
		result["ndl"] = QStringLiteral("%1 min").arg(s.ndl.seconds / 60);
	}
	result["inDeco"] = s.in_deco;
	if (s.in_deco && s.stopdepth.mm > 0) {
		int stopDecimals = 0;
		const char *stopUnit = nullptr;
		const double stopDepth = get_depth_units(s.stopdepth, &stopDecimals, &stopUnit);
		result["decoStop"] = QStringLiteral("%1 %2 · %3 min")
			.arg(stopDepth, 0, 'f', stopDecimals)
			.arg(QString::fromUtf8(stopUnit ? stopUnit : ""))
			.arg(std::max(0, s.stoptime.seconds) / 60);
	}
	if (s.tts.seconds > 0)
		result["tts"] = QStringLiteral("%1 min").arg(s.tts.seconds / 60);

	for (const pressure_t &pressure : s.pressure) {
		if (pressure.mbar > 0) {
			const char *pressureUnit = nullptr;
			const int value = get_pressure_units(pressure.mbar, &pressureUnit);
			result["pressure"] = QStringLiteral("%1 %2")
				.arg(value)
				.arg(QString::fromUtf8(pressureUnit ? pressureUnit : ""));
			break;
		}
	}
	if (s.setpoint.mbar > 0)
		result["setpoint"] = QStringLiteral("%1 bar").arg(s.setpoint.mbar / 1000.0, 0, 'f', 2);
	if (s.cns > 0)
		result["cns"] = QStringLiteral("%1%").arg(s.cns);

	if (m_inspectorPlot && !m_inspectorPlot->entry.empty()) {
		const auto &entries = m_inspectorPlot->entry;
		auto plotIt = std::lower_bound(entries.begin(), entries.end(), totalSeconds,
			[](const plot_data &entry, int time) { return entry.sec < time; });
		if (plotIt == entries.end())
			plotIt = std::prev(entries.end());
		else if (plotIt != entries.begin()) {
			auto previous = std::prev(plotIt);
			if (totalSeconds - previous->sec <= plotIt->sec - totalSeconds)
				plotIt = previous;
		}

		const plot_data &plot = *plotIt;
		if (plot.current_gf > 0.0) {
			const int gf = static_cast<int>(std::lround(plot.current_gf * 100.0));
			result["gfPercent"] = gf;
			result["gf"] = QStringLiteral("%1%").arg(gf);
		}
		if (plot.surface_gf > 0.0) {
			const int surfaceGf = static_cast<int>(std::lround(plot.surface_gf));
			result["surfaceGfPercent"] = surfaceGf;
			result["surfaceGf"] = QStringLiteral("%1%").arg(surfaceGf);
		}
		if (plot.ceiling.mm > 0) {
			int ceilingDecimals = 0;
			const char *ceilingUnit = nullptr;
			const double ceiling = get_depth_units(plot.ceiling, &ceilingDecimals, &ceilingUnit);
			result["calculatedCeiling"] = QStringLiteral("%1 %2")
				.arg(ceiling, 0, 'f', ceilingDecimals)
				.arg(QString::fromUtf8(ceilingUnit ? ceilingUnit : ""));
		}
		if (plot.ndl_calc > 0)
			result["calculatedNdl"] = QStringLiteral("%1 min").arg(plot.ndl_calc / 60);
		if (plot.tts_calc > 0)
			result["calculatedTts"] = QStringLiteral("%1 min").arg(plot.tts_calc / 60);
	}

	result["fraction"] = lastTime > 0 ? qreal(s.time.seconds) / qreal(lastTime) : qreal(0.0);
	return result;
}

qreal QMLProfile::devicePixelRatio() const
{
	return m_devicePixelRatio;
}

void QMLProfile::setDevicePixelRatio(qreal dpr)
{
	if (dpr != m_devicePixelRatio) {
		m_devicePixelRatio = dpr;
		// Recreate the view to redraw the text items with the new scale.
		createProfileView();
		emit devicePixelRatioChanged();
	}
}

// don't update the profile here, have the user update x and y and then manually trigger an update
void QMLProfile::setXOffset(qreal value)
{
	if (nearly_equal(value, m_xOffset))
		return;
	m_xOffset = value;
	emit xOffsetChanged();
}

// don't update the profile here, have the user update x and y and then manually trigger an update
void QMLProfile::setYOffset(qreal value)
{
	if (nearly_equal(value, m_yOffset))
		return;
	m_yOffset = value;
	emit yOffsetChanged();
}

void QMLProfile::screenChanged(QScreen *screen)
{
	setDevicePixelRatio(screen->devicePixelRatio());
}

void QMLProfile::divesChanged(const QVector<dive *> &dives, DiveField)
{
	for (struct dive *d: dives) {
		if (d->id == m_diveId) {
			report_info("dive #%d changed, trigger profile update", d->number);
			if (m_dc >= d->number_of_computers())
				m_dc = 0;
			rebuildInspectorPlot();
			emit numDCChanged();
			emit currentDCChanged();
			triggerUpdate();
			return;
		}
	}
}

void QMLProfile::nextDC()
{
	rotateDC(1);
}

void QMLProfile::prevDC()
{
	rotateDC(-1);
}

void QMLProfile::rotateDC(int dir)
{
	struct dive *d = divelog.dives.get_by_uniq_id(m_diveId);
	if (!d)
		return;
	int numDC = d->number_of_computers();
	if (numDC <= 1)
		return;
	m_dc = (m_dc + dir) % numDC;
	if (m_dc < 0)
		m_dc += numDC;
	rebuildInspectorPlot();
	emit currentDCChanged();
	triggerUpdate();
}

int QMLProfile::numDC() const
{
	struct dive *d = divelog.dives.get_by_uniq_id(m_diveId);
	return d ? d->number_of_computers() : 0;
}
