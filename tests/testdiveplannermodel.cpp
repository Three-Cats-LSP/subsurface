// SPDX-License-Identifier: GPL-2.0
#include "testdiveplannermodel.h"
#include "qt-models/diveplannermodel.h"
#include "core/subsurfacestartup.h"
#include "commands/command.h"
#include "core/divelog.h"
#include "core/pref.h"
#include "core/units.h"
#include <QSignalSpy>
#include <QVariantMap>
#include <memory>
#include <vector>

#ifdef MAP_SUPPORT
#include "desktop-widgets/mapwidget.h"
#include "desktop-widgets/mainwindow.h"
#endif

void TestDivePlannerModel::initTestCase()
{
	TestBase::initTestCase();

	QCoreApplication::setOrganizationName("Subsurface");
	QCoreApplication::setOrganizationDomain("subsurface.hohndel.org");
	QCoreApplication::setApplicationName("SubsurfaceTestDivePlannerModel");
}

void TestDivePlannerModel::testEmptyModelDataAccess()
{
	// Test that accessing data on an empty model doesn't crash
	DivePlannerPointsModel *model = DivePlannerPointsModel::instance();
	
	model->resetPlanState();
	
	// Try to access data - should return invalid QVariant, not crash
	QModelIndex invalidIndex = model->index(0, 0);
	QVariant result = model->data(invalidIndex, Qt::DisplayRole);
	QVERIFY(!result.isValid());
}

void TestDivePlannerModel::testEmptyModelEmitDataChanged()
{
	// Test that emitDataChanged on an empty model doesn't crash or emit invalid signals
	DivePlannerPointsModel *model = DivePlannerPointsModel::instance();
	
	model->resetPlanState();
	
	// Set up signal spy to verify no dataChanged signal with invalid range is emitted
	QSignalSpy spy(model, &DivePlannerPointsModel::dataChanged);
	
	// This should not crash and should not emit dataChanged for an empty model
	model->emitDataChanged();
	
	// Verify no signal was emitted (model is empty)
	QCOMPARE(spy.count(), 0);
}

void TestDivePlannerModel::testInvalidCylinderIndex()
{
	// This test verifies that the model handles invalid cylinder indices gracefully
	// Note: This is testing the internal validation, not creating an actual invalid state
	// The fix ensures cylinder access is checked before accessing cylinders
	
	DivePlannerPointsModel *model = DivePlannerPointsModel::instance();
	model->resetPlanState();
	
	// When model is in NOTHING mode, data() should return invalid for any index
	QModelIndex testIndex = model->index(0, DivePlannerPointsModel::GAS);
	QVariant result = model->data(testIndex, Qt::DisplayRole);
	QVERIFY(!result.isValid());
}

void TestDivePlannerModel::testInvalidRowIndex()
{
	// Test that accessing an invalid row index doesn't crash
	DivePlannerPointsModel *model = DivePlannerPointsModel::instance();
	model->resetPlanState();
	
	// Try various invalid indices
	QModelIndex invalidIndex1 = model->index(-1, 0);
	QVariant result1 = model->data(invalidIndex1, Qt::DisplayRole);
	QVERIFY(!result1.isValid());
	
	QModelIndex invalidIndex2 = model->index(9999, 0);
	QVariant result2 = model->data(invalidIndex2, Qt::DisplayRole);
	QVERIFY(!result2.isValid());
	
	// Index that looks valid but is out of bounds for empty model
	QModelIndex invalidIndex3 = model->index(0, 0);
	QVariant result3 = model->data(invalidIndex3, Qt::DisplayRole);
	QVERIFY(!result3.isValid());
}

void TestDivePlannerModel::testNothingModeDataAccess()
{
	// Test that when mode is NOTHING, data access returns invalid QVariant
	DivePlannerPointsModel *model = DivePlannerPointsModel::instance();
	model->resetPlanState();
	
	// Test all column types
	for (int col = 0; col < model->columnCount(); ++col) {
		QModelIndex idx = model->index(0, col);
		QVariant result = model->data(idx, Qt::DisplayRole);
		QVERIFY(!result.isValid());
	}
	
	// Test flags() as well
	QModelIndex idx = model->index(0, 0);
	Qt::ItemFlags flags = model->flags(idx);
	QCOMPARE(flags, Qt::NoItemFlags);
}

