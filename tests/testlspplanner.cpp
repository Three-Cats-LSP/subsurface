// SPDX-License-Identifier: GPL-2.0
#include "testlspplanner.h"
#include "qt-models/lspplanner.h"
#include "qt-models/neoplanmetadata.h"
#include <QCryptographicHash>
#include <QFile>
#include <QJsonDocument>
#include <QTest>
#include <cmath>

namespace {
QJsonObject reference()
{
	QFile file(QStringLiteral(SUBSURFACE_TEST_DATA "/tests/data/neo-lsp-regression.json"));
	return file.open(QIODevice::ReadOnly) ? QJsonDocument::fromJson(file.readAll()).object() : QJsonObject();
}

QString compare(const QJsonValue &expected, const QJsonValue &actual, const QString &path)
{
	if (expected.type() != actual.type()) return path + QStringLiteral(": different JSON types");
	if (expected.isDouble()) {
		if (!std::isfinite(actual.toDouble()) || std::abs(expected.toDouble() - actual.toDouble()) > 1e-7)
			return path + QStringLiteral(": expected %1, got %2").arg(expected.toDouble(), 0, 'g', 17).arg(actual.toDouble(), 0, 'g', 17);
	} else if (expected.isObject()) {
		const auto e = expected.toObject(), a = actual.toObject();
		if (e.keys() != a.keys()) return path + QStringLiteral(": different object fields");
		for (auto it = e.begin(); it != e.end(); ++it) {
			const auto error = compare(it.value(), a.value(it.key()), path + QLatin1Char('.') + it.key());
			if (!error.isEmpty()) return error;
		}
	} else if (expected.isArray()) {
		const auto e = expected.toArray(), a = actual.toArray();
		if (e.size() != a.size()) return path + QStringLiteral(": different array lengths");
		for (int i = 0; i < e.size(); ++i) {
			const auto error = compare(e.at(i), a.at(i), path + QStringLiteral("[%1]").arg(i));
			if (!error.isEmpty()) return error;
		}
	} else if (expected != actual) return path + QStringLiteral(": different values");
	return {};
}
}

void TestLspPlanner::identityAndPresets()
{
	const auto id = LspPlanner::identity();
	QCOMPARE(id.value("id").toString(), QStringLiteral("lsp-d-planner-plus"));
	QCOMPARE(id.value("sourceCommit"), reference().value("sourceCommit"));
	QCOMPARE(id.value("version").toString(), QStringLiteral("3.2.0"));
	const auto presets = LspPlanner::presets();
	QCOMPARE(presets.size(), 6);
	const auto defaults = presets.at(0).toObject().value("values").toObject();
	QCOMPARE(defaults.value("waterVapor").toString(), QStringLiteral("0.0577"));
	QCOMPARE(defaults.value("decoTransitMode").toString(), QStringLiteral("multideco"));
	QCOMPARE(defaults.value("minStopTime").toString(), QStringLiteral("2"));
	const auto subsurface = presets.at(3).toObject().value("values").toObject();
	QCOMPARE(subsurface.value("heHalfTimeMode").toString(), QStringLiteral("buhl2003"));
	QCOMPARE(subsurface.value("stopRounding").toString(), QStringLiteral("fractional"));
}

void TestLspPlanner::upstreamReference_data()
{
	QTest::addColumn<QJsonObject>("fixture");
	const auto cases = reference().value("cases").toArray();
	QCOMPARE(cases.size(), 38);
	for (const auto &entry : cases) {
		const auto object = entry.toObject();
		QTest::newRow(qPrintable(object.value("id").toString())) << object;
	}
}

