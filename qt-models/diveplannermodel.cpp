// SPDX-License-Identifier: GPL-2.0
#include "diveplannermodel.h"
#include "core/color.h"
#include "core/dive.h"
#include "core/divelist.h"
#include "core/divelog.h"
#include "core/event.h"
#include "core/format.h"
#include "core/subsurface-string.h"
#include "qt-models/cylindermodel.h"
#include "core/metrics.h" // For defaultModelFont().
#include "core/planner.h"
#include "core/profile.h"
#include "core/device.h"
#include "core/qthelper.h"
#include "core/range.h"
#include "core/sample.h"
#include "core/selection.h"
#include "core/subsurface-time.h"
#include "core/string-format.h"
#include "core/settings/qPrefDivePlanner.h"
#include "core/settings/qPrefTechnicalDetails.h"
#include "core/settings/qPrefUnit.h"
#include "commands/command.h"
#include "core/gettextfromc.h"
#include "core/deco.h"
#include "core/gas.h"
#include <QApplication>
#include <QTextDocument>
#include <QtConcurrent>
#include <QVariantMap>
#include <cstdlib>
#include <limits>

#define VARIATIONS_IN_BACKGROUND 1

static double unit_factor()
{
	return prefs.units.length == units::METERS ? 1000.0 / 60.0
						   : feet_to_mm(1.0) / 60.0;
}

static constexpr int decotimestep = 60; // seconds

static QString neoPlannerGasLabel(const gasmix &mix)
{
	const int oxygen = (get_o2(mix) + 5) / 10;
	const int helium = (get_he(mix) + 5) / 10;
	if (oxygen == 21 && helium == 0)
		return QStringLiteral("Air");
	if (oxygen == 100 && helium == 0)
		return QStringLiteral("100%");
	return QStringLiteral("%1/%2").arg(oxygen).arg(helium);
}

static cylinder_t *real_cylinder_or_null(struct dive *d, int cylinderId)
{
	return d && cylinderId >= 0 && static_cast<size_t>(cylinderId) < d->cylinders.size() ? d->get_cylinder(cylinderId) : nullptr;
}

static bool is_surface_air_cylinder(const struct dive *d, int cylinderId)
{
	return d && cylinderId >= 0 && static_cast<size_t>(cylinderId) == d->cylinders.size();
}

CylindersModel *DivePlannerPointsModel::cylindersModel()
{
	return &cylinders;
}

void DivePlannerPointsModel::removePoints(const std::vector<int> &rows)
{
	if (rows.empty())
		return;
	std::vector<int> v2 = rows;
	std::sort(v2.begin(), v2.end());

	for (int i = (int)v2.size() - 1; i >= 0; i--) {
		beginRemoveRows(QModelIndex(), v2[i], v2[i]);
		divepoints.erase(divepoints.begin() + v2[i]);
		endRemoveRows();
	}
}

void DivePlannerPointsModel::removeSelectedPoints(const std::vector<int> &rows)
{
	removePoints(rows);

	emitDataChanged();
	cylinders.updateTrashIcon();
}

void DivePlannerPointsModel::createSimpleDive(struct dive *dIn)
{
	// clean out the dive and give it an id and the correct dc model
	d = dIn;
	dcNr = 0;
	d->clear();
	d->id = dive_getUniqID();
	d->when = QDateTime::currentMSecsSinceEpoch() / 1000L + gettimezoneoffset() + 3600;
	make_planner_dc(&d->dcs[0]);

	clear();
	removeDeco();
	setupCylinders();
	setupStartTime();

	// initialize the start time in the plan
	diveplan.when = dateTimeToTimestamp(startTime);
	d->when = diveplan.when;

	// Use gas from the first cylinder
	int cylinderid = 0;

	// If we're in drop_stone_mode, don't add a first point.
	// It will be added implicitly.
	if (!prefs.drop_stone_mode)
		addStop(m_or_ft(15, 45), 1 * 60, cylinderid, prefs.defaultsetpoint, true, UNDEF_COMP_TYPE);

	addStop(m_or_ft(15, 45), 20 * 60, 0, prefs.defaultsetpoint, true, UNDEF_COMP_TYPE);
	if (!isPlanner()) {
		addStop(m_or_ft(5, 15), 42 * 60, cylinderid, prefs.defaultsetpoint, true, UNDEF_COMP_TYPE);
		addStop(m_or_ft(5, 15), 45 * 60, cylinderid, prefs.defaultsetpoint, true, UNDEF_COMP_TYPE);
	}
	updateDiveProfile();
}

void DivePlannerPointsModel::setupStartTime()
{
	// if the latest dive is in the future, then start an hour after it ends
	// otherwise start an hour from now
	startTime = QDateTime::currentDateTimeUtc().addSecs(3600 + gettimezoneoffset());
	if (!divelog.dives.empty()) {
		time_t ends = divelog.dives.back()->endtime();
		time_t diff = ends - dateTimeToTimestamp(startTime);
		if (diff > 0)
			startTime = startTime.addSecs(diff + 3600);
	}
}

void DivePlannerPointsModel::loadFromDive(dive *dIn, int dcNrIn)
{
	d = dIn;
	dcNr = dcNrIn;

	depth_t depthsum;
	int samplecount = 0;
	o2pressure_t last_sp;
	struct divecomputer *dc = d->get_dc(dcNr);
	cylinders.updateDive(d, dcNr);
	duration_t lasttime;
	duration_t lastrecordedtime;
	duration_t newtime;

	clear();
	removeDeco();
	diveplan.dp.clear();

	diveplan.when = d->when;
	// is this a "new" dive where we marked manually entered samples?
	// if yes then the first sample should be marked
	// if it is we only add the manually entered samples as waypoints to the diveplan
	// otherwise we have to add all of them

	bool hasMarkedSamples = false;

	if (!dc->samples.empty())
		hasMarkedSamples = dc->samples[0].manually_entered;
	else
		fake_dc(dc);

	// if this dive has more than 100 samples (so it is probably a logged dive),
	// average samples so we end up with a total of 100 samples.
	int plansamples = std::min(static_cast<int>(dc->samples.size()), 100);
	int j = 0;
	int cylinderid = 0;

	gasmix_loop loop_gas(*d, *dc);
	divemode_loop loop_mode(*dc);
	for (int i = 0; i < plansamples - 1; i++) {
		if (dc->last_manual_time.seconds && dc->last_manual_time.seconds > 120 && lasttime.seconds >= dc->last_manual_time.seconds)
			break;
		while (j * plansamples <= i * static_cast<int>(dc->samples.size())) {
			const sample &s = dc->samples[j];
			if (s.time.seconds != 0 && (!hasMarkedSamples || s.manually_entered)) {
				depthsum += s.depth;
				if (j > 0)
					last_sp = dc->samples[j-1].setpoint;
				++samplecount;
				newtime = s.time;
			}
			j++;
		}
		if (samplecount) {
			cylinderid = get_cylinderid_at_time(d, dc, lasttime);
			duration_t nexttime = newtime;
			++nexttime.seconds;
			if (newtime.seconds - lastrecordedtime.seconds > 10 || cylinderid == get_cylinderid_at_time(d, dc, nexttime)) {
				if (newtime.seconds == lastrecordedtime.seconds)
					newtime.seconds += 10;

				[[maybe_unused]] auto [current_divemode, _cylinder_index, _gasmix] = get_dive_status_at(*d, *dc, newtime.seconds - 1, &loop_mode, &loop_gas);
				addStop(depthsum / samplecount, newtime.seconds, cylinderid, last_sp.mbar, true, current_divemode);
				lastrecordedtime = newtime;
			}
			lasttime = newtime;
			depthsum = 0_m;
			samplecount = 0;
		}
	}
	// make sure we get the last point right so the duration is correct
	[[maybe_unused]] auto [current_divemode, _cylinder_index, _gasmix] = get_dive_status_at(*d, *dc, dc->duration.seconds, &loop_mode, &loop_gas);
	if (!hasMarkedSamples && !dc->last_manual_time.seconds)
		addStop(0_m, dc->duration.seconds, cylinderid, last_sp.mbar, true, current_divemode);
	preserved_until = d->duration;

	emitDataChanged();
}

// copy the tanks from the current dive, or the default cylinder
// or an unknown cylinder
// setup the cylinder widget accordingly
void DivePlannerPointsModel::setupCylinders()
{
	d->cylinders.clear();
	if (mode == PLAN && current_dive) {
		// take the displayed cylinders from the selected dive as starting point
		copy_used_cylinders(current_dive, d, !prefs.include_unused_tanks);
		reset_cylinders(d, true);

		if (!d->cylinders.empty()) {
			cylinders.updateDive(d, dcNr);
			return;		// We have at least one cylinder
		}
	}

	add_default_cylinder(d);
	cylinders.updateDive(d, dcNr);
}

// Update the dive's maximum depth.  Returns true if max. depth changed
bool DivePlannerPointsModel::updateMaxDepth()
{
	int prevMaxDepth = d->maxdepth.mm;
	d->maxdepth = 0_m;
	for (int i = 0; i < rowCount(); i++) {
		divedatapoint p = at(i);
		if (p.depth.mm > d->maxdepth.mm)
			d->maxdepth.mm = p.depth.mm;
	}
	return d->maxdepth.mm != prevMaxDepth;
}