void TestDivePlannerModel::testSurfaceAirCylinderDataAccess()
{
	DivePlannerPointsModel *model = DivePlannerPointsModel::instance();
	dive plannedDive;

	model->setPlanMode(DivePlannerPointsModel::PLAN);
	model->createSimpleDive(&plannedDive);

	QVERIFY(model->rowCount() > 1);
	int surfaceAirCylinder = static_cast<int>(plannedDive.cylinders.size());
	QModelIndex gasIndex = model->index(1, DivePlannerPointsModel::GAS);
	model->gasChange(gasIndex, surfaceAirCylinder);

	QVariant gas = model->data(gasIndex, Qt::DisplayRole);
	QVERIFY(gas.isValid());
	QCOMPARE(gas.toString(), QStringLiteral("AIR"));

	model->resetPlanState();
}

// AI-generated (Claude)
// Recreational plans that require decompression cannot be saved, and become
// saveable again once the entered profile is within the NDL.
void TestDivePlannerModel::testRecreationalPlanSaveAllowed()
{
	DivePlannerPointsModel *model = DivePlannerPointsModel::instance();
	dive plannedDive;

	prefs = default_prefs;
	prefs.unit_system = METRIC;
	prefs.units = SI_units;
	prefs.planner_deco_mode = RECREATIONAL;
	prefs.drop_stone_mode = false;
	model->setPlanMode(DivePlannerPointsModel::PLAN);
	diveplan &plan = model->getDiveplan();
	plan.salinity = 10300;
	plan.surface_pressure = 1_atm;
	plan.gfhigh = 100;
	plan.gflow = 100;
	plan.bottomsac = prefs.bottomsac;
	plan.decosac = prefs.decosac;
	model->createSimpleDive(&plannedDive);
	QVERIFY(model->planSaveAllowed());
	QSignalSpy saveAllowedSpy(model, &DivePlannerPointsModel::planSaveAllowedChanged);

	int lastRow = model->rowCount() - 1;
	model->setData(model->index(0, DivePlannerPointsModel::DEPTH), 50);
	model->setData(model->index(lastRow, DivePlannerPointsModel::DEPTH), 50);
	model->setData(model->index(lastRow, DivePlannerPointsModel::RUNTIME), 50);
	QVERIFY(!model->planSaveAllowed());
	QCOMPARE(saveAllowedSpy.last().at(0).toBool(), false);

	model->savePlan();
	QCOMPARE(model->currentMode(), DivePlannerPointsModel::PLAN);

	model->setData(model->index(0, DivePlannerPointsModel::DEPTH), 20);
	model->setData(model->index(lastRow, DivePlannerPointsModel::DEPTH), 20);
	model->setData(model->index(lastRow, DivePlannerPointsModel::RUNTIME), 20);
	QVERIFY(model->planSaveAllowed());
	QCOMPARE(saveAllowedSpy.last().at(0).toBool(), true);

	model->resetPlanState();
	prefs = default_prefs;
}

