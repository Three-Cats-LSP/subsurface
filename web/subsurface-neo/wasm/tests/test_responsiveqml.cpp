// SPDX-License-Identifier: GPL-2.0
#include "web/subsurface-neo/wasm/neowebdivelogmodel.h"
#include "web/subsurface-neo/wasm/neowebplannermodel.h"
#include "web/subsurface-neo/wasm/neowebsyncmodel.h"
#include "web/subsurface-neo/wasm/webcapabilities.h"
#include "web/subsurface-neo/wasm/webdevicetransport.h"

#include <QDir>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickWindow>
#include <QtTest>

class TestResponsiveQml : public QObject {
	Q_OBJECT
private slots:
	void rendersBreakpoints_data();
	void rendersBreakpoints();
};

void TestResponsiveQml::rendersBreakpoints_data()
{
	QTest::addColumn<int>("width");
	QTest::addColumn<int>("height");
	QTest::addColumn<bool>("compact");
	QTest::newRow("mobile-390") << 390 << 844 << true;
	QTest::newRow("tablet-768") << 768 << 900 << false;
	QTest::newRow("desktop-1180") << 1180 << 760 << false;
	QTest::newRow("wide-1440") << 1440 << 900 << false;
}

void TestResponsiveQml::rendersBreakpoints()
{
	QFETCH(int, width);
	QFETCH(int, height);
	QFETCH(bool, compact);
	WebCapabilities capabilities;
	WebDeviceTransport transport(false, false);
	NeoWebDiveLogModel diveLog;
	NeoWebPlannerModel planner;
	NeoWebSyncModel sync;
	QQmlApplicationEngine engine;
	engine.rootContext()->setContextProperty(QStringLiteral("webCapabilities"), &capabilities);
	engine.rootContext()->setContextProperty(QStringLiteral("webDeviceTransport"), &transport);
	engine.rootContext()->setContextProperty(QStringLiteral("diveLog"), &diveLog);
	engine.rootContext()->setContextProperty(QStringLiteral("webPlanner"), &planner);
	engine.rootContext()->setContextProperty(QStringLiteral("webSync"), &sync);
	engine.load(QUrl::fromLocalFile(QStringLiteral(NEO_WEB_QML_PATH)));
	QVERIFY2(!engine.rootObjects().isEmpty(), "Main.qml did not load");
	auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().first());
	QVERIFY(window);
	window->setWidth(width);
	window->setHeight(height);
	window->show();
	QTest::qWait(100);
	QCOMPARE(window->property("compact").toBool(), compact);
	QObject *sidebar = window->findChild<QObject *>(QStringLiteral("sidebar"));
	QObject *header = window->findChild<QObject *>(QStringLiteral("mobileHeader"));
	QObject *bottom = window->findChild<QObject *>(QStringLiteral("bottomNav"));
	QVERIFY(sidebar && header && bottom);
	QCOMPARE(sidebar->property("visible").toBool(), !compact);
	QCOMPARE(header->property("visible").toBool(), compact);
	QCOMPARE(bottom->property("visible").toBool(), compact);
	QVERIFY(window->contentItem()->width() >= width);

	const QString directory = qEnvironmentVariable("NEO_SNAPSHOT_DIR", QDir::tempPath());
	QDir().mkpath(directory);
	const QImage snapshot = window->grabWindow();
	QVERIFY(!snapshot.isNull());
	QVERIFY(snapshot.save(QDir(directory).filePath(QStringLiteral("neo-%1x%2.png").arg(width).arg(height))));

	QVERIFY(QMetaObject::invokeMethod(window, "openNavigation", Q_ARG(QVariant, compact ? 4 : 5)));
	QTest::qWait(50);
	QCOMPARE(window->property("activePage").toInt(), 3);
	QObject *plannerPage = window->findChild<QObject *>(QStringLiteral("plannerPage"));
	QVERIFY(plannerPage && plannerPage->property("visible").toBool());
}

QTEST_MAIN(TestResponsiveQml)
#include "test_responsiveqml.moc"