void DivePlannerPointsModel::removeDeco()
{
	std::vector<int> computedPoints;
	for (int i = 0; i < rowCount(); i++) {
		if (!at(i).entered)
			computedPoints.push_back(i);
	}
	removePoints(computedPoints);
}

void DivePlannerPointsModel::addCylinder_clicked()
{
	cylinders.add();
}

void DivePlannerPointsModel::setPlanMode(Mode m)
{
	mode = m;
	// the planner may reset our GF settings that are used to show deco
	// reset them to what's in the preferences
	if (m != PLAN) {
		set_gf(prefs.gflow, prefs.gfhigh);
		set_vpmb_conservatism(prefs.vpmb_conservatism);
	}
}

void DivePlannerPointsModel::resetPlanState()
{
	setPlanSaveAllowed(true);
	setPlanMode(NOTHING);
	clear();
	diveplan.dp.clear();
	d = nullptr;
	dcNr = 0;
}

bool DivePlannerPointsModel::isPlanner() const
{
	return mode == PLAN;
}

bool DivePlannerPointsModel::planSaveAllowed() const
{
	return saveAllowed;
}

void DivePlannerPointsModel::setPlanSaveAllowed(bool allowed)
{
	if (saveAllowed == allowed)
		return;
	saveAllowed = allowed;
	emit planSaveAllowedChanged(allowed);
}

int DivePlannerPointsModel::columnCount(const QModelIndex&) const
{
	return COLUMNS; // to disable CCSETPOINT subtract one
}

static divemode_t get_local_divemode(struct dive *d, int dcNr, int cylinderId, divemode_t selectedDivemode)
{
	divemode_t divemode;
	switch (d->get_dc(dcNr)->divemode) {
	case OC:
	default:
		divemode = OC;

		break;
	case CCR:
		if (cylinder_t *cyl = real_cylinder_or_null(d, cylinderId))
			divemode = cyl->cylinder_use == DILUENT ? CCR : OC;
		else
			divemode = OC;
		if (divemode == OC && prefs.allowOcGasAsDiluent && selectedDivemode == CCR)
			divemode = CCR;

		break;
	case PSCR:
		divemode = selectedDivemode == PSCR ? PSCR : OC;

		break;
	}

	return divemode;
}

QVariant DivePlannerPointsModel::data(const QModelIndex &index, int role) const
{
	if (!d || mode == NOTHING)
		return QVariant();

	if (!index.isValid() || index.row() < 0 || index.row() >= divepoints.count())
		return QVariant();

	const divedatapoint p = divepoints.at(index.row());
	cylinder_t *cyl_for_check = real_cylinder_or_null(d, p.cylinderid);
	bool isInappropriateCylinder = cyl_for_check ? !is_cylinder_use_appropriate(*d->get_dc(dcNr), *cyl_for_check, false)
						     : !is_surface_air_cylinder(d, p.cylinderid);
	divemode_t divemode = get_local_divemode(d, dcNr, p.cylinderid, p.divemode);
	if (role == Qt::DisplayRole || role == Qt::EditRole) {

		switch (index.column()) {
		case CCSETPOINT:
			return (divemode == CCR) ? (double)(p.setpoint / 1000.0) : QVariant();
		case DEPTH:
			return int_cast<int>(get_depth_units(p.depth, NULL, NULL));
		case RUNTIME:
			return p.time / 60;
		case DURATION:
			if (index.row())
				return (p.time - divepoints.at(index.row() - 1).time) / 60;
			else
				return p.time / 60;
		case DIVEMODE:
			return gettextFromC::tr(divemode_text_ui[divemode]);
		case GAS:
			if (cyl_for_check)
				return get_dive_gas(d, dcNr, p.cylinderid);
			else if (is_surface_air_cylinder(d, p.cylinderid))
				return get_gas_string(gasmix_air);
			else
				return QVariant();
		}
	} else if (role == Qt::DecorationRole) {
		switch (index.column()) {
		case REMOVE:
			if (rowCount() > 1)
				return p.entered ? trashIcon() : QVariant();
			else
				return trashForbiddenIcon();
		}
	} else if (role == Qt::SizeHintRole) {
		switch (index.column()) {
		case REMOVE:
			if (rowCount() > 1)
				return p.entered ? trashIcon().size() : QVariant();
			else
				return trashForbiddenIcon().size();
		}
	} else if (role == Qt::FontRole) {
		QFont font = defaultModelFont();

		font.setBold(!p.entered);

		font.setItalic(isInappropriateCylinder);

		return font;
	} else if (role == Qt::BackgroundRole) {
		switch (index.column()) {
		case GAS:
			if (isInappropriateCylinder)
				return REDORANGE1_HIGH_TRANS;

			break;
		case CCSETPOINT:
			if (divemode != CCR)
				return MED_GRAY_HIGH_TRANS;

			break;
		}
	}

	return QVariant();
}

bool DivePlannerPointsModel::setData(const QModelIndex &index, const QVariant &value, int role)
{
	int i, shift;
	if (role == Qt::EditRole) {
		divedatapoint &p = divepoints[index.row()];
		switch (index.column()) {
		case DEPTH: {
			int depth = value.toInt();
			if (depth >= 0) {
				p.depth = units_to_depth(depth);
				if (updateMaxDepth())
					cylinders.updateBestMixes();
			}
			break;
		}
		case RUNTIME: {
			int secs = value.toInt() * 60;
			i = index.row();
			int duration = secs;
			if (i)
				duration -= divepoints[i-1].time;
			// Make sure segments have a minimal duration
			if (duration <= 0)
				secs += 10 - duration;
			p.time = secs;
			while (++i < divepoints.size())
				if (divepoints[i].time < divepoints[i - 1].time + 10)
					divepoints[i].time = divepoints[i - 1].time + 10;
			break;
		}
		case DURATION: {
			int secs = value.toInt() * 60;
			if (secs < 0)
				secs = 10;
			i = index.row();
			if (i)
				shift = divepoints[i].time - divepoints[i - 1].time - secs;
			else
				shift = divepoints[i].time - secs;
			while (i < divepoints.size())
				divepoints[i++].time -= shift;
			break;
		}
		case CCSETPOINT: {
			bool ok;
			int po2 = static_cast<int>(round(value.toFloat(&ok) * 100) * 10);

			if (ok)
				p.setpoint = std::max(po2, 160);

			break;
		}
		case GAS:
			if (value.toInt() >= 0)
				p.cylinderid = value.toInt();
			/* Did we change the start (dp 0) cylinder to another cylinderid than 0? */
			if (value.toInt() > 0 && index.row() == 0)
				cylinders.moveAtFirst(value.toInt());
			cylinders.updateTrashIcon();
			break;
		case DIVEMODE:
			if (value.toInt() < FREEDIVE) {
				p.divemode = (enum divemode_t) value.toInt();
			}
			break;
		}
		editStop(index.row(), p);
	}
	return QAbstractItemModel::setData(index, value, role);
}

void DivePlannerPointsModel::gasChange(const QModelIndex &index, int newcylinderid)
{
	int i = index.row(), oldcylinderid = divepoints[i].cylinderid;
	while (i < rowCount() && oldcylinderid == divepoints[i].cylinderid)
		divepoints[i++].cylinderid = newcylinderid;
	emitDataChanged();
}

void DivePlannerPointsModel::cylinderRenumber(int mapping[])
{
	for (int i = 0; i < rowCount(); i++) {
		if (mapping[divepoints[i].cylinderid] >= 0)
			divepoints[i].cylinderid = mapping[divepoints[i].cylinderid];
	}
	emitDataChanged();
}

QVariant DivePlannerPointsModel::headerData(int section, Qt::Orientation orientation, int role) const
{
	if (role == Qt::DisplayRole && orientation == Qt::Horizontal) {
		switch (section) {
		case DEPTH:
			return tr("Final depth");
		case RUNTIME:
			return tr("Run time");
		case DURATION:
			return tr("Duration");
		case GAS:
			return tr("Used gas");
		case CCSETPOINT:
			return tr("Setpoint");
		case DIVEMODE:
			return tr("Dive mode");
		}
	} else if (role == Qt::FontRole) {
		return defaultModelFont();
	}
	return QVariant();
}

Qt::ItemFlags DivePlannerPointsModel::flags(const QModelIndex &index) const
{
	if (!index.isValid())
		return QAbstractItemModel::flags(index);
	if (!d || mode == NOTHING)
		return QAbstractItemModel::flags(index);
	if (index.row() < 0 || index.row() >= divepoints.count())
		return QAbstractItemModel::flags(index);

	if (index.column() == REMOVE)
		return Qt::ItemIsEnabled;

	const divedatapoint p = divepoints.at(index.row());
	switch (index.column()) {
	case REMOVE:
		return QAbstractItemModel::flags(index);
	case CCSETPOINT:
		if (get_local_divemode(d, dcNr, p.cylinderid, p.divemode) != CCR)
			return QAbstractItemModel::flags(index) & ~Qt::ItemIsEditable & ~Qt::ItemIsEnabled;

		break;
	case DIVEMODE:
		{
			cylinder_t *cyl = real_cylinder_or_null(d, p.cylinderid);
			if (!((d->get_dc(dcNr)->divemode == CCR && prefs.allowOcGasAsDiluent && cyl && is_oc(*cyl)) || d->get_dc(dcNr)->divemode == PSCR))
				return QAbstractItemModel::flags(index) & ~Qt::ItemIsEditable & ~Qt::ItemIsEnabled;
		}
		break;
	}

	return QAbstractItemModel::flags(index) | Qt::ItemIsEditable;
}