void TestDivePlannerModel::testNeoPlanResultContract()
{
	DivePlannerPointsModel *model = DivePlannerPointsModel::instance();
	prefs = default_prefs;
	prefs.unit_system = METRIC;
	prefs.units = SI_units;
	prefs.planner_deco_mode = BUEHLMANN;
	prefs.drop_stone_mode = false;

	QVariantMap cylinder;
	cylinder.insert("type", "AL80");
	cylinder.insert("mix", "21/0");
	cylinder.insert("pressure", 200);
	cylinder.insert("use", 0);
	QVariantMap segment;
	segment.insert("depth", 18);
	segment.insert("duration", 20);
	segment.insert("gas", 0);
	segment.insert("setpoint", 0);
	segment.insert("divemode", 0);

	QVariantList cylinders;
	cylinders.append(cylinder);
	QVariantList segments;
	segments.append(segment);
	const QVariantMap result = model->calculatePlan(cylinders, segments, "2026-01-01", "12:00:00", OC, 10300, 1013, false);
	QVERIFY(result.contains("planSaveAllowed"));
	QVERIFY(result.value("planSaveAllowed").toBool());
	QVERIFY(result.contains("otu"));
	QVERIFY(result.value("otu").toInt() >= 0);
	QVERIFY(result.contains("schedule"));
	QVERIFY(result.value("schedule").canConvert<QVariantList>());
	QVERIFY(!result.value("gasAnalysis").toList().empty());
	QVERIFY(result.value("gasAnalysis").toList().first().toMap().contains("remaining"));
	QVERIFY(result.value("gasAnalysis").toList().first().toMap().contains("startPressure"));

	const QVariantList profile = result.value("profile").toList();
	QVERIFY(!profile.empty());
	const QVariantMap lastSample = profile.last().toMap();
	QVERIFY(lastSample.contains("ndl"));
	QVERIFY(lastSample.contains("tts"));
	QVERIFY(lastSample.contains("ceiling"));
	QVERIFY(lastSample.contains("cns"));
	QVERIFY(lastSample.contains("gf"));
	QVERIFY(lastSample.contains("surfaceGf"));
	QVERIFY(lastSample.contains("po2"));
	QVERIFY(lastSample.contains("tissueLoad"));
	const QVariantMap customWater = model->calculatePlan(cylinders, segments, "2026-01-01", "12:00:00", OC, 10150, 1013, false);
	QVERIFY(!customWater.value("profile").toList().empty());

	segment.insert("depth", 45);
	segment.insert("duration", 30);
	segments[0] = segment;
	const QVariantMap deepDecoResult = model->calculatePlan(cylinders, segments, "2026-01-01", "12:00:00", OC, 10300, 1013, false);
	const QVariantList decoSchedule = deepDecoResult.value("schedule").toList();
	QVERIFY(!decoSchedule.empty());
	QVERIFY(decoSchedule.first().toMap().contains("depth"));
	QVERIFY(decoSchedule.first().toMap().contains("duration"));

	// A decompression profile must carry real values from the native plot
	// pipeline, rather than merely placeholder keys for the Neo inspector.
	QVariantMap decoSample;
	for (const QVariant &value : deepDecoResult.value("profile").toList()) {
		const QVariantMap sample = value.toMap();
		if (sample.value("inDeco").toBool()) {
			decoSample = sample;
			break;
		}
	}
	QVERIFY(!decoSample.empty());
	QVERIFY(decoSample.value("ceiling").toInt() > 0);
	QVERIFY(decoSample.value("tts").toInt() > 0);
	QVERIFY(decoSample.value("gf").toDouble() > 0.0);
	QVERIFY(decoSample.value("surfaceGf").toDouble() > 0.0);
	prefs.display_variations = true;
	const QVariantMap variationResult = model->calculatePlan(cylinders, segments, "2026-01-01", "12:00:00", OC, 10300, 1013, false);
	QVERIFY(!variationResult.value("notes").toString().contains("VARIATIONS"));
	prefs.display_variations = false;
	segment.insert("depth", 18);
	segment.insert("duration", 20);
	segments[0] = segment;
	cylinder.insert("use", NOT_USED);
	cylinders[0] = cylinder;
	const QVariantMap lostGasResult = model->calculatePlan(cylinders, segments, "2026-01-01", "12:00:00", OC, 10300, 1013, false);
	QVERIFY(!lostGasResult.value("planSaveAllowed").toBool());
	cylinder.insert("use", OC_GAS);
	cylinders[0] = cylinder;
	prefs.planner_deco_mode = VPMB;
	const QVariantMap vpmbResult = model->calculatePlan(cylinders, segments, "2026-01-01", "12:00:00", OC, 10300, 1013, false);
	QVERIFY(!vpmbResult.value("profile").toList().empty());
	prefs.planner_deco_mode = BUEHLMANN;

	segment.insert("divemode", PSCR);
	segments[0] = segment;
	const QVariantMap pscrResult = model->calculatePlan(cylinders, segments, "2026-01-01", "12:00:00", PSCR, 10300, 1013, false);
	QVERIFY(!pscrResult.value("profile").toList().empty());
	segment.insert("divemode", OC);
	segments[0] = segment;

	cylinder.insert("mix", "18/45");
	cylinder.insert("use", OC_GAS);
	cylinders[0] = cylinder;
	segment.insert("depth", 45);
	segment.insert("duration", 25);
	const QVariantMap trimixResult = model->calculatePlan(cylinders, segments, "2026-01-01", "12:00:00", OC, 10300, 1013, false);
	QVERIFY(!trimixResult.value("profile").toList().empty());
	const QVariantMap altitudeResult = model->calculatePlan(cylinders, segments, "2026-01-01", "12:00:00", OC, 10300, 850, false);
	QVERIFY(!altitudeResult.value("profile").toList().empty());

	cylinder.insert("mix", "21/35");
	cylinder.insert("use", DILUENT);
	cylinders[0] = cylinder;
	segment.insert("depth", 30);
	segment.insert("duration", 20);
	segment.insert("setpoint", 1300);
	segment.insert("divemode", CCR);
	segments[0] = segment;
	const QVariantMap ccrResult = model->calculatePlan(cylinders, segments, "2026-01-01", "12:00:00", CCR, 10300, 1013, false);
	QVERIFY(!ccrResult.value("profile").toList().empty());
	cylinder.insert("mix", "21/0");
	cylinder.insert("use", OC_GAS);
	cylinders[0] = cylinder;
	segment.insert("setpoint", 0);
	segment.insert("divemode", OC);
	segments[0] = segment;

	prefs.planner_deco_mode = RECREATIONAL;
	segment.insert("depth", 50);
	segment.insert("duration", 50);
	segments[0] = segment;
	const QVariantMap exceedsNdl = model->calculatePlan(cylinders, segments, "2026-01-01", "12:00:00", OC, 10300, 1013, false);
	QVERIFY(exceedsNdl.value("exceedsNDL").toBool());
	QVERIFY(!exceedsNdl.value("planSaveAllowed").toBool());

	const QVariantMap invalidStart = model->calculatePlan(cylinders, segments, "not-a-date", "not-a-time", OC, 10300, 1013, false);
	QVERIFY(!invalidStart.value("planSaveAllowed").toBool());
	QVERIFY(!invalidStart.value("notes").toString().isEmpty());
	prefs = default_prefs;
}

