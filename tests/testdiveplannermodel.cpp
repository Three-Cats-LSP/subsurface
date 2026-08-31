// SPDX-License-Identifier: GPL-2.0
#include "testdiveplannermodel.h"
#include "qt-models/diveplannermodel.h"
#include "core/subsurfacestartup.h"
#include "commands/command.h"
#include "core/divelog.h"
#include "core/pref.h"
#include "core/string-format.h"
#include "core/units.h"
#include <QSignalSpy>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QScopedValueRollback>
#include <QVariantMap>
#include <memory>
#include <vector>

namespace {
int lastSavedPlannerDuration = -1;
QString lastSavedPlannerNotes;
}

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
void TestDivePlannerModel::testImportMissingCylinderDepth()
{
	prefs = default_prefs;
	dive importedDive;
	cylinder_t cylinder;
	cylinder.gasmix.o2 = 50_percent;
	importedDive.cylinders.push_back(cylinder);
	depth_t expected = calculate_deco_switch_depth(&importedDive, cylinder.gasmix);

	normalize_imported_cylinder_depths(&importedDive);

	QVERIFY(expected.mm != 0);
	QCOMPARE(importedDive.cylinders[0].depth.mm, expected.mm);
	prefs = default_prefs;
}

// AI-generated (Claude)
void TestDivePlannerModel::testImportedCylinderDepthPreserved()
{
	prefs = default_prefs;
	dive importedDive;
	cylinder_t cylinder;
	cylinder.gasmix.o2 = 50_percent;
	cylinder.depth = 12_m;
	importedDive.cylinders.push_back(cylinder);

	normalize_imported_cylinder_depths(&importedDive);

	QCOMPARE(importedDive.cylinders[0].depth.mm, 12000);
	prefs = default_prefs;
}

// AI-generated (Claude)
void TestDivePlannerModel::testCylinderDepthInput()
{
	DivePlannerPointsModel *model = DivePlannerPointsModel::instance();
	dive plannedDive;
	prefs = default_prefs;
	prefs.unit_system = METRIC;
	prefs.units = SI_units;
	model->setPlanMode(DivePlannerPointsModel::PLAN);
	model->createSimpleDive(&plannedDive);

	CylindersModel *cylinders = model->cylindersModel();
	QModelIndex depthIndex = cylinders->index(0, CylindersModel::DEPTH);
	cylinder_t *cylinder = plannedDive.get_cylinder(0);
	depth_t calculatedDepth = calculate_deco_switch_depth(&plannedDive, cylinder->gasmix);

	QVERIFY(cylinders->setData(depthIndex, QStringLiteral("12 m")));
	QCOMPARE(cylinder->depth.mm, 12000);

	QVERIFY(cylinders->setData(depthIndex, QString()));
	QCOMPARE(cylinder->depth.mm, calculatedDepth.mm);

	QVERIFY(cylinders->setData(depthIndex, QStringLiteral("12 m")));
	QVERIFY(cylinders->setData(depthIndex, QStringLiteral("  \t")));
	QCOMPARE(cylinder->depth.mm, calculatedDepth.mm);

	QVERIFY(cylinders->setData(depthIndex, QStringLiteral("12 m")));
	QVERIFY(cylinders->setData(depthIndex, QStringLiteral("0")));
	QCOMPARE(cylinder->depth.mm, 0);
	QCOMPARE(cylinders->data(depthIndex, Qt::DisplayRole).toString(), get_depth_string(0_m, true));

	model->resetPlanState();
	prefs = default_prefs;
}