int DivePlannerPointsModel::rowCount(const QModelIndex&) const
{
	return divepoints.count();
}

DivePlannerPointsModel::DivePlannerPointsModel(QObject *parent) : QAbstractTableModel(parent),
	d(nullptr),
	cylinders(true),
	mode(NOTHING),
	saveAllowed(true)
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 9, 0)
	startTime = QDateTime(startTime.date(), startTime.time(), QTimeZone(QTimeZone::UTC));
#else
	startTime = QDateTime(startTime.date(), startTime.time(), Qt::UTC);
#endif
	// use a Qt-connection to send the variations text across thread boundary (in case we
	// are calculating the variations in a background thread).
	connect(this, &DivePlannerPointsModel::variationsComputed, this, &DivePlannerPointsModel::computeVariationsDone);
}

DivePlannerPointsModel *DivePlannerPointsModel::instance()
{
	static DivePlannerPointsModel self;
	return &self;
}

void DivePlannerPointsModel::emitDataChanged()
{
	// add a reasonable setpoint for CCR segments that don't have one yet
	if (d && !divepoints.isEmpty()) {
		int rows = rowCount();
		for (int j = 0; j < rows && j < static_cast<int>(divepoints.size()); j++) {
			divedatapoint &p = divepoints[j];
			cylinder_t *cyl = real_cylinder_or_null(d, p.cylinderid);
			if (!cyl)
				continue;
			if (cyl->cylinder_use == DILUENT && p.setpoint == 0)
				p.setpoint = prefs.defaultsetpoint;
		}
	}

	updateDiveProfile();
	int numRows = rowCount();
	if (numRows > 0) {
		emit dataChanged(createIndex(0, 0), createIndex(numRows - 1, COLUMNS - 1));
	}
}

void DivePlannerPointsModel::setBottomSac(double sac)
{
// mobile delivers the same value as desktop when using
// units:METERS
// however when using units:CUFT mobile deliver 0-300 which
// are really 0.00 - 3.00 so start be correcting that
#ifdef SUBSURFACE_MOBILE
	if (qPrefUnits::volume() == units::CUFT)
		sac /= 100; // cuft without decimals (0 - 300)
#endif
	diveplan.bottomsac = units_to_sac(sac);
	qPrefDivePlanner::set_bottomsac(diveplan.bottomsac);
	emitDataChanged();
}

void DivePlannerPointsModel::setDecoSac(double sac)
{
// mobile delivers the same value as desktop when using
// units:METERS
// however when using units:CUFT mobile deliver 0-300 which
// are really 0.00 - 3.00 so start be correcting that
#ifdef SUBSURFACE_MOBILE
	if (qPrefUnits::volume() == units::CUFT)
		sac /= 100; // cuft without decimals (0 - 300)
#endif
	diveplan.decosac = units_to_sac(sac);
	qPrefDivePlanner::set_decosac(diveplan.decosac);
	emitDataChanged();
}

void DivePlannerPointsModel::setSacFactor(double factor)
{
// sacfactor is normal x.y (one decimal), however mobile
// delivers 0 - 100 so adjust that to 0.0 - 10.0, to have
// the same value as desktop
#ifdef SUBSURFACE_MOBILE
	factor /= 10.0;
#endif
	qPrefDivePlanner::set_sacfactor((int) round(factor * 100));
	emitDataChanged();
}

void DivePlannerPointsModel::setProblemSolvingTime(int minutes)
{
	qPrefDivePlanner::set_problemsolvingtime(minutes);
	emitDataChanged();
}

void DivePlannerPointsModel::setGFHigh(const int gfhigh)
{
	if (diveplan.gfhigh != gfhigh) {
		diveplan.gfhigh = gfhigh;
		emitDataChanged();
	}
}

int DivePlannerPointsModel::gfHigh() const
{
	return diveplan.gfhigh;
}

void DivePlannerPointsModel::setGFLow(const int gflow)
{
	if (diveplan.gflow != gflow) {
		diveplan.gflow = gflow;
		emitDataChanged();
	}
}

int DivePlannerPointsModel::gfLow() const
{
	return diveplan.gflow;
}

void DivePlannerPointsModel::cylindersChanged()
{
	if (!d)
		return;

	cylinders.updateDive(d, dcNr);

	emitDataChanged();
	cylinders.emitDataChanged();
}

void DivePlannerPointsModel::setVpmbConservatism(int level)
{
	if (diveplan.vpmb_conservatism != level) {
		diveplan.vpmb_conservatism = level;
		emitDataChanged();
	}
}

void DivePlannerPointsModel::setSurfacePressure(pressure_t pressure)
{
	diveplan.surface_pressure = pressure;
	emitDataChanged();
}

void DivePlannerPointsModel::setSalinity(int salinity)
{
	diveplan.salinity = salinity;
	emitDataChanged();
}

pressure_t DivePlannerPointsModel::getSurfacePressure() const
{
	return diveplan.surface_pressure;
}

void DivePlannerPointsModel::setLastStop6m(bool value)
{
	qPrefDivePlanner::set_last_stop(value);
	emitDataChanged();
}

void DivePlannerPointsModel::setAscrate75Display(int rate)
{
	qPrefDivePlanner::set_ascrate75(lrint(rate * unit_factor()));
	emitDataChanged();
}
int DivePlannerPointsModel::ascrate75Display() const
{
	return lrint((float)prefs.ascrate75 / unit_factor());
}

void DivePlannerPointsModel::setAscrate50Display(int rate)
{
	qPrefDivePlanner::set_ascrate50(lrint(rate * unit_factor()));
	emitDataChanged();
}
int DivePlannerPointsModel::ascrate50Display() const
{
	return lrint((float)prefs.ascrate50 / unit_factor());
}

void DivePlannerPointsModel::setAscratestopsDisplay(int rate)
{
	qPrefDivePlanner::set_ascratestops(lrint(rate * unit_factor()));
	emitDataChanged();
}

int DivePlannerPointsModel::ascratestopsDisplay() const
{
	return lrint((float)prefs.ascratestops / unit_factor());
}

void DivePlannerPointsModel::setAscratelast6mDisplay(int rate)
{
	qPrefDivePlanner::set_ascratelast6m(lrint(rate * unit_factor()));
	emitDataChanged();
}
int DivePlannerPointsModel::ascratelast6mDisplay() const
{
	return lrint((float)prefs.ascratelast6m / unit_factor());
}

void DivePlannerPointsModel::setDescrateDisplay(int rate)
{
	qPrefDivePlanner::set_descrate(lrint(rate * unit_factor()));
	emitDataChanged();
}
int DivePlannerPointsModel::descrateDisplay() const
{
	return lrint((float)prefs.descrate / unit_factor());
}

void DivePlannerPointsModel::setVerbatim(bool value)
{
	qPrefDivePlanner::set_verbatim_plan(value);
	emitDataChanged();
}

void DivePlannerPointsModel::setDisplayRuntime(bool value)
{
	qPrefDivePlanner::set_display_runtime(value);
	emitDataChanged();
}

void DivePlannerPointsModel::setDisplayDuration(bool value)
{
	qPrefDivePlanner::set_display_duration(value);
	emitDataChanged();
}

void DivePlannerPointsModel::setDisplayTransitions(bool value)
{
	qPrefDivePlanner::set_display_transitions(value);
	emitDataChanged();
}

void DivePlannerPointsModel::setDisplayVariations(bool value)
{
	qPrefDivePlanner::set_display_variations(value);
	emitDataChanged();
}

void DivePlannerPointsModel::setDecoMode(int mode)
{
	qPrefDivePlanner::set_planner_deco_mode(deco_mode(mode));
	emit recreationChanged(mode == int(prefs.planner_deco_mode));
	emitDataChanged();
}

void DivePlannerPointsModel::setSafetyStop(bool value)
{
	qPrefDivePlanner::set_safetystop(value);
	emitDataChanged();
}

void DivePlannerPointsModel::setReserveGas(int reserve)
{
	if (prefs.units.pressure == units::BAR)
		qPrefDivePlanner::set_reserve_gas(reserve * 1000);
	else
		qPrefDivePlanner::set_reserve_gas(psi_to_mbar(reserve));
	emitDataChanged();
}

void DivePlannerPointsModel::setDropStoneMode(bool value)
{
	qPrefDivePlanner::set_drop_stone_mode(value);
	if (divepoints.isEmpty())
		return;
	if (prefs.drop_stone_mode) {
	/* Remove the first entry if we enable drop_stone_mode */
		if (rowCount() >= 2) {
			beginRemoveRows(QModelIndex(), 0, 0);
			divepoints.remove(0);
			endRemoveRows();
		}
	} else {
		/* Add a first entry if we disable drop_stone_mode */
		beginInsertRows(QModelIndex(), 0, 0);
		/* Copy the first current point */
		divedatapoint p = divepoints.at(0);
		p.time = p.depth.mm / prefs.descrate;
		divepoints.push_front(p);
		endInsertRows();
	}
	emitDataChanged();
}