void TestDivePlannerModel::testNeoPlannerNativeRegression()
{
	// Native counterparts of the portable engine assertions in the external
	// planner suites. These deliberately use Subsurface results as the oracle:
	// external-engine schedules have different, non-native assumptions.
	DivePlannerPointsModel *model = DivePlannerPointsModel::instance();
	prefs = default_prefs;
	prefs.unit_system = METRIC;
	prefs.units = SI_units;
	prefs.planner_deco_mode = BUEHLMANN;
	prefs.gflow = 30;
	prefs.gfhigh = 75;
	prefs.drop_stone_mode = false;

	auto calculate = [model](int depth, int duration, int oxygen = 21) {
		QVariantMap cylinder;
		cylinder.insert("type", "AL80");
		cylinder.insert("mix", QString::number(oxygen) + "/0");
		cylinder.insert("pressure", 200);
		cylinder.insert("use", OC_GAS);
		QVariantMap segment;
		segment.insert("depth", depth);
		segment.insert("duration", duration);
		segment.insert("gas", 0);
		segment.insert("setpoint", 0);
		segment.insert("divemode", OC);
		return model->calculatePlan(QVariantList { cylinder }, QVariantList { segment },
			"2026-01-01", "12:00:00", OC, 10300, 1013, false);
	};
	auto runtime = [](const QVariantMap &result) {
		const QVariantList profile = result.value("profile").toList();
		return profile.empty() ? 0 : profile.last().toMap().value("time").toInt();
	};

	// Core no-decompression and decompression scenario checks.
	const QVariantMap noDeco = calculate(15, 20);
	QVERIFY(noDeco.value("planSaveAllowed").toBool());
	for (const QVariant &sample : noDeco.value("profile").toList())
		QVERIFY(!sample.toMap().value("inDeco").toBool());
	const QVariantMap deco40 = calculate(40, 25);
	const QVariantMap deco50 = calculate(50, 30);
	QVERIFY(deco40.value("planSaveAllowed").toBool());
	QVERIFY(!deco40.value("schedule").toList().empty());
	QVERIFY(!deco50.value("schedule").toList().empty());
	QVERIFY(runtime(deco50) > runtime(deco40));

	// Monotonicity checks are stable across native implementation details.
	const QVariantMap shallow = calculate(30, 25);
	const QVariantMap longBottomTime = calculate(40, 30);
	QVERIFY(runtime(deco40) >= runtime(shallow));
	QVERIFY(runtime(longBottomTime) >= runtime(deco40));
	prefs.gfhigh = 85;
	const QVariantMap relaxedGf = calculate(40, 25);
	prefs.gfhigh = 70;
	const QVariantMap conservativeGf = calculate(40, 25);
	QVERIFY(runtime(conservativeGf) >= runtime(relaxedGf));

	// Gas and toxicity assertions are checked through the same QML-facing data
	// contract that Neo uses, rather than through a duplicate calculation.
	prefs.gfhigh = 75;
	const QVariantMap air = calculate(30, 30, 21);
	const QVariantMap ean32 = calculate(30, 30, 32);
	QVERIFY(runtime(ean32) <= runtime(air));
	QVERIFY(air.value("otu").toInt() >= 0);
	QVERIFY(deco40.value("otu").toInt() >= 0);
	QVERIFY(!deco40.value("gasAnalysis").toList().empty());

	// VPM-B verification follows the native model's own conservatism ordering,
	// instead of comparing schedules from a different implementation.
	prefs.planner_deco_mode = VPMB;
	prefs.vpmb_conservatism = 0;
	const QVariantMap vpmb40 = calculate(40, 25);
	const QVariantMap vpmb50 = calculate(50, 25);
	const QVariantMap vpmbLong = calculate(40, 30);
	QVERIFY(vpmb40.value("planSaveAllowed").toBool());
	QVERIFY(runtime(vpmb50) > runtime(vpmb40));
	QVERIFY(runtime(vpmbLong) > runtime(vpmb40));
	prefs.vpmb_conservatism = 3;
	const QVariantMap vpmbConservative = calculate(40, 25);
	QVERIFY(runtime(vpmbConservative) >= runtime(vpmb40));
	prefs = default_prefs;
}

