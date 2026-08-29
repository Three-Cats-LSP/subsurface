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
	QCOMPARE(model.waypoints().size(), 4);
	QVERIFY(model.summary().contains(QStringLiteral("EAN32")));
	QVERIFY(model.warning().contains(QStringLiteral("native planner")));
}

void TestNeoWebPlannerModel::rejectsInvalidInputs()
{
	NeoWebPlannerModel model;
	model.setDepthMeters(0.0);
	QVERIFY(!model.valid());
	QVERIFY(model.waypoints().isEmpty());
	model.reset();
	QVERIFY(model.valid());
}

QTEST_GUILESS_MAIN(TestNeoWebPlannerModel)
#include "test_neowebplannermodel.moc"
