// SPDX-License-Identifier: GPL-2.0
#include "testcloudsyncmanifest.h"

#include "core/cloudsyncmanifest.h"

#include <QTest>

void TestCloudSyncManifest::testRoundTrip()
{
	const QByteArray payload("<divelog><dive number=\"1\"/></divelog>");
	const CloudSyncManifest original = CloudSyncManifest::forPayload(payload, "previous-payload");
	QVERIFY(original.isValid());
	QCOMPARE(original.payloadSha256,
		 CloudSyncManifest::sha256(payload));
	QCOMPARE(original.parentSha256, QString("previous-payload"));

	QString error;
	const CloudSyncManifest decoded = CloudSyncManifest::fromJson(original.toJson(), &error);
	QVERIFY2(error.isEmpty(), qPrintable(error));
	QVERIFY(decoded.isValid());
	QCOMPARE(decoded.schemaVersion, original.schemaVersion);
	QCOMPARE(decoded.revisionId, original.revisionId);
	QCOMPARE(decoded.payloadSha256, original.payloadSha256);
	QCOMPARE(decoded.parentSha256, original.parentSha256);
	QCOMPARE(decoded.createdAtUtc.toUTC(), original.createdAtUtc.toUTC());
}

void TestCloudSyncManifest::testRejectsInvalidManifest()
{
	QString error;
	QVERIFY(!CloudSyncManifest::fromJson("not json", &error).isValid());
	QVERIFY(!error.isEmpty());

	error.clear();
	QVERIFY(!CloudSyncManifest::fromJson("{\"schema\": 99}", &error).isValid());
	QVERIFY(!error.isEmpty());

	CloudSyncManifest incomplete;
	incomplete.revisionId = "revision";
	incomplete.payloadSha256 = "hash";
	QVERIFY(!incomplete.isValid());
}

void TestCloudSyncManifest::testStateRelation_data()
{
	QTest::addColumn<QString>("local");
	QTest::addColumn<QString>("cloud");
	QTest::addColumn<QString>("previous");
	QTest::addColumn<int>("expected");

	QTest::newRow("identical") << "same" << "same" << "previous" << static_cast<int>(CloudSyncRelation::Identical);
	QTest::newRow("local only") << "local" << "previous" << "previous" << static_cast<int>(CloudSyncRelation::LocalOnlyChanged);
	QTest::newRow("cloud only") << "previous" << "cloud" << "previous" << static_cast<int>(CloudSyncRelation::CloudOnlyChanged);
	QTest::newRow("conflict") << "local" << "cloud" << "previous" << static_cast<int>(CloudSyncRelation::Conflict);
	QTest::newRow("first sync") << "local" << "cloud" << "" << static_cast<int>(CloudSyncRelation::Unknown);
	QTest::newRow("missing local") << "" << "cloud" << "previous" << static_cast<int>(CloudSyncRelation::Unknown);
	QTest::newRow("missing cloud") << "local" << "" << "previous" << static_cast<int>(CloudSyncRelation::Unknown);
}

void TestCloudSyncManifest::testStateRelation()
{
	QFETCH(QString, local);
	QFETCH(QString, cloud);
	QFETCH(QString, previous);
	QFETCH(int, expected);
	QCOMPARE(static_cast<int>(compareCloudSyncState(local, cloud, previous)), expected);
}

QTEST_GUILESS_MAIN(TestCloudSyncManifest)