// Stubs for symbols referenced by libraries linked into TestDivePlannerModel
// but not available without the full desktop-widgets and commands libraries.

// Command stubs — these are referenced by various qt-models source files
namespace Command {

void addDive(std::unique_ptr<dive>, bool, bool) {}
void importDives(struct divelog *, int, const QString &) {}
void replanDive(dive *) {}
int editCylinder(int, cylinder_t, EditCylinderType, bool) { return 0; }
void editSensors(int, int, int) {}
void editDiveSiteName(dive_site *, const QString &) {}
void editDiveSiteDescription(dive_site *, const QString &) {}
void removePictures(const std::vector<PictureListForDeletion> &) {}
int editWeight(int, weightsystem_t, bool) { return 0; }

} // namespace Command

// MapWidget stubs — referenced by maplocationmodel.cpp and core/divefilter.cpp
#ifdef MAP_SUPPORT
MapWidget *MapWidget::m_instance = nullptr;

MapWidget *MapWidget::instance()
{
	return m_instance;
}

bool MapWidget::editMode() const
{
	return false;
}

void MapWidget::reload()
{
}

void MapWidget::setSelected(std::vector<dive_site *>)
{
}

// MainWindow stub — referenced by core/divefilter.cpp
MainWindow *MainWindow::instance()
{
	return nullptr;
}
#endif

QTEST_GUILESS_MAIN(TestDivePlannerModel)