void DivePlannerPointsModel::setSwitchAtReqStop(bool value)
{
	qPrefDivePlanner::set_switch_at_req_stop(value);
	emitDataChanged();
}

void DivePlannerPointsModel::setMinSwitchDuration(int duration)
{
	qPrefDivePlanner::set_min_switch_duration(duration * 60);
	emitDataChanged();
}

void DivePlannerPointsModel::setSurfaceSegment(int duration)
{
	qPrefDivePlanner::set_surface_segment(duration * 60);
	emitDataChanged();
}

void DivePlannerPointsModel::setStartDate(const QDate &date)
{
	startTime.setDate(date);
	diveplan.when = dateTimeToTimestamp(startTime);
	d->when = diveplan.when;
	emitDataChanged();
}

void DivePlannerPointsModel::setStartTime(const QTime &t)
{
	startTime.setTime(t);
	diveplan.when = dateTimeToTimestamp(startTime);
	d->when = diveplan.when;
	emitDataChanged();
}

bool divePointsLessThan(const divedatapoint &p1, const divedatapoint &p2)
{
	return p1.time < p2.time;
}

int DivePlannerPointsModel::lastEnteredPoint() const
{
	for (int i = divepoints.count() - 1; i >= 0; i--)
		if (divepoints.at(i).entered)
			return i;
	return -1;
}

void DivePlannerPointsModel::addDefaultStop()
{
	removeDeco();
	addStop(0_m, 0, -1, prefs.defaultsetpoint, true, UNDEF_COMP_TYPE);

	emitDataChanged();
}

void DivePlannerPointsModel::addStop(depth_t depth, int seconds)
{
	removeDeco();
	addStop(depth, seconds, -1, prefs.defaultsetpoint, true, UNDEF_COMP_TYPE);
	emitDataChanged();
}

void DivePlannerPointsModel::addReverseProfile() {
	if (divepoints.size() <= 1)
		return;

	int runtime = divepoints.back().time;

	beginInsertRows(QModelIndex(), divepoints.size(), 2 * divepoints.size() - (prefs.drop_stone_mode ? 1 : 2));
	for (int i = divepoints.count() - 2; i >= 0; --i) {
		divepoints << divepoints[i];
		runtime += divepoints[i+1].time - divepoints[i].time;
		divepoints.back().time = runtime;
	}

	if (prefs.drop_stone_mode) {
		divepoints << divepoints[0];
		divepoints.back().time = runtime + divepoints[0].time - divepoints[0].depth.mm / prefs.descrate;
	}

	endInsertRows();

	emitDataChanged();
}

// cylinderid_in == -1 means same gas as before.
// divemode == UNDEF_COMP_TYPE means determine from previous point.
int DivePlannerPointsModel::addStop(depth_t depth, int seconds, int cylinderid_in, int ccpoint, bool entered, enum divemode_t divemode)
{
	int cylinderid = 0;
	bool usePrevious = false;
	if (cylinderid_in >= 0)
		cylinderid = cylinderid_in;
	else
		usePrevious = true;

	int row = divepoints.count();
	if (seconds == 0 && depth.mm == 0) {
		if (row == 0) {
			depth = m_or_ft(5, 15); // 5m / 15ft
			seconds = 600;			// 10 min
			// Default to the first cylinder
			cylinderid = 0;
		} else {
			/* this is only possible if the user clicked on the 'plus' sign on the DivePoints Table */
			const divedatapoint t = divepoints.at(lastEnteredPoint());
			depth = t.depth;
			seconds = t.time + 600; // 10 minutes.
			cylinderid = t.cylinderid;
			ccpoint = t.setpoint;
		}
	}

	// check if there's already a new stop before this one:
	for (int i = 0; i < row; i++) {
		const divedatapoint &dp = divepoints.at(i);
		if (dp.time == seconds) {
			row = i;
			beginRemoveRows(QModelIndex(), row, row);
			divepoints.remove(row);
			endRemoveRows();
			break;
		}
		if (dp.time > seconds) {
			row = i;
			break;
		}
	}
	// Previous, actually means next as we are typically subdiving a segment and the gas for
	// the segment is determined by the waypoint at the end.
	if (usePrevious) {
		if (row  < divepoints.count()) {
			cylinderid = divepoints.at(row).cylinderid;
			if (divemode == UNDEF_COMP_TYPE)
				divemode = divepoints.at(row).divemode;
			ccpoint = divepoints.at(row).setpoint;
		} else if (row > 0) {
			cylinderid = divepoints.at(row - 1).cylinderid;
			if (divemode == UNDEF_COMP_TYPE)
				divemode = divepoints.at(row - 1).divemode;
			ccpoint = divepoints.at(row -1).setpoint;
		}
	}
	if (divemode == UNDEF_COMP_TYPE)
		divemode = d->get_dc(dcNr)->divemode;

	// add the new stop
	beginInsertRows(QModelIndex(), row, row);
	divedatapoint point(seconds, depth, cylinderid, ccpoint, entered);
	point.divemode = divemode;
	divepoints.insert(divepoints.begin() + row, point);
	endInsertRows();

	return row;
}

void DivePlannerPointsModel::editStop(int row, divedatapoint newData)
{
	if (row < 0 || row >= divepoints.count())
		return;

	// Refuse to move to 0, since that has special meaning.
	if (newData.time <= 0)
		return;

	/*
	 * When moving divepoints rigorously, we might end up with index
	 * out of range, thus returning the last one instead.
	 */
	int old_first_cylid = divepoints[0].cylinderid;

	// Refuse creation of two points with the same time stamp.
	// Note: "time" is moved in the positive direction to avoid
	// time becoming zero or, worse, negative.
	while (std::any_of(divepoints.begin(), divepoints.begin() + row,
			[t = newData.time] (const divedatapoint &data)
			{ return data.time == t; }))
		newData.time += 10;
	while (std::any_of(divepoints.begin() + row + 1, divepoints.end(),
			[t = newData.time] (const divedatapoint &data)
			{ return data.time == t; }))
		newData.time += 10;

	// Is it ok to change data first and then move the rows?
	divepoints[row] = newData;

	// If the time changed, the item might have to be moved. Oh joy.
	int newRow = row;
	while (newRow + 1 < divepoints.count() && divepoints[newRow + 1].time < divepoints[row].time)
		++newRow;
	if (newRow != row) {
		++newRow; // Move one past item with smaller time stamp
	} else {
		// If we didn't move forward, try moving backwards
		while (newRow > 0 && divepoints[newRow - 1].time > divepoints[row].time)
			--newRow;
	}

	if (newRow != row && newRow != row + 1) {
		beginMoveRows(QModelIndex(), row, row, QModelIndex(), newRow);
		move_in_range(divepoints, row, row + 1, newRow);
		endMoveRows();

		// Account for moving the row backwards in the array.
		row = newRow > row ? newRow - 1 : newRow;
	}

	if (updateMaxDepth())
		cylinders.updateBestMixes();
	if (divepoints[0].cylinderid != old_first_cylid)
		cylinders.moveAtFirst(divepoints[0].cylinderid);

	updateDiveProfile();
	emit dataChanged(createIndex(row, 0), createIndex(row, COLUMNS - 1));
}

divedatapoint DivePlannerPointsModel::at(int row) const
{
	/*
	 * When moving divepoints rigorously, we might end up with index
	 * out of range, thus returning the last one instead.
	 */
	if (row >= divepoints.count())
		return divepoints.at(divepoints.count() - 1);
	return divepoints.at(row);
}

void DivePlannerPointsModel::removeControlPressed(const QModelIndex &index)
{
	// Never delete all points.
	int rows = rowCount();
	if (index.column() != REMOVE || index.row() <= 0 || index.row() >= rows)
		return;

	int old_first_cylid = divepoints[0].cylinderid;

	preserved_until.seconds = divepoints.at(index.row()).time;
	beginRemoveRows(QModelIndex(), index.row(), rows - 1);
	divepoints.erase(divepoints.begin() + index.row(), divepoints.end());
	endRemoveRows();

	cylinders.updateTrashIcon();
	if (divepoints[0].cylinderid != old_first_cylid)
		cylinders.moveAtFirst(divepoints[0].cylinderid);

	emitDataChanged();
}

void DivePlannerPointsModel::remove(const QModelIndex &index)
{
/* TODO: this seems so wrong.
 * We can't do this here if we plan to use QML on mobile
 * as mobile has no ControlModifier.
 * The correct thing to do is to create a new method
 * remove method that will pass the first and last index of the
 * removed rows, and remove those in a go.
 */
	if (QApplication::keyboardModifiers() & Qt::ControlModifier)
		return removeControlPressed(index);

	// Refuse deleting the last point.
	int rows = rowCount();
	if (index.column() != REMOVE || index.row() < 0 || index.row() >= rows || rows <= 1)
		return;

	divedatapoint dp = at(index.row());
	if (!dp.entered)
		return;

	int old_first_cylid = divepoints[0].cylinderid;

	if (index.row() == rows)
		preserved_until.seconds = divepoints.at(rows - 1).time;
	beginRemoveRows(QModelIndex(), index.row(), index.row());
	divepoints.remove(index.row());
	endRemoveRows();

	cylinders.updateTrashIcon();
	if (divepoints[0].cylinderid != old_first_cylid)
		cylinders.moveAtFirst(divepoints[0].cylinderid);

	emitDataChanged();
}