void TestLspPlanner::upstreamReference()
{
	QFETCH(QJsonObject, fixture);
	const auto request = fixture.value("request").toObject();
	const auto response = LspPlanner::calculate(request);
	QVERIFY2(response.contains("result"), qPrintable(QString::fromUtf8(QJsonDocument(response).toJson())));
	const auto expected = fixture.value("expectedUpstream").toObject();
	QVERIFY2(response.value("ok").toBool() == expected.value("error").isNull(),
		qPrintable(QString::fromUtf8(QJsonDocument(response).toJson(QJsonDocument::Compact))));
	const auto error = compare(expected, response.value("result"), fixture.value("id").toString());
	QVERIFY2(error.isEmpty(), qPrintable(error));
	QCOMPARE(response.value("effectiveRequest").toObject(), request);
	QCOMPARE(response.value("requestSha256").toString().toLatin1(),
		QCryptographicHash::hash(QJsonDocument(request).toJson(QJsonDocument::Compact), QCryptographicHash::Sha256).toHex());
}

void TestLspPlanner::invalidRequest_data()
{
	QTest::addColumn<int>("mode");
	const char *names[] = {"version", "missing-settings", "display-units", "unknown-model", "pressure", "zero-rate", "oversize-profile", "unknown-circuit", "null-gas", "damaged-tissues", "negative-interval", "reversed-gf", "oversize-request"};
	for (int i = 0; i < 13; ++i) QTest::newRow(names[i]) << i;
}

void TestLspPlanner::invalidRequest()
{
	QFETCH(int, mode);
	auto request = reference().value("cases").toArray().at(0).toObject().value("request").toObject();
	auto settings = request.value("settings").toObject();
	switch (mode) {
	case 0: request["schema"] = 2; break;
	case 1: settings = {}; break;
	case 2: settings["metric"] = false; break;
	case 3: request["model"] = "not-an-engine"; break;
	case 4: { auto e = request.value("environment").toObject(); e["altSurfaceP"] = -1; request["environment"] = e; break; }
	case 5: settings["descentRate"] = 0; break;
	case 6: { QJsonArray levels; for (int i = 0; i < 65; ++i) levels.append(request.value("levels").toArray().at(0)); request["levels"] = levels; break; }
	case 7: settings["circuit"] = "unknown"; break;
	case 8: request["decoGases"] = QJsonArray{QJsonValue(QJsonValue::Null)}; break;
	case 9: settings["_preTissues"] = QJsonArray{QJsonObject{{"pN2", -1}, {"pHe", 0}}}; break;
	case 10: { QJsonArray tissues; for (int i = 0; i < 16; ++i) tissues.append(QJsonObject{{"pN2", 0.75}, {"pHe", 0}}); settings["_preTissues"] = tissues; settings["_surfaceInterval"] = -1; break; }
	case 11: settings["gfLo"] = 90; settings["gfHi"] = 10; break;
	case 12: request["oversize"] = QString(70 * 1024, QLatin1Char('x')); break;
	}
	request["settings"] = settings;
	const auto result = LspPlanner::calculate(request);
	QVERIFY(!result.value("ok").toBool());
	if (result.contains("result")) {
		QVERIFY(result.value("result").toObject().value("plan").toArray().isEmpty());
		QVERIFY(!result.value("result").toObject().value("error").toString().isEmpty());
	} else QVERIFY(!result.value("error").toString().isEmpty());
}

void TestLspPlanner::freshEngineIsolation()
{
	const auto cases = reference().value("cases").toArray();
	const auto first = cases.at(0).toObject().value("request").toObject();
	const auto before = LspPlanner::calculate(first);
	for (const auto &entry : cases) {
		const auto fixture = entry.toObject();
		if (fixture.value("id").toString() == QLatin1String("CCR-REP") || fixture.value("id").toString() == QLatin1String("OC-altitude-freshwater"))
			QVERIFY(LspPlanner::calculate(fixture.value("request").toObject()).value("ok").toBool());
	}
	QCOMPARE(LspPlanner::calculate(first), before);
}

void TestLspPlanner::physicalProjection_data()
{
	QTest::addColumn<QJsonObject>("fixture");
	for(const auto &value:reference().value("cases").toArray()) {
		const auto fixture=value.toObject();
		if(fixture.value("expectedUpstream").toObject().value("error").isNull()) QTest::newRow(qPrintable(fixture.value("id").toString()))<<fixture;
	}
}

