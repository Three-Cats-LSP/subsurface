// SPDX-License-Identifier: GPL-2.0
#include "web/subsurface-neo/wasm/neowebplannermodel.h"
#include <QtTest>

class TestNeoWebPlannerModel : public QObject {
	Q_OBJECT
private slots:
	void buildsDraftWaypoints();
	void rejectsInvalidInputs();
};

void TestNeoWebPlannerModel::buildsDraftWaypoints()
{
	NeoWebPlannerModel model;
	model.setDepthMeters(24.0);
	model.setBottomTimeMinutes(35);
	model.setGas(QStringLiteral("EAN32"));
	QVERIFY(model.valid());
	QCOMPARE(model.waypoints().size(), 6);
	QVERIFY(model.summary().contains(QStringLiteral("EAN32")));
	QVERIFY(model.estimatedGasLiters() > 0.0);
	QVERIFY(model.availableGasLiters() > 0.0);
	QVERIFY(model.gasSummary().contains(QStringLiteral("reserve")));
	QVERIFY(model.warning().contains(QStringLiteral("native planner")));
	model.setSafetyStop(false);
	QCOMPARE(model.waypoints().size(), 4);
}

void TestNeoWebPlannerModel::rejectsInvalidInputs()
{
	NeoWebPlannerModel model;
	model.setDepthMeters(0.0);
	QVERIFY(!model.valid());
	QVERIFY(model.waypoints().isEmpty());
	model.reset();
	QVERIFY(model.valid());
	model.setCylinderVolume(3.0);
	model.setStartPressure(100);
	model.setReservePressure(90);
	QVERIFY(model.valid());
	QVERIFY(!model.gasAdequate());
	QVERIFY(model.warning().contains(QStringLiteral("exceeds usable gas")));
}

QTEST_GUILESS_MAIN(TestNeoWebPlannerModel)
#include "test_neowebplannermodel.moc"
