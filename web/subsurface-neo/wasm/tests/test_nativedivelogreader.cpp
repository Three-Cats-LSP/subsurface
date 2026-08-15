// SPDX-License-Identifier: GPL-2.0
#include "core/native-divelog-summary.h"
#include "core/native-profile-calculator.h"
#include "web/subsurface-neo/wasm/neowebdivelogmodel.h"

#include <QBuffer>
#include <QTemporaryFile>
#include <QtTest>

#include <cmath>

class TestNativeDiveLogReader : public QObject {
	Q_OBJECT

private slots:
	void readsNativeSummary();
	void rejectsForeignRoot();
	void filtersAndSelectsDives();
	void calculatesProfileValues();
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

void TestNativeDiveLogReader::filtersAndSelectsDives()
{
	QTemporaryFile file;
	QVERIFY(file.open());
	const QByteArray xml(R"xml(<divelog program="subsurface" version="3"><dives>
		<dive number="10" date="2024-05-01"><buddy>Alice</buddy><divecomputer dctype="OC"><sample time="20:00 min" depth="18.0 m" /></divecomputer></dive>
		<dive number="11" date="2025-06-02"><notes>Blue cave survey</notes><divecomputer dctype="CCR"><sample time="40:00 min" depth="30.0 m" /></divecomputer></dive>
		<dive number="12" date="2025-07-03"><buddy>Bob</buddy><cylinder o2="32.0%" /><divecomputer dctype="OC"><sample time="30:00 min" depth="24.0 m" /></divecomputer></dive>
	</dives></divelog>)xml");
	QCOMPARE(file.write(xml), qint64(xml.size()));
	file.flush();

	NeoWebDiveLogModel model;
	model.openLocalFile(QUrl::fromLocalFile(file.fileName()));
	QVERIFY(model.loaded());
	QCOMPARE(model.diveCount(), 3);
	QCOMPARE(model.filteredDives().size(), 3);
	QCOMPARE(model.availableYears(), QStringList({ QStringLiteral("2025"), QStringLiteral("2024") }));
	QCOMPARE(model.availableModes(), QStringList({ QStringLiteral("CCR"), QStringLiteral("OC") }));

	model.setYearFilter(QStringLiteral("2025"));
	QCOMPARE(model.filteredDives().size(), 2);
	model.setModeFilter(QStringLiteral("OC"));
	QCOMPARE(model.filteredDives().size(), 1);
	model.setSearchText(QStringLiteral("bob"));
	QCOMPARE(model.filteredDives().size(), 1);
	model.setSearchText(QStringLiteral("blue cave"));
	QCOMPARE(model.filteredDives().size(), 0);

	model.setYearFilter(QString());
	model.setModeFilter(QString());
	QCOMPARE(model.filteredDives().size(), 1);
	const int sourceIndex = model.filteredDives().first().toMap().value(QStringLiteral("sourceIndex")).toInt();
	model.selectDive(sourceIndex);
	QVERIFY(model.hasSelectedDive());
	QCOMPARE(model.selectedDive().value(QStringLiteral("number")).toInt(), 11);
	QCOMPARE(model.profileSamples().size(), 1);
}

void TestNativeDiveLogReader::calculatesProfileValues()
{
	QByteArray xml(R"xml(<divelog program="subsurface" version="3"><dives>
		<dive number="42" date="2026-08-01" time="09:00:00" duration="36:00 min">
			<cylinder description="AL80" o2="21.0%" />
			<divecomputer dctype="OC">
				<sample time="0:00 min" depth="0.0 m" />
				<sample time="3:00 min" depth="40.0 m" />
				<sample time="28:00 min" depth="40.0 m" />
				<sample time="31:00 min" depth="21.0 m" />
				<sample time="34:00 min" depth="6.0 m" />
				<sample time="36:00 min" depth="0.0 m" />
			</divecomputer>
		</dive>
	</dives></divelog>)xml");
	QBuffer buffer(&xml);
	QVERIFY(buffer.open(QIODevice::ReadOnly));
	native_divelog_summary summary = read_native_divelog_summary(buffer, QStringLiteral("profile.ssrf"));
	QVERIFY2(summary.ok, qPrintable(summary.error));

	QString error;
	QVERIFY2(calculate_native_profile(summary, 0, &error), qPrintable(error));
	QVERIFY(summary.dives[0].profile_calculated);
	QCOMPARE(summary.dives[0].samples.size(), 6);
	bool foundTts = false;
	for (const native_sample_summary &sample : summary.dives[0].samples) {
		QVERIFY(sample.has_current_gf);
		QVERIFY(sample.has_surface_gf);
		QVERIFY(sample.has_calculated_ceiling);
		QVERIFY(std::isfinite(sample.current_gf_percent));
		QVERIFY(std::isfinite(sample.surface_gf_percent));
		QVERIFY(sample.calculated_ceiling_m >= 0.0);
		foundTts = foundTts || sample.has_calculated_tts;
	}
	QVERIFY(foundTts);
}

QTEST_GUILESS_MAIN(TestNativeDiveLogReader)
#include "test_nativedivelogreader.moc"