void TestLspPlanner::physicalProjection()
{
	QFETCH(QJsonObject, fixture);
	const auto request=fixture.value("request").toObject(), settings=request.value("settings").toObject();
	const bool loop=settings.value("circuit").toString()!=QLatin1String("OC")&&!settings.value("bailout").toBool();
	QJsonArray cylinders;
	auto add=[&](const QJsonObject &gas){
		for(const auto &value:cylinders){const auto known=value.toObject();if(known.value("o2")==gas.value("o2")&&known.value("he")==gas.value("he"))return;}
		cylinders.append(QJsonObject{{"o2",gas.value("o2")},{"he",gas.value("he")},{"sizeLitres",24},{"startBar",200},{"role",loop?1:0}});
	};
	for(const auto &gas:request.value("levels").toArray())add(gas.toObject());
	for(const auto &gas:request.value("decoGases").toArray())add(gas.toObject());
	if(loop&&settings.value("circuit").toString()==QLatin1String("CCR"))cylinders.append(QJsonObject{{"o2",100},{"he",0},{"sizeLitres",3},{"startBar",200},{"role",2}});
	const QJsonObject input{{"schema",1},{"engineRequest",request},{"cylinders",cylinders},{"priorProfiles",QJsonArray()},
		{"start",1000000},{"bottomSacLpm",22},{"decoSacLpm",20}};
	const auto response=LspPlanner::project(input);
	QVERIFY2(response.value("ok").toBool(),qPrintable(QString::fromUtf8(QJsonDocument(response).toJson(QJsonDocument::Compact))));
	const auto result=response.value("result").toObject();
	const auto profile=result.value("profile").toArray(), rows=result.value("timeline").toArray();
	QVERIFY(profile.size()>2);QCOMPARE(profile.first().toObject().value("depth").toInt(),0);QCOMPARE(profile.last().toObject().value("depth").toInt(),0);
	int seconds=0;
	for(const auto &value:rows){const auto row=value.toObject();QCOMPARE(row.value("runTime").toInt()-seconds,row.value("duration").toInt());seconds=row.value("runTime").toInt();}
	QCOMPARE(seconds,result.value("runtimeSeconds").toInt());
	QVERIFY(std::abs(seconds-fixture.value("expectedUpstream").toObject().value("totalRuntime").toDouble()*60)<=61);
	const auto error=compare(fixture.value("expectedUpstream"),result.value("raw"),fixture.value("id").toString());
	QVERIFY2(error.isEmpty(),qPrintable(error));
	QCOMPARE(result.value("gasVolumes").toArray().size(),cylinders.size());
	for(const auto &value:result.value("gasVolumes").toArray())QVERIFY(value.toObject().value("usedLitres").toDouble()>=0);
}

void TestLspPlanner::ndlReference()
{
	QJsonObject request;
	for(const auto &entry:reference().value("cases").toArray()) {
		const auto candidate=entry.toObject().value("request").toObject();
		if(candidate.value("model").toString()==QLatin1String("ZHLC_GF")&&candidate.value("settings").toObject().value("circuit").toString()==QLatin1String("OC")) {request=candidate;break;}
	}
	QVERIFY(!request.isEmpty());
	QJsonObject input{{"request",request},{"depthsMetres",QJsonArray{12,18,30}}};
	const auto result=LspPlanner::ndlReference(input);
	QVERIFY2(result.value("ok").toBool(),qPrintable(QString::fromUtf8(QJsonDocument(result).toJson())));
	const auto rows=result.value("result").toObject().value("rows").toArray();
	QCOMPARE(rows.size(),3);
	QVERIFY(rows[0].toObject().value("available").toBool());
	QVERIFY(rows[0].toObject().value("ndl").toInt()>rows[2].toObject().value("ndl").toInt());
	request["model"]="VPMB";input["request"]=request;
	const auto unavailable=LspPlanner::ndlReference(input);
	QVERIFY(unavailable.value("ok").toBool());
	QVERIFY(!unavailable.value("result").toObject().value("rows").toArray()[0].toObject().value("available").toBool());
	input["request"]=QJsonObject();
	QVERIFY(!LspPlanner::ndlReference(input).value("ok").toBool());
	input["request"]=request;input["depthsMetres"]=QJsonArray{-1};
	QVERIFY(!LspPlanner::ndlReference(input).value("ok").toBool());
}