// AI-generated (Claude)
void TestDivePlannerModel::testStoredZeroCylinderDepthDisplay()
{
	DivePlannerPointsModel *model = DivePlannerPointsModel::instance();
	dive plannedDive;
	prefs = default_prefs;
	prefs.unit_system = METRIC;
	prefs.units = SI_units;
	model->setPlanMode(DivePlannerPointsModel::PLAN);
	model->createSimpleDive(&plannedDive);

	CylindersModel *cylinders = model->cylindersModel();
	QModelIndex depthIndex = cylinders->index(0, CylindersModel::DEPTH);
	plannedDive.cylinders[0].depth = 0_m;

	QCOMPARE(cylinders->data(depthIndex, Qt::DisplayRole).toString(), get_depth_string(0_m, true));

	model->resetPlanState();
	prefs = default_prefs;
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
	QVERIFY(result.contains("analysis"));
	QVERIFY(result.value("runtimeSeconds").toInt() >= 20 * 60);
	QCOMPARE(result.value("bottomTimeSeconds").toInt(), 20 * 60);
	const QVariantList timeline = result.value("timeline").toList();
	QVERIFY(!timeline.empty());
	QCOMPARE(timeline.first().toMap().value("phase").toString(), QStringLiteral("descent"));
	QVERIFY(timeline.last().toMap().value("runTime").toInt() == result.value("runtimeSeconds").toInt());
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
	QVERIFY(lastSample.contains("ead"));
	QVERIFY(lastSample.contains("gas"));
	QVERIFY(lastSample.contains("tissueLoad"));
	const QVariantMap analysis = result.value("analysis").toMap();
	QVERIFY(!analysis.empty());
	QVERIFY(qAbs(analysis.value("time").toInt() - 21 * 60) <= 60);
	QVERIFY(qAbs(analysis.value("depth").toInt() - 18000) <= 1000);
	QVERIFY(analysis.value("ndl").toInt() > 0);
	QVERIFY(analysis.value("cns").toDouble() > 0.0);
	QVERIFY(!result.value("notes").toString().contains("<div>"));
	QVERIFY(!result.value("notes").toString().contains("</table>"));
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
	for (const QVariant &stop : decoSchedule)
	{
		QVERIFY(stop.toMap().value("duration").toInt() > 0);
		QVERIFY(stop.toMap().contains("runTime"));
		QVERIFY(stop.toMap().contains("gas"));
	}

	// A saved planner dive must retain the generated runtime even though generic
	// fixup deliberately ignores planner dive computers at the top level.
	lastSavedPlannerDuration = -1;
	lastSavedPlannerNotes.clear();
	segment.insert("depth", 30);
	segment.insert("duration", 30);
	segments[0] = segment;
	const QVariantMap savedPlan = model->calculatePlan(cylinders, segments, "2026-01-01", "12:00:00", OC, 10300, 1013, true);
	QVERIFY(savedPlan.value("planSaveAllowed").toBool());
	QVERIFY(lastSavedPlannerDuration >= 30 * 60);
	QCOMPARE(lastSavedPlannerDuration, savedPlan.value("runtimeSeconds").toInt());
	QVERIFY(!lastSavedPlannerNotes.isEmpty());

	// Match the canonical desktop case reported by users: 40 m at 30 min
	// runtime on air with EAN50 and oxygen available for decompression.
	QVariantMap ean50Cylinder = cylinder;
	ean50Cylinder.insert("type", "10L 200 bar");
	ean50Cylinder.insert("mix", "50/0");
	ean50Cylinder.insert("use", OC_GAS);
	QVariantMap oxygenCylinder = ean50Cylinder;
	oxygenCylinder.insert("mix", "100/0");
	cylinder.insert("type", "12L 200 bar");
	cylinder.insert("mix", "21/0");
	cylinder.insert("use", OC_GAS);
	segment.insert("depth", 40);
	segment.insert("duration", 30);
	segments[0] = segment;
	const QVariantMap desktopEquivalent = model->calculatePlan(QVariantList { cylinder, ean50Cylinder, oxygenCylinder },
		segments, "2026-01-01", "12:00:00", OC, 10300, 1013, false);
	// Exact decompression runtime and first-stop depth also depend on tissue
	// state from preceding dives.  The wrapper contract is that the native
	// engine extends the 30-minute waypoint with a complete ascent schedule.
	QVERIFY(desktopEquivalent.value("runtimeSeconds").toInt() > 30 * 60);
	const QVariantList desktopSchedule = desktopEquivalent.value("schedule").toList();
	QVERIFY(!desktopSchedule.empty());
	for (const QVariant &stop : desktopSchedule) {
		const QVariantMap row = stop.toMap();
		QVERIFY(row.value("tts").toInt() > 0);
	}
	QVERIFY(!desktopEquivalent.value("gasAnalysis").toList().first().toMap().value("remaining").toString().contains(QStringLiteral("4,294")));
	bool switchedToEan50 = false;
	bool switchedToOxygen = false;
	for (const QVariant &timelineRow : desktopEquivalent.value("timeline").toList()) {
		const QVariantMap row = timelineRow.toMap();
		if (!row.value("gasSwitch").toBool())
			continue;
		switchedToEan50 |= row.value("gas").toString() == QStringLiteral("50/0");
		switchedToOxygen |= row.value("gas").toString() == QStringLiteral("100%");
	}
	QVERIFY(switchedToEan50);
	QVERIFY(switchedToOxygen);
	QCOMPARE(desktopEquivalent.value("gasAnalysis").toList().first().toMap().value("mix").toString(), QStringLiteral("Air"));
	const QVariantList desktopProfile = desktopEquivalent.value("profile").toList();
	QVERIFY(!desktopProfile.empty());
	bool hasCalculatedCeiling = false;
	bool hasCalculatedTts = false;
	bool hasProfileGasSwitch = false;
	for (const QVariant &profileValue : desktopProfile) {
		const QVariantMap sample = profileValue.toMap();
		hasCalculatedCeiling |= sample.value("ceiling").toInt() > 0;
		hasCalculatedTts |= sample.value("tts").toInt() > 0;
		hasProfileGasSwitch |= sample.value("gasSwitch").toBool();
		QVERIFY(sample.contains("ead"));
		QVERIFY(sample.contains("gas"));
	}
	QVERIFY(hasCalculatedCeiling);
	QVERIFY(hasCalculatedTts);
	QVERIFY(hasProfileGasSwitch);
	const double bottomSurfaceGf = desktopEquivalent.value("analysis").toMap().value("surfaceGf").toDouble();
	const double surfacedSurfaceGf = desktopProfile.last().toMap().value("surfaceGf").toDouble();
	QVERIFY(surfacedSurfaceGf > 0.0);
	QVERIFY(surfacedSurfaceGf < bottomSurfaceGf);

	const QVariantList oxygenReference = model->calculateGasInfo(QStringLiteral("10L 200 bar"), 1000, 0);
	const QVariantList ean50Reference = model->calculateGasInfo(QStringLiteral("10L 200 bar"), 500, 0);
	QCOMPARE(oxygenReference.at(4).toMap().value("mod").toString(), get_depth_string(3_m, true));
	QCOMPARE(oxygenReference.at(6).toMap().value("decoSwitch").toString(), get_depth_string(6_m, true));
	QCOMPARE(ean50Reference.at(4).toMap().value("mod").toString(), get_depth_string(17_m, true));
	QCOMPARE(ean50Reference.at(6).toMap().value("decoSwitch").toString(), get_depth_string(21_m, true));

	// The profile keeps the inspector's native plot values alongside the
	// schedule. The detailed fields above are asserted on the regular profile
	// contract because an in_deco sample flag is not emitted by every plan mode.
	QVERIFY(!deepDecoResult.value("profile").toList().empty());
	prefs.display_variations = true;
	const QVariantMap variationResult = model->calculatePlan(cylinders, segments, "2026-01-01", "12:00:00", OC, 10300, 1013, false);
	QVERIFY(!variationResult.value("notes").toString().contains("VARIATIONS"));
	prefs.display_variations = false;
	segment.insert("depth", 18);
	segment.insert("duration", 20);
	segments[0] = segment;
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
		// Keep the regression focused on decompression behavior rather than an
		// exhausted single cylinder at the deeper/longer fixture profiles.
		cylinder.insert("type", "D12 232 bar");
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
	const QVariantMap deco45 = calculate(45, 30);
	const QVariantMap deco50 = calculate(50, 35);
	QVERIFY(deco45.value("planSaveAllowed").toBool());
	QVERIFY(!deco45.value("schedule").toList().empty());
	QVERIFY(!deco50.value("schedule").toList().empty());
	QVERIFY(runtime(deco50) > runtime(deco45));

	// Monotonicity checks are stable across native implementation details.
	const QVariantMap shallow = calculate(30, 25);
	const QVariantMap longBottomTime = calculate(45, 35);
	QVERIFY(runtime(deco45) >= runtime(shallow));
	QVERIFY(runtime(longBottomTime) >= runtime(deco45));
	prefs.gfhigh = 85;
	const QVariantMap relaxedGf = calculate(45, 30);
	prefs.gfhigh = 70;
	const QVariantMap conservativeGf = calculate(45, 30);
	QVERIFY(runtime(conservativeGf) >= runtime(relaxedGf));

	// Gas and toxicity assertions are checked through the same QML-facing data
	// contract that Neo uses, rather than through a duplicate calculation.
	prefs.gfhigh = 75;
	const QVariantMap air = calculate(30, 30, 21);
	const QVariantMap ean32 = calculate(30, 30, 32);
	QVERIFY(runtime(ean32) <= runtime(air));
	QVERIFY(air.value("otu").toInt() >= 0);
	QVERIFY(deco45.value("otu").toInt() >= 0);
	QVERIFY(!deco45.value("gasAnalysis").toList().empty());

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

void TestDivePlannerModel::testNeoPlannerFixtureManifest()
{
	QFile fixtureFile(QStringLiteral(SUBSURFACE_TEST_DATA) + QStringLiteral("/tests/data/neo-planner-regression.json"));
	QVERIFY2(fixtureFile.open(QIODevice::ReadOnly), qPrintable(fixtureFile.errorString()));
	QJsonParseError parseError;
	const QJsonDocument fixtureDocument = QJsonDocument::fromJson(fixtureFile.readAll(), &parseError);
	QCOMPARE(parseError.error, QJsonParseError::NoError);
	QVERIFY(fixtureDocument.isObject());
	QCOMPARE(fixtureDocument.object().value(QStringLiteral("schema")).toInt(), 1);

	DivePlannerPointsModel *model = DivePlannerPointsModel::instance();
	for (const QJsonValue &fixtureValue : fixtureDocument.object().value(QStringLiteral("scenarios")).toArray()) {
		const QJsonObject fixture = fixtureValue.toObject();
		const QByteArray fixtureName = fixture.value(QStringLiteral("id")).toString().toUtf8();
		QScopedValueRollback<decltype(prefs)> restorePrefs(prefs, default_prefs);
		prefs.unit_system = METRIC;
		prefs.units = SI_units;
		prefs.drop_stone_mode = false;
		const QString algorithm = fixture.value(QStringLiteral("algorithm")).toString();
		prefs.planner_deco_mode = algorithm == QStringLiteral("vpmb") ? VPMB : BUEHLMANN;
		prefs.gflow = fixture.value(QStringLiteral("gfLow")).toInt(30);
		prefs.gfhigh = fixture.value(QStringLiteral("gfHigh")).toInt(75);
		prefs.vpmb_conservatism = fixture.value(QStringLiteral("vpmbConservatism")).toInt(0);

		QVariantMap cylinder;
		// Use enough back-gas capacity for every fixture so a required schedule
		// is not suppressed by an already exhausted cylinder.
		cylinder.insert(QStringLiteral("type"), QStringLiteral("D12 232 bar"));
		cylinder.insert(QStringLiteral("mix"), fixture.value(QStringLiteral("mix")).toString(QStringLiteral("21/0")));
		cylinder.insert(QStringLiteral("pressure"), 200);
		cylinder.insert(QStringLiteral("use"), OC_GAS);
		QVariantMap segment;
		segment.insert(QStringLiteral("depth"), fixture.value(QStringLiteral("depthMeters")).toInt());
		segment.insert(QStringLiteral("duration"), fixture.value(QStringLiteral("durationMinutes")).toInt());
		segment.insert(QStringLiteral("gas"), 0);
		segment.insert(QStringLiteral("setpoint"), 0);
		segment.insert(QStringLiteral("divemode"), OC);

		const QVariantMap result = model->calculatePlan(QVariantList { cylinder }, QVariantList { segment },
			QStringLiteral("2026-01-01"), QStringLiteral("12:00:00"), OC, 10300,
			fixture.value(QStringLiteral("surfacePressureMbar")).toInt(1013), false);
		QVERIFY2(result.value(QStringLiteral("planSaveAllowed")).toBool() == fixture.value(QStringLiteral("planSaveAllowed")).toBool(), fixtureName.constData());
		QVERIFY2(result.value(QStringLiteral("profile")).toList().size() >= fixture.value(QStringLiteral("minimumProfileSamples")).toInt(2), fixtureName.constData());
		const bool hasSchedule = !result.value(QStringLiteral("schedule")).toList().empty();
		if (fixture.value(QStringLiteral("requiresSchedule")).toBool())
			QVERIFY2(hasSchedule, fixtureName.constData());
		QVERIFY2(result.value(QStringLiteral("otu")).toInt() >= 0, fixtureName.constData());
		QVERIFY2(!result.value(QStringLiteral("gasAnalysis")).toList().empty(), fixtureName.constData());
	}
	prefs = default_prefs;
}

// Stubs for symbols referenced by libraries linked into TestDivePlannerModel
// but not available without the full desktop-widgets and commands libraries.

// Command stubs — these are referenced by various qt-models source files
namespace Command {

void addDive(std::unique_ptr<dive> d, bool, bool)
{
	lastSavedPlannerDuration = d ? d->duration.seconds : -1;
	lastSavedPlannerNotes = d ? QString::fromStdString(d->notes) : QString();
}
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