struct diveplan &DivePlannerPointsModel::getDiveplan()
{
	return diveplan;
}

void DivePlannerPointsModel::cancelPlan()
{
	/* TODO:
	 * This check shouldn't be here - this is the interface responsability.
	 * as soon as the interface thinks that it could cancel the plan, this should be
	 * called.
	 */

	/*
	if (mode == PLAN && rowCount()) {
		if (QMessageBox::warning(MainWindow::instance(), TITLE_OR_TEXT(tr("Discard the plan?"),
												 tr("You are about to discard your plan.")),
					 QMessageBox::Discard | QMessageBox::Cancel, QMessageBox::Discard) != QMessageBox::Discard) {
			return;
		}
	}
	*/

	resetPlanState();

	emit planCanceled();
}

DivePlannerPointsModel::Mode DivePlannerPointsModel::currentMode() const
{
	return mode;
}

bool DivePlannerPointsModel::tankInUse(int cylinderid) const
{
	for (int j = 0; j < rowCount(); j++) {
		const divedatapoint p = at(j);
		if (p.time == 0) // special entries that hold the available gases
			continue;
		if (!p.entered) // removing deco gases is ok
			continue;
		if (p.cylinderid == cylinderid) // tank is in use
			return true;
	}
	return false;
}

void DivePlannerPointsModel::clear()
{
	cylinders.clear();
	preserved_until = 0_sec;
	beginResetModel();
	divepoints.clear();
	endResetModel();
}

void DivePlannerPointsModel::createTemporaryPlan()
{
	// Get the user-input and calculate the dive info
	diveplan.dp.clear();

	for (auto [i, cyl]: enumerated_range(d->cylinders)) {
		if (cyl.depth.mm && cyl.cylinder_use == OC_GAS)
			plan_add_segment(diveplan, 0, cyl.depth, i, 0, false, OC);
	}

	int lastIndex = -1;
	for (int i = 0; i < rowCount(); i++) {
		const divedatapoint p = at(i);
		divemode_t divemode = get_local_divemode(d, dcNr, p.cylinderid, p.divemode);
		int deltaT = lastIndex != -1 ? p.time - at(lastIndex).time : p.time;
		lastIndex = i;
		if (i == 0 && mode == PLAN && prefs.drop_stone_mode) {
			/* Okay, we add a first segment where we go down to depth */
			plan_add_segment(diveplan, p.depth.mm / prefs.descrate, p.depth, p.cylinderid, p.setpoint, true, divemode);
			deltaT -= p.depth.mm / prefs.descrate;
		}
		if (p.entered)
			plan_add_segment(diveplan, deltaT, p.depth, p.cylinderid, p.setpoint, true, divemode);
	}

#if DEBUG_PLAN
	dump_plan(diveplan);
#endif
}

static bool shouldComputeVariations()
{
	return prefs.display_variations && pref_deco_mode(true) != RECREATIONAL;
}

void DivePlannerPointsModel::updateDiveProfile()
{
	if (!d) {
		setPlanSaveAllowed(true);
		return;
	}

	createTemporaryPlan();
	if (diveplan.is_empty()) {
		setPlanSaveAllowed(true);
		return;
	}

	// For calculating variations, we need a copy of the plan. We have to copy _before_
	// calling plan(), because that adds deco stops.
	bool doComputeVariations = isPlanner() && shouldComputeVariations();
	struct diveplan plan_copy;
	if (doComputeVariations)
		plan_copy = diveplan;

	deco_state_cache cache;
	struct deco_state plan_deco_state;

	// AI-generated (Claude)
	planner_error_t planError = plan(&plan_deco_state, diveplan, d, dcNr, decotimestep, cache, isPlanner(), false, nullptr);
	setPlanSaveAllowed(planError != PLAN_ERROR_RECREATIONAL_EXCEEDS_NDL);
	updateMaxDepth();

	if (doComputeVariations) {
#ifdef VARIATIONS_IN_BACKGROUND
		(void)QtConcurrent::run([this, plan = std::move(plan_copy), deco = plan_deco_state] ()
				       { this->computeVariationsAsync(std::move(plan), deco); });
#else
		computeVariationsAsync(std::move(plan_copy), plan_deco_state);
#endif
		final_deco_state = plan_deco_state;
	}
	emit calculatedPlanNotes(QString::fromStdString(d->notes));

#if DEBUG_PLAN
	save_dive(stderr, *d);
	dump_plan(&diveplan);
#endif
}

void DivePlannerPointsModel::deleteTemporaryPlan()
{
	diveplan.dp.clear();
}

void DivePlannerPointsModel::savePlan()
{
	createPlan(false);
}

void DivePlannerPointsModel::saveDuplicatePlan()
{
	createPlan(true);
}

int DivePlannerPointsModel::analyzeVariations(const std::vector<decostop> &min, const std::vector<decostop> &mid, const std::vector<decostop> &max, const char *unit)
{
	auto sum_time = [](int time, const decostop &ds) { return ds.time + time; };
	int minsum = std::accumulate(min.begin(), min.end(), 0, sum_time);
	int midsum = std::accumulate(mid.begin(), mid.end(), 0, sum_time);
	int maxsum = std::accumulate(max.begin(), max.end(), 0, sum_time);
	int leftsum = midsum - minsum;
	int rightsum = maxsum - midsum;

#ifdef DEBUG_STOPVAR
	printf("Total + %d:%02d/%s +- %d s/%s\n\n", FRACTION_TUPLE((leftsum + rightsum) / 2, 60), unit,
							   (rightsum - leftsum) / 2, unit);
#else
	Q_UNUSED(unit)
#endif
	return (leftsum + rightsum) / 2;
}

// Return reference to second to last element.
// Caller is responsible for checking that there are at least two elements.
template <typename T>
auto &second_to_last(T &v)
{
	return *std::prev(std::prev(v.end()));
}

QString DivePlannerPointsModel::computeVariations(const struct diveplan &original_plan, const struct deco_state &ds_in, int *instance_id)
{
	// nothing to do unless there's an original plan
	if (original_plan.dp.empty())
		return QString();

	struct deco_state ds = ds_in; // Work on a copy of the deco state

	auto dive = std::make_unique<struct dive>();
	copy_dive(d, dive.get());
	deco_state_cache cache, save;
	struct diveplan plan_copy;

	save.cache(&ds);

	duration_t delta_time = 1_min;
	QString time_units = tr("min");
	depth_t delta_depth;
	QString depth_units;

	if (prefs.units.length == units::METERS) {
		delta_depth = 1_m;
		depth_units = tr("m");
	} else {
		delta_depth = 1_ft;
		depth_units = tr("ft");
	}

	plan_copy = original_plan;
	if (plan_copy.dp.size() < 2)
		return QString();
	if (instance_id && *instance_id != instanceCounter)
		return QString();

	std::vector<decostop> original;
	plan(&ds, plan_copy, dive.get(), dcNr, 1, cache, true, false, &original);
	save.restore(&ds, false);

	plan_copy = original_plan;
	second_to_last(plan_copy.dp).depth.mm += delta_depth.mm;
	plan_copy.dp.back().depth.mm += delta_depth.mm;
	if (instance_id && *instance_id != instanceCounter)
		return QString();
	std::vector<decostop> deeper;
	plan(&ds, plan_copy, dive.get(), dcNr, 1, cache, true, false, &deeper);
	save.restore(&ds, false);

	plan_copy = original_plan;
	second_to_last(plan_copy.dp).depth.mm -= delta_depth.mm;
	plan_copy.dp.back().depth.mm -= delta_depth.mm;
	if (instance_id && *instance_id != instanceCounter)
		return QString();
	std::vector<decostop> shallower;
	plan(&ds, plan_copy, dive.get(), dcNr, 1, cache, true, false, &shallower);
	save.restore(&ds, false);

	plan_copy = original_plan;
	plan_copy.dp.back().time += delta_time.seconds;
	if (instance_id && *instance_id != instanceCounter)
		return QString();
	std::vector<decostop> longer;
	plan(&ds, plan_copy, dive.get(), dcNr, 1, cache, true, false, &longer);
	save.restore(&ds, false);

	plan_copy = original_plan;
	plan_copy.dp.back().time -= delta_time.seconds;
	if (instance_id && *instance_id != instanceCounter)
		return QString();
	std::vector<decostop> shorter;
	plan(&ds, plan_copy, dive.get(), dcNr, 1, cache, true, false, &shorter);
	save.restore(&ds, false);

	std::string buf = format_string_std(", %s: %c %d:%02d /%s %c %d:%02d /min", qPrintable(tr("Stop times")),
		SIGNED_FRAC_TRIPLET(analyzeVariations(shallower, original, deeper, qPrintable(depth_units)), 60), qPrintable(depth_units),
		SIGNED_FRAC_TRIPLET(analyzeVariations(shorter, original, longer, qPrintable(time_units)), 60));

	return QString::fromStdString(buf);
}