void TestLspPlanner::readableDiveSettings_data()
{
	QTest::addColumn<int>("mode");
	const QStringList modes = {"old-saved-metric", "canonical-imperial-supplies", "ccr-trimix-and-oxygen", "vpm-conservatism", "hybrid-gf", "missing-request"};
	for (int i = 0; i < modes.size(); ++i) QTest::newRow(qPrintable(modes[i])) << i;
}
void TestLspPlanner::readableDiveSettings()
{
	QFETCH(int, mode);
	QFile fixture(QStringLiteral(SUBSURFACE_TEST_DATA "/tests/data/neo-lsp-report-v2.json")); QVERIFY(fixture.open(QIODevice::ReadOnly));
	auto metadata = QJsonDocument::fromJson(fixture.readAll()).toVariant().toMap();
	auto request = metadata.value("engineRequest").toMap(), input = metadata.value("input").toMap(), settings = request.value("settings").toMap();
	if (mode == 1) metadata["gasSupplies"] = QVariantList{QVariantMap{{"startBar", 200}, {"sizeLitres", 24}, {"role", 0}}};
	if (mode == 2) {
		settings["circuit"] = "CCR";
		input["cylinders"] = QVariantList{QVariantMap{{"mix", "18/45"}, {"use", 1}, {"type", "Twin 12 L"}}, QVariantMap{{"mix", "100/0"}, {"use", 2}, {"type", "3 L oxygen"}}, QVariantMap{{"mix", "50/0"}, {"use", 0}, {"type", "7 L bailout"}}};
	}
	if (mode == 3 || mode == 4) { request["model"] = mode == 3 ? "VPMBE" : "VPMB_GFS"; settings["conservatism"] = 3; }
	request["settings"] = settings; metadata["engineRequest"] = request; metadata["input"] = input;
	if (mode == 5) metadata.remove("engineRequest");
	const auto before = metadata;
	const auto report = neoLspDiveSettings(metadata, mode != 1, mode != 1, mode != 1);
	QCOMPARE(metadata, before); // Rendering an old plan must not rewrite it.
	if (mode == 5) { QVERIFY(report.isEmpty()); return; }
	for (const auto &label : {"DIVE SETTINGS", "Algorithm:", "Circuit:", "GASES", "Descent:", "Stop spacing:", "Gas consumption (SAC):", "Gas reserve:"})
		QVERIFY2(report.contains(QLatin1String(label)), label);
	for (const auto &internal : {"Resolved LSP", "Effective LSP", "Source:", "Engine:", "gfLo", "altSurfaceP", "{", "}"})
		QVERIFY2(!report.contains(QLatin1String(internal)), internal);
	if (mode == 1) { QVERIFY(report.contains("ft/min")); QVERIFY(report.contains("psi")); QVERIFY(report.contains("cu ft/min")); }
	else QVERIFY(report.contains("m/min"));
	if (mode == 2) {
		for (const auto &label : {"Closed circuit", "Setpoints:", "Trimix 18/45", "Diluent", "Loop oxygen", "Bailout gas", "Nitrox 50"}) QVERIFY2(report.contains(QLatin1String(label)), label);
	} else { QVERIFY(!report.contains("Setpoints:")); QVERIFY(report.contains("Air (21% oxygen)")); }
	if (mode == 3) { QVERIFY(report.contains("VPM-B/E")); QVERIFY(report.contains("Conservatism: 3")); QVERIFY(!report.contains("Gradient factors:")); }
	else QVERIFY(report.contains("Gradient factors: 20/85"));
	if (mode == 0) { QVERIFY(report.contains("fill 200 bar")); QVERIFY(report.contains("Ascent: 9.88 m/min")); }
}

QTEST_GUILESS_MAIN(TestLspPlanner)
