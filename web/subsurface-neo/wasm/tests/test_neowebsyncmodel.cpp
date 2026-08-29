// SPDX-License-Identifier: GPL-2.0
#include "web/subsurface-neo/wasm/neowebsyncmodel.h"
#include <QtTest>

class TestNeoWebSyncModel : public QObject {
	Q_OBJECT
private slots:
	void classifiesManifestChanges();
	void requiresExplicitConflictResolution();
};

void TestNeoWebSyncModel::classifiesManifestChanges()
{
	NeoWebSyncModel model;
	model.evaluate(3, QStringLiteral("aaa"), 2, QStringLiteral("bbb"));
	QCOMPARE(model.state(), QStringLiteral("upload-ready"));
	model.evaluate(3, QStringLiteral("aaa"), 4, QStringLiteral("bbb"));
	QCOMPARE(model.state(), QStringLiteral("download-ready"));
	model.evaluate(4, QStringLiteral("same"), 9, QStringLiteral("same"));
	QCOMPARE(model.state(), QStringLiteral("up-to-date"));
}

void TestNeoWebSyncModel::requiresExplicitConflictResolution()
{
	NeoWebSyncModel model;
	model.evaluate(5, QStringLiteral("local"), 5, QStringLiteral("remote"));
	QVERIFY(model.conflict());
	QVERIFY(!model.actionReady());
	model.keepRemote();
	QCOMPARE(model.state(), QStringLiteral("download-ready"));
	QVERIFY(model.actionReady());
}

QTEST_GUILESS_MAIN(TestNeoWebSyncModel)
#include "test_neowebsyncmodel.moc"