void DivePlannerPointsModel::computeVariationsAsync(struct diveplan original_plan, struct deco_state ds)
{
	int my_instance = ++instanceCounter;

	QString variations = computeVariations(std::move(original_plan), ds, &my_instance);

	// By using a signal, we can transport the variations to the main thread.
	if (variations != QString())
		emit variationsComputed(variations);

#ifdef DEBUG_STOPVAR
	printf("\n\n");
#endif
}

void DivePlannerPointsModel::computeVariationsDone(QString variations)
{
	QString notes = QString::fromStdString(d->notes);
	notes = notes.replace("VARIATIONS", variations);
	d->notes = notes.toStdString();
	emit calculatedPlanNotes(notes);
}

static void addDive(dive *d, bool autogroup, bool newNumber)
{
	// Create a new dive and clear out the old one.
	auto new_d = std::make_unique<dive>();
	std::swap(*d, *new_d);
	Command::addDive(std::move(new_d), autogroup, newNumber);
}

void DivePlannerPointsModel::createPlan(bool saveAsNew)
{
	// Ok, so, here the diveplan creates a dive
	deco_state_cache cache;
	removeDeco();
	createTemporaryPlan();

	// For calculating variations, we need a copy of the plan. We have to copy _before_
	// calling plan(), because that adds deco stops.
	struct diveplan plan_copy;
	if (shouldComputeVariations())
		plan_copy = diveplan;

	// AI-generated (Claude)
	// Recalculate at the persistence boundary so stale UI state or direct slot
	// invocation cannot save an invalid recreational plan.
	planner_error_t planError = plan(&ds_after_previous_dives, diveplan, d, dcNr, decotimestep, cache, isPlanner(), true, nullptr);
	setPlanSaveAllowed(planError != PLAN_ERROR_RECREATIONAL_EXCEEDS_NDL);
	if (!saveAllowed)
		return;

	if (shouldComputeVariations())
		computeVariationsAsync(std::move(plan_copy), ds_after_previous_dives);

	// Fixup planner notes.
	if (current_dive && d->id == current_dive->id) {
		// Try to identify old planner output and remove only this part
		// Treat user provided text as plain text.
		QTextDocument notesDocument;
		notesDocument.setHtml(QString::fromStdString(current_dive->notes));
		QString oldnotes(notesDocument.toPlainText());
		QString disclaimer = get_planner_disclaimer();
		int disclaimerMid = disclaimer.indexOf("%s");
		QString disclaimerBegin, disclaimerEnd;
		if (disclaimerMid >= 0) {
			disclaimerBegin = disclaimer.left(disclaimerMid);
			disclaimerEnd = disclaimer.mid(disclaimerMid + 2);
		} else {
			disclaimerBegin = std::move(disclaimer);
		}
		int disclaimerPositionStart = oldnotes.indexOf(disclaimerBegin);
		if (disclaimerPositionStart >= 0) {
			if (oldnotes.indexOf(disclaimerEnd, disclaimerPositionStart) >= 0) {
				// We found a disclaimer according to the current locale.
				// Remove the disclaimer and anything after the disclaimer, because
				// that's supposedly the old planner notes.
				oldnotes = oldnotes.left(disclaimerPositionStart);
			}
		}
		// Deal with line breaks
		oldnotes.replace("\n", "<br>");
		oldnotes.append(QString::fromStdString(d->notes));
		d->notes = oldnotes.toStdString();
		// If we save as new create a copy of the dive here
	}

	// Now, add or modify the dive.
	if (!current_dive || d->id != current_dive->id) {
		// we were planning a new dive, not re-planning an existing one
		d->divetrip = nullptr; // Should not be necessary, just in case!
		addDive(d, divelog.autogroup, true);
	} else {
		copy_events_until(current_dive, d, dcNr, preserved_until.seconds);
		if (saveAsNew) {
			// we were planning an old dive and save as a new dive
			d->id = dive_getUniqID(); // Things will break horribly if we create dives with the same id.
			addDive(d, false, false);
		} else {
			// we were planning an old dive and rewrite the plan
			Command::replanDive(d);
		}
	}

	// Clear the model state after finishing with the dive
	// This includes removing and clearing the diveplan, so we don't delete
	// the dive by mistake.
	resetPlanState();

	planCreated(); // This signal will exit the UI from planner state.
}

