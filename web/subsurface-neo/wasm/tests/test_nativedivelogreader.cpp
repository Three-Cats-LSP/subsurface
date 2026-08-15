// SPDX-License-Identifier: GPL-2.0
#include "core/native-divelog-summary.h"

#include <QBuffer>
#include <QtTest>

class TestNativeDiveLogReader : public QObject {
	Q_OBJECT

private slots:
	void readsNativeSummary();
	void rejectsForeignRoot();
};

void TestNativeDiveLogReader::readsNativeSummary()
{
	QByteArray xml(R"xml(<?xml version="1.0"?>
<divelog program="subsurface" version="3">
  <divesites>
    <site uuid="1a2b3c4d" name="Izu Oceanic Park" />
  </divesites>
  <dives>
    <dive number="247" divesiteid="1a2b3c4d" date="2026-07-28" time="10:42:00" duration="48:00 min">
      <buddy>Dirk Hohndel</buddy>
      <notes>Clear water.</notes>
      <cylinder description="AL80" o2="32.0%" />
      <divecomputer dctype="CCR">
        <depth max="27.4 m" />
        <temperature water="22.0 C" />
        <sample time="0:30 min" depth="6.0 m" temp="22.0 C" pressure0="198.0 bar" ndl="45:00 min" cns="2%" po2="1.20 bar" />
        <sample time="1:00 min" depth="12.0 m" pressure0="194.0 bar" ndl="40:00 min" tts="3:00 min" in_deco="1" stopdepth="3.0 m" stoptime="1:00 min" />
      </divecomputer>
      <divecomputer model="backup"><sample time="1:00 min" depth="11.5 m" /></divecomputer>
    </dive>
    <dive number="248" date="2026-07-29" time="09:05:00">
      <divecomputer>
        <sample time="1:00 min" depth="10.0 m" temp="23.0 C" />
        <sample time="52:00 min" depth="36.8 m" />
      </divecomputer>
    </dive>
  </dives>
</divelog>)xml");
	QBuffer buffer(&xml);
	QVERIFY(buffer.open(QIODevice::ReadOnly));

	const native_divelog_summary summary = read_native_divelog_summary(buffer, QStringLiteral("test.ssrf"));
	QVERIFY2(summary.ok, qPrintable(summary.error));
	QCOMPARE(summary.dives.size(), 2);
	const native_dive_summary &first = summary.dives.at(0);
	QCOMPARE(first.number, 247);
	QCOMPARE(first.location, QStringLiteral("Izu Oceanic Park"));
	QCOMPARE(first.duration_seconds, 48 * 60);
	QCOMPARE(first.max_depth_m, 27.4);
	QCOMPARE(first.water_temperature_c, 22.0);
	QCOMPARE(first.gas, QStringLiteral("EAN32"));
	QCOMPARE(first.mode, QStringLiteral("CCR"));
	QCOMPARE(first.gear, QStringLiteral("AL80"));
	QCOMPARE(first.buddy, QStringLiteral("Dirk Hohndel"));
	QCOMPARE(first.samples.size(), 2);
	const native_sample_summary &sample = first.samples.at(1);
	QCOMPARE(sample.time_seconds, 60);
	QCOMPARE(sample.depth_m, 12.0);
	QCOMPARE(sample.temperature_c, 22.0);
	QCOMPARE(sample.pressure_bar, 194.0);
	QCOMPARE(sample.ndl_seconds, 40 * 60);
	QCOMPARE(sample.tts_seconds, 3 * 60);
	QCOMPARE(sample.stop_depth_m, 3.0);
	QCOMPARE(sample.stop_time_seconds, 60);
	QCOMPARE(sample.cns_percent, 2.0);
	QCOMPARE(sample.setpoint_bar, 1.2);
	QVERIFY(sample.in_deco);

	const native_dive_summary &second = summary.dives.at(1);
	QCOMPARE(second.duration_seconds, 52 * 60);
	QCOMPARE(second.max_depth_m, 36.8);
	QCOMPARE(second.water_temperature_c, 23.0);
	QCOMPARE(second.gas, QStringLiteral("Air"));
}

void TestNativeDiveLogReader::rejectsForeignRoot()
{
	QByteArray xml("<uddf version='3.2.2'></uddf>");
	QBuffer buffer(&xml);
	QVERIFY(buffer.open(QIODevice::ReadOnly));

	const native_divelog_summary summary = read_native_divelog_summary(buffer, QStringLiteral("foreign.uddf"));
	QVERIFY(!summary.ok);
	QVERIFY(summary.error.contains(QStringLiteral("not a native Subsurface XML")));
}

QTEST_GUILESS_MAIN(TestNativeDiveLogReader)
#include "test_nativedivelogreader.moc"