QVariantMap DivePlannerPointsModel::calculatePlan(const QVariantList &cylindersData, const QVariantList &segmentsData, const QString &date, const QString &time, int diveMode, int waterType, int surfacePressureMbar, bool shouldSave)
{
	if (d) {
		delete d;
	}
	d = new dive();
	dcNr = 0;

	// Clear previous plan data
	diveplan.dp.clear();
	d->cylinders.clear();
	d->notes.clear();

	make_planner_dc(&d->dcs[dcNr]);

	// Set Date, Time, and Dive Mode from parameters
	QString dateTimeString = date + " " + time;
	QDateTime plannedDateTime = QDateTime::fromString(dateTimeString, "yyyy-MM-dd hh:mm:ss");
	if (!plannedDateTime.isValid()) {
		QVariantMap results;
		results["notes"] = tr("Enter a valid planned start date and time.");
		results["exceedsNDL"] = false;
		results["planSaveAllowed"] = false;
		results["otu"] = 0;
		results["schedule"] = QVariantList();
		results["profile"] = QVariantList();
		results["newDiveId"] = -1;
		return results;
	}
#if QT_VERSION >= QT_VERSION_CHECK(6, 9, 0)
	plannedDateTime = QDateTime(plannedDateTime.date(), plannedDateTime.time(), QTimeZone(QTimeZone::UTC));
#else
	plannedDateTime = QDateTime(plannedDateTime.date(), plannedDateTime.time(), Qt::UTC);
#endif
	d->when = static_cast<time_t>(plannedDateTime.toSecsSinceEpoch());
	diveplan.when = d->when;

	d->dcs[dcNr].divemode = static_cast<enum divemode_t>(diveMode);

	// Populate cylinders from QML data
	for (const QVariant &cylData : cylindersData) {
		QVariantMap map = cylData.toMap();
		cylinder_t newCyl;
		std::pair<volume_t, pressure_t> type_info = get_tank_info_data(tank_info_table, map["type"].toString().toStdString());
		volume_t size = type_info.first;
		newCyl.type.size = size;
		if (prefs.units.pressure == units::BAR)
			newCyl.type.workingpressure.mbar = map["pressure"].toInt() * 1000;
		else
			newCyl.type.workingpressure.mbar = psi_to_mbar(map["pressure"].toInt());
		newCyl.type.description = map["type"].toString().toStdString();
		QString mix = map["mix"].toString();
		newCyl.gasmix.o2.permille = parseGasMixO2(mix);
		newCyl.gasmix.he.permille = parseGasMixHE(mix);
		sanitize_gasmix(newCyl.gasmix);

		int useIndex = map["use"].toInt();
		newCyl.cylinder_use = (enum cylinderuse)useIndex;
		d->cylinders.add(d->cylinders.size(), newCyl);
	}
	this->cylinders.updateDive(d, dcNr);
	reset_cylinders(d, true);

	// Add available OC-gases as "time=0" waypoints for the planner engine
	pressure_t deco_po2_limit = { .mbar = qPrefDivePlanner::decopo2() };
	for (size_t i = 0; i < d->cylinders.size(); ++i) {
		const cylinder_t &cyl = d->cylinders[i];
		if (cyl.cylinder_use == OC_GAS) {
			depth_t mod = d->gas_mod(cyl.gasmix, deco_po2_limit, 1_m);
			divedatapoint point(0, mod, i, 0, false); // time=0, depth=MOD, cylinderid=i
			diveplan.dp.push_back(point);
		}
	}
	diveplan.salinity = waterType;

	// Neo rows use the mature planner's waypoint semantics: the entered time is
	// the runtime at the end of that target-depth segment. Pass the same runtime
	// delta that createTemporaryPlan() gives the native engine; it expands the
	// transition using the configured rates. Only drop-stone mode inserts the
	// initial descent explicitly, exactly as the desktop planner does.
	int enteredProfileRuntime = 0;
	bool firstEnteredSegment = true;
	for (const QVariant &segData : segmentsData) {
		QVariantMap map = segData.toMap();
		int cylinderId = map["gas"].toInt();
		divemode_t divemode = get_local_divemode(d, dcNr, cylinderId, static_cast<divemode_t>(map["divemode"].toInt()));
		const depth_t targetDepth = units_to_depth(map["depth"].toInt());
		const int requestedRuntime = std::max(0, map["duration"].toInt()) * 60;
		int segmentDuration = std::max(0, requestedRuntime - enteredProfileRuntime);
		if (firstEnteredSegment && prefs.drop_stone_mode && targetDepth.mm > 0) {
			const int travelDuration = targetDepth.mm / std::max(1, prefs.descrate);
			plan_add_segment(diveplan, travelDuration, targetDepth, cylinderId, map["setpoint"].toInt(), true, divemode);
			segmentDuration = std::max(0, segmentDuration - travelDuration);
		}
		if (segmentDuration > 0)
			plan_add_segment(diveplan, segmentDuration, targetDepth, cylinderId, map["setpoint"].toInt(), true, divemode);
		enteredProfileRuntime = std::max(enteredProfileRuntime, requestedRuntime);
		firstEnteredSegment = false;
	}

	struct diveplan plan_copy = diveplan;

	// Load ALL current settings from the correct preference classes
	diveplan.gflow = gfLow();
	diveplan.gfhigh = gfHigh();
	diveplan.bottomsac = qPrefDivePlanner::bottomsac();
	diveplan.decosac = qPrefDivePlanner::decosac();
	diveplan.surface_pressure = surfacePressureMbar > 0 ? pressure_t { .mbar = surfacePressureMbar } : d->get_surface_pressure();
	diveplan.vpmb_conservatism = qPrefTechnicalDetails::vpmb_conservatism();

	// Run the planner engine
	// AI-generated (Claude)
	planner_error_t planError = PLAN_OK;
	struct deco_state plan_deco_state;
	bool has_planner_deco_state = false;
	std::vector<decostop> decostops;
	if (!diveplan.is_empty()) {
		deco_state_cache cache;
		planError = plan(&plan_deco_state, diveplan, d, dcNr, 60, cache, true, shouldSave, &decostops);
		has_planner_deco_state = true;
		if (shouldComputeVariations()) {
			QString variations = computeVariations(plan_copy, plan_deco_state, nullptr);
			if (!variations.isEmpty()) {
				QString notes = QString::fromStdString(d->notes);
				notes = notes.replace("VARIATIONS", variations);
				d->notes = notes.toStdString();
			}
		}

		updateMaxDepth();
		d->fixup_dive();
	}
	// Planner dive computers are intentionally ignored by the generic dive
	// fixup when deriving the top-level duration.  Neo saves the generated plan
	// directly, so preserve the native planner runtime explicitly for the dive
	// list and details header.
	int runtimeSeconds = 0;
	if (!d->dcs.empty() && !d->dcs[0].samples.empty())
		runtimeSeconds = d->dcs[0].samples.back().time.seconds;
	for (const divedatapoint &point : diveplan.dp)
		runtimeSeconds = std::max(runtimeSeconds, point.time);
	if (!d->dcs.empty())
		d->dcs[0].duration.seconds = runtimeSeconds;
	d->duration.seconds = runtimeSeconds;
	// Planner samples commonly omit computer-reported CNS. Preserve the
	// unrounded result from Subsurface's established oxygen-exposure algorithm
	// so shallow plans below one percent do not appear to have no CNS data.
	QString notes_qstr = QString::fromStdString(d->notes);
	notes_qstr.replace("&#10138;", "&#8593;");
	notes_qstr.replace("&#10136;", "&#8595;");
	notes_qstr.replace("&#10137;", "&#8594;");
	d->notes = notes_qstr.toStdString();
	// Build the results map
	QVariantMap results;
	QTextDocument notesDocument;
	notesDocument.setHtml(QString::fromStdString(d->notes));
	results["notes"] = notesDocument.toPlainText().trimmed();
	results["maxDepth"] = get_depth_string(d->maxdepth, true);
	results["duration"] = QString::number((runtimeSeconds + 30) / 60) + " min";
	results["runtimeSeconds"] = runtimeSeconds;
	results["bottomTimeSeconds"] = enteredProfileRuntime;
	// AI-generated (Claude)
	// Only flag the recreational no-decompression-limit violation here: other
	// planner errors are reported as text in the plan notes, and the mobile UI
	// shows a warning that is specific to the recreational NDL case.
	const bool exceedsNDL = planError == PLAN_ERROR_RECREATIONAL_EXCEEDS_NDL;
	results["exceedsNDL"] = exceedsNDL;
	results["planSaveAllowed"] = planError == PLAN_OK;
	results["otu"] = d->otu;
	QVariantList timeline;
	int previousTime = 0;
	depth_t previousTimelineDepth = 0_m;
	int previousCylinder = -1;
	for (const divedatapoint &point : diveplan.dp) {
		if (point.time <= 0 || point.time < previousTime || point.cylinderid < 0 ||
		    static_cast<size_t>(point.cylinderid) >= d->cylinders.size())
			continue;
		const bool gasSwitch = previousCylinder >= 0 && point.cylinderid != previousCylinder;
		const int segmentDuration = point.time - previousTime;
		if (segmentDuration == 0 && !gasSwitch)
			continue;
		QVariantMap row;
		row.insert("depth", point.depth.mm);
		row.insert("duration", segmentDuration);
		row.insert("runTime", point.time);
		row.insert("gas", neoPlannerGasLabel(d->cylinders[point.cylinderid].gasmix));
		row.insert("gasSwitch", gasSwitch);
		row.insert("setpoint", point.setpoint);
		row.insert("entered", point.entered);
		if (segmentDuration == 0)
			row.insert("phase", QStringLiteral("switch"));
		else if (point.depth.mm > previousTimelineDepth.mm)
			row.insert("phase", QStringLiteral("descent"));
		else if (point.depth.mm < previousTimelineDepth.mm)
			row.insert("phase", QStringLiteral("ascent"));
		else if (point.depth.mm > SURFACE_THRESHOLD && point.time > enteredProfileRuntime)
			// Native stop waypoints can retain the entered flag. Runtime is the
			// reliable boundary between the requested profile and generated ascent.
			row.insert("phase", QStringLiteral("deco"));
		else if (point.entered)
			row.insert("phase", QStringLiteral("level"));
		else
			row.insert("phase", QStringLiteral("surface"));
		timeline.append(row);
		previousTime = point.time;
		previousTimelineDepth = point.depth;
		previousCylinder = point.cylinderid;
	}
	results["timeline"] = timeline;
	QVariantList schedule;
	int totalDecoSeconds = 0;
	for (const decostop &stop : decostops) {
		// The mature planner also reports zero-time depths that it cleared while
		// ascending. They are useful internally, but they are not deco stops.
		if (stop.time <= 0)
			continue;
		QVariantMap row;
		row.insert("depth", stop.depth);
		row.insert("duration", stop.time);
		row.insert("phase", QStringLiteral("deco"));
		totalDecoSeconds += stop.time;
		if (!d->dcs.empty()) {
			const struct sample *matchingSample = nullptr;
			for (const struct sample &sample : d->dcs[0].samples) {
				if (sample.depth.mm != stop.depth)
					continue;
				matchingSample = &sample;
				if (sample.in_deco || sample.stoptime.seconds > 0)
					break;
			}
			if (matchingSample) {
				const int cylinderId = get_cylinderid_at_time(d, &d->dcs[0], matchingSample->time);
				if (cylinderId >= 0 && static_cast<size_t>(cylinderId) < d->cylinders.size())
					row.insert("gas", neoPlannerGasLabel(d->cylinders[cylinderId].gasmix));
				const int runTime = matchingSample->time.seconds;
				row.insert("runTime", runTime);
				// Planner samples do not always carry recorded-computer TTS.  The
				// completed native plan nevertheless gives us the exact remaining
				// time to surface, so do not expose a misleading zero in Neo.
				row.insert("tts", matchingSample->tts.seconds > 0 ? matchingSample->tts.seconds :
						   std::max(0, runtimeSeconds - runTime));
				row.insert("cns", matchingSample->cns);
				row.insert("setpoint", matchingSample->setpoint.mbar);
			}
		}
		schedule.append(row);
	}
	// Plans with explicit deco gases can expose their stops only through the
	// completed native waypoint timeline. Fall back to those dwell rows when
	// the planner's auxiliary stop table is empty.
	if (schedule.empty()) {
		for (const QVariant &timelineValue : timeline) {
			const QVariantMap timelineRow = timelineValue.toMap();
			if (timelineRow.value("phase").toString() != QStringLiteral("deco") ||
			    timelineRow.value("duration").toInt() <= 0)
				continue;
			QVariantMap row = timelineRow;
			totalDecoSeconds += row.value("duration").toInt();
			if (!d->dcs.empty()) {
				const struct sample *matchingSample = nullptr;
				for (const struct sample &sample : d->dcs[0].samples) {
					if (sample.time.seconds < row.value("runTime").toInt())
						continue;
					matchingSample = &sample;
					break;
				}
				if (matchingSample) {
					const int runTime = row.value("runTime").toInt();
					row.insert("tts", matchingSample->tts.seconds > 0 ? matchingSample->tts.seconds :
							   std::max(0, runtimeSeconds - runTime));
					row.insert("cns", matchingSample->cns);
				}
			}
			schedule.append(row);
		}
	}
	// Some air-only plans omit generated ascent waypoints and the auxiliary
	// stop table, while the planner dive computer still contains the complete
	// stepped ascent. Recover actual post-profile dwell segments from those
	// samples so the Neo schedule never loses native stops.
	if (schedule.empty() && !d->dcs.empty()) {
		const std::vector<struct sample> &samples = d->dcs[0].samples;
		for (size_t i = 1; i < samples.size(); ++i) {
			const struct sample &previousSample = samples[i - 1];
			const struct sample &currentSample = samples[i];
			const int duration = currentSample.time.seconds - previousSample.time.seconds;
			if (duration <= 0 || currentSample.time.seconds <= enteredProfileRuntime ||
			    currentSample.depth.mm <= SURFACE_THRESHOLD || currentSample.depth.mm != previousSample.depth.mm)
				continue;
			QVariantMap row;
			row.insert("depth", currentSample.depth.mm);
			row.insert("duration", duration);
			row.insert("runTime", currentSample.time.seconds);
			row.insert("phase", QStringLiteral("deco"));
			const int cylinderId = get_cylinderid_at_time(d, &d->dcs[0], currentSample.time);
			if (cylinderId >= 0 && static_cast<size_t>(cylinderId) < d->cylinders.size())
				row.insert("gas", neoPlannerGasLabel(d->cylinders[cylinderId].gasmix));
			row.insert("tts", currentSample.tts.seconds > 0 ? currentSample.tts.seconds :
					   std::max(0, runtimeSeconds - currentSample.time.seconds));
			row.insert("cns", currentSample.cns);
			row.insert("setpoint", currentSample.setpoint.mbar);
			totalDecoSeconds += duration;
			schedule.append(row);
		}
	}
	results["schedule"] = schedule;
	results["decoTimeSeconds"] = totalDecoSeconds;

	// The planner has already calculated consumption and end pressures while
	// constructing this dive. Expose those results rather than recalculate
	// gas sufficiency in the QML layer.
	QVariantList gasAnalysis;
	for (const cylinder_t &cylinder : d->cylinders) {
		if (cylinder.cylinder_use == NOT_USED)
			continue;
		QVariantMap gas;
		gas["mix"] = neoPlannerGasLabel(cylinder.gasmix);
		gas["used"] = get_volume_string(cylinder.gas_used, true);
		gas["decoUsed"] = get_volume_string(cylinder.deco_gas_used, true);
		gas["startPressure"] = get_pressure_string(cylinder.start, true);
		gas["endPressure"] = get_pressure_string(cylinder.end, true);
		// A cylinder that is exhausted can have a negative calculated end
		// pressure.  Do not feed that sentinel into volume conversion, where it
		// wraps into the enormous positive value previously shown in Neo exports.
		const pressure_t nonNegativeEnd { .mbar = std::max(0, cylinder.end.mbar) };
		gas["remaining"] = get_volume_string(cylinder.gas_volume(nonNegativeEnd), true);
		gas["belowMinimum"] = cylinder.end.mbar <= (10_bar).mbar;
		gas["belowReserve"] = cylinder.end.mbar > (10_bar).mbar && cylinder.end.mbar < qPrefDivePlanner::reserve_gas();
		gasAnalysis.append(gas);
	}
	results["gasAnalysis"] = gasAnalysis;

	QVariantList profileData;
	QVariantMap analysis;
	int closestAnalysisSample = std::numeric_limits<int>::max();
	if (d->dcs.size() > 0) {
		// Project the planner's established tissue/GF results for display only.
		const bool oldCalcNdlTts = prefs.calcndltts;
		prefs.calcndltts = true;
		const plot_info plot = has_planner_deco_state
			? create_plot_info_new(d, &d->dcs[0], &plan_deco_state)
			: create_plot_info_new(d, &d->dcs[0], nullptr);
		prefs.calcndltts = oldCalcNdlTts;
		std::vector<double> plotCns;
		plotCns.reserve(plot.entry.size());
		double cumulativeCns = 0.0;
		for (size_t index = 0; index < plot.entry.size(); ++index) {
			if (index > 0) {
				const plot_data &previous = plot.entry[index - 1];
				const plot_data &current = plot.entry[index];
				const int averagePo2 = std::lround((previous.pressures.o2 + current.pressures.o2) * 500.0);
				cumulativeCns += cns_for_segment(current.sec - previous.sec, averagePo2);
			}
			plotCns.push_back(cumulativeCns);
		}
		for (const struct sample &sample : d->dcs[0].samples) {
			QVariantMap point;
			point["time"] = sample.time.seconds;
			point["depth"] = sample.depth.mm;
			point["ndl"] = sample.ndl.seconds > 0 ? sample.ndl.seconds : -1;
			point["tts"] = sample.tts.seconds;
			point["ceiling"] = sample.stopdepth.mm;
			point["stopTime"] = sample.stoptime.seconds;
			point["cns"] = sample.cns;
			point["setpoint"] = sample.setpoint.mbar;
			point["inDeco"] = sample.in_deco;
			if (!plot.entry.empty()) {
				auto plotIt = std::lower_bound(plot.entry.begin(), plot.entry.end(), sample.time.seconds,
					[](const plot_data &entry, int time) { return entry.sec < time; });
				if (plotIt == plot.entry.end())
					plotIt = std::prev(plot.entry.end());
				else if (plotIt != plot.entry.begin()) {
					auto previous = std::prev(plotIt);
					if (sample.time.seconds - previous->sec <= plotIt->sec - sample.time.seconds)
						plotIt = previous;
				}
				point["gf"] = plotIt->current_gf * 100.0;
				point["surfaceGf"] = plotIt->surface_gf;
				if (sample.ndl.seconds <= 0 && plotIt->ndl_calc > 0 && !plotIt->in_deco_calc)
					point["ndl"] = plotIt->ndl_calc;
				point["po2"] = static_cast<int>(std::lround(plotIt->pressures.o2 * 1000.0));
				point["tissueLoad"] = *std::max_element(plotIt->percentages.begin(), plotIt->percentages.end());
				point["cns"] = plotCns[static_cast<size_t>(std::distance(plot.entry.begin(), plotIt))];
			}
			const int analysisDistance = std::abs(sample.time.seconds - enteredProfileRuntime);
			if (analysisDistance < closestAnalysisSample) {
				closestAnalysisSample = analysisDistance;
				analysis = point;
			}
			profileData.append(point);
		}
	}
	if (!analysis.isEmpty()) {
		const int calculatedTts = std::max(0, runtimeSeconds - enteredProfileRuntime);
		if (analysis.value("tts").toInt() <= 0 && calculatedTts > 0)
			analysis["tts"] = calculatedTts;
	}
	results["profile"] = profileData;
	results["analysis"] = analysis;

	// Save the dive if requested
	int newDiveId = -1;
	// AI-generated (Claude)
	// Keep invalid recreational plans available for preview, but do not persist
	// them as dive plans that could be mistaken for safe plans.
	if (shouldSave && planError == PLAN_OK) {
		std::unique_ptr<dive> d_to_save = std::make_unique<dive>();
		copy_dive(d, d_to_save.get());
		newDiveId = d_to_save->id;
		Command::addDive(std::move(d_to_save), divelog.autogroup, true);
	}
	results["newDiveId"] = newDiveId;

	return results;
}

QVariantList DivePlannerPointsModel::calculateGasInfo(const QString &cylinderType, int o2_permille, int he_permille)
{
	// Create a temporary dive context for calculations
	struct dive temp_dive;
	make_planner_dc(&temp_dive.dcs[0]);

	// Create a temporary cylinder with the specified gas mix
	cylinder_t temp_cyl;

	// Populate cylinder type info
	std::pair<volume_t, pressure_t> type_info = get_tank_info_data(tank_info_table, cylinderType.toStdString());
	volume_t size = type_info.first;
	pressure_t pressure = type_info.second;
	temp_cyl.type.size = size;
	temp_cyl.type.workingpressure = pressure;
	temp_cyl.type.description = cylinderType.toStdString();

	// Populate gas mix
	temp_cyl.gasmix.o2.permille = o2_permille;
	temp_cyl.gasmix.he.permille = he_permille;
	sanitize_gasmix(temp_cyl.gasmix);

	// Get narcotic preference
	bool o2_is_narcotic = qPrefDivePlanner::o2narcotic();
	int fNarcotic_permille;
	if (o2_is_narcotic) {
		// If O2 is narcotic, the narcotic fraction is N2 + O2.
		fNarcotic_permille = get_n2(temp_cyl.gasmix) + o2_permille;
	} else {
		// Otherwise, it's just N2.
		fNarcotic_permille = get_n2(temp_cyl.gasmix);
	}

	QVariantList results;
	// Calculate for a standard range of pO₂ values
	double po2_values[] = { 1.0, 1.1, 1.2, 1.3, 1.4, 1.5, 1.6, 1.7, 1.8, 1.9, 2.0 };

	for (double po2 : po2_values) {
		pressure_t po2_limit = { .mbar = static_cast<int>(po2 * 1000.0) };

		// Calculate MOD
		depth_t mod = temp_dive.gas_mod(temp_cyl.gasmix, po2_limit, 1_m);
		// Match the desktop planner's "Deco switch at" column: gas switches
		// are aligned to actual 3 m / 10 ft decompression-stop increments.
		depth_t deco_switch = temp_dive.gas_mod(temp_cyl.gasmix, po2_limit, m_or_ft(3, 10));

		// Calculate EAD/END at the MOD
		double p_amb_at_mod = temp_dive.depth_to_atm(mod);

		double p_narcotic = p_amb_at_mod * fNarcotic_permille / 1000.0;

		depth_t narcotic_depth = { .mm = 0 };
		if (fNarcotic_permille > 0) {
			double divisor = o2_is_narcotic ? 1.0 : 0.79;
			double ead_atm = p_narcotic / divisor;
			narcotic_depth.mm = static_cast<int>((ead_atm - 1.0) * 10000.0);
			if (narcotic_depth.mm < 0) narcotic_depth.mm = 0;
		}

		QVariantMap row;
		row["po2"] = QString::number(po2, 'f', 1);
		row["mod"] = get_depth_string(mod, true);
		row["decoSwitch"] = get_depth_string(deco_switch, true);
		row["ead"] = get_depth_string(narcotic_depth, true);
		results.append(row);
	}
	return results;
}
