#!/usr/bin/env python3
"""Exercise the native desktop QML shell with a disposable --user profile.

The first launch uses packaged QML. The following launches use the existing
--testqml development entry point with an instrumented copy of the same source.
No test hooks or sample data are added to the shipping application.
"""
import argparse
import base64
import json
import os
import re
from pathlib import Path
import shutil
import subprocess
import time
import uuid
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parents[1]

DRIVER = r'''
    property int smokeStep: 0
    property string smokeLastCapture: ""
    property bool smokeCapturePending: false
    property var smokeConfig: CONFIG
    property int smokeLspRuntime: 0
    property var smokeLspProfile: []
    function smokeCheck(value, message) {
        if (!value) throw new Error(message)
    }
    function smokeFind(item, name) {
        if (item.objectName === name) return item
        var children = item.children || []
        for (var i = 0; i < children.length; ++i) {
            var found = smokeFind(children[i], name)
            if (found) return found
        }
        return null
    }
    function smokeCheckVpmb() {
        var previous = {units: Backend.unit_system, mode: Backend.planner_deco_mode,
                        conservatism: Backend.vpmb_conservatism, drop: Backend.drop_stone_mode}
        try {
            Backend.unit_system = Enums.METRIC
            Backend.planner_deco_mode = Enums.VPMB
            Backend.drop_stone_mode = false
            var cylinders = [{type: "D12 232 bar", mix: "21/0", pressure: 200, use: 0}]
            var segments = [{depth: 40, duration: 25, gas: 0, setpoint: 0, divemode: 0}]
            Backend.vpmb_conservatism = 0
            smokeCheck(Backend.vpmb_conservatism === 0, "VPM-B level 0 reaches calculation preferences")
            var relaxed = Backend.divePlannerPointsModel.calculatePlan(cylinders, segments, "2026-01-01", "12:00:00", 0, 10300, 1013, false)
            Backend.vpmb_conservatism = 3
            smokeCheck(Backend.vpmb_conservatism === 3, "VPM-B level 3 reaches calculation preferences")
            var conservative = Backend.divePlannerPointsModel.calculatePlan(cylinders, segments, "2026-01-01", "12:00:00", 0, 10300, 1013, false)
            smokeCheck(relaxed.planSaveAllowed && conservative.planSaveAllowed, "VPM-B profiles calculate")
            smokeCheck(conservative.runtimeSeconds > relaxed.runtimeSeconds, "VPM-B conservatism changes the native schedule")
        } finally {
            Backend.vpmb_conservatism = previous.conservatism
            Backend.planner_deco_mode = previous.mode
            Backend.drop_stone_mode = previous.drop
            Backend.unit_system = previous.units
        }
    }
    function smokeCheckCleanWidth() {
        var layout = smokeFind(modernDiveList, "NeoDiveListContent")
        smokeCheck(layout !== null, "Clean content layout")
        for (var i = 0; i < layout.children.length; ++i) {
            var child = layout.children[i]
            if (child.visible)
                smokeCheck(child.x >= -1 && child.x + child.width <= layout.width + 1,
                           "Clean child exceeds viewport: " + child)
        }
    }
    function smokeCapture(name) {
        if (pageStack.currentItem === modernDiveList) smokeCheckCleanWidth()
        if (smokeLastCapture === name) return true
        smokeLastCapture = name
        smokeCapturePending = true
        smokeStep--
        var requested = pageStack.currentItem.grabToImage(function(result) {
            if (!result.saveToFile(smokeConfig.output + "/" + name + ".png"))
                console.error("NEO_UI_FAIL screenshot " + name)
            smokeCapturePending = false
        })
        smokeCheck(requested, "page screenshot request")
        return false
    }
    Timer {
        id: smokeTimer
        interval: 900; repeat: true; running: true
        onTriggered: {
            if (!rootItem.initialized || rootItem.smokeCapturePending) return
            try {
                switch (rootItem.smokeStep++) {
                case 0:
                    smokeCheck(rootItem.neoInterfaceStyle === smokeConfig.expectedStyle, "style restart persistence")
                    if (smokeConfig.restartOnly) {
                        smokeCheck(NeoDashboard.diveCount === 12, "populated log restart")
                        smokeCheck(NeoDashboard.planCount === 2, "LSP saved-plan restart")
                        neoPlannerLab.loadPreset(0)
                        smokeCheck(neoPlannerLab.useLspPlanner && Math.abs(neoPlannerLab.lspParameters.ascentRate-9.876543)<1e-9, "full LSP preset restart")
                        rootItem.openNeoDiveDetails(NeoDashboard.recentPlans[0].id, false, true)
                        rootItem.smokeStep = 24
                        return
                    }
                    smokeCheck(NeoDashboard.diveCount === 0, "isolated empty log")
                    smokeCheckVpmb()
                    Backend.cloud_verification_status = Enums.CS_NOCLOUD
                    manager.openNoCloudRepo()
                    rootItem.neoInterfaceStyle = "Clean"
                    subsurfaceTheme.currentTheme = "Blue"
                    rootItem.width = 1100; rootItem.height = 760
                    rootItem.showNeoHome()
                    break
                case 1:
                    smokeCheck(pageStack.currentItem === modernDiveList, "Clean home")
                    smokeCheck(!neoModernDashboard.visible, "inactive dashboard hidden")
                    if (!smokeCapture("clean-empty-light-wide")) return
                    rootItem.neoInterfaceStyle = "Modern"
                    break
                case 2:
                    smokeCheck(pageStack.currentItem === neoModernDashboard, "Modern home")
                    if (!smokeCapture("modern-empty-light-wide")) return
                    subsurfaceTheme.currentTheme = "Dark"
                    rootItem.width = 390
                    break
                case 3:
                    if (!smokeCapture("modern-empty-dark-small")) return
                    rootItem.neoInterfaceStyle = "Clean"
                    break
                case 4:
                    smokeCheck(pageStack.currentItem === modernDiveList, "Clean return")
                    if (!smokeCapture("clean-empty-dark-small")) return
                    smokeCheck(manager.importDiveLogFile(smokeConfig.fixture), "canonical fixture import")
                    rootItem.neoInterfaceStyle = "Modern"
                    subsurfaceTheme.currentTheme = "Blue"
                    rootItem.width = 1100
                    break
                case 5:
                    smokeCheck(NeoDashboard.diveCount === 12, "real dive count")
                    var dive = NeoDashboard.recentDives[0]
                    smokeCheck(dive.number === 247 && dive.day === 28 && dive.year === 2026, "real date/number")
                    smokeCheck(dive.profile.length > 2 && dive.profile[0].time === 0, "real depth samples")
                    smokeCheck(NeoDashboard.planCount === 1, "plans excluded from dive count")
                    if (NeoDashboard.activityYear === 2026)
                        smokeCheck(NeoDashboard.monthlyActivity[6] === 2, "real July activity")
                    if (!smokeCapture("modern-populated-light-wide")) return
                    manager.exportToFile(0, smokeConfig.exportUrl, false, false)
                    manager.exportToFile(0, smokeConfig.exportLegacyUrl, false, false)
                    manager.exportToFile(0, smokeConfig.exportPath, false, false)
                    rootItem.openNeoDiveDetails(dive.id, false, false)
                    break
                case 6:
                    smokeCheck(pageStack.currentItem.objectName === "ModernDiveDetails", "dive detail route")
                    smokeCheck(pageStack.currentItem.navigationSection === "dives", "dive navigation scope")
                    smokeCheck(pageStack.currentItem.initialDiveId === NeoDashboard.recentDives[0].id, "canonical dive identity")
                    rootItem.openNeoDiveDetails(NeoDashboard.recentDives[0].id, true, false)
                    break
                case 7:
                    smokeCheck(pageStack.currentItem.objectName === "ModernDiveEditor", "dive editor route")
                    rootItem.showNeoHome()
                    subsurfaceTheme.currentTheme = "Dark"
                    rootItem.width = 390
                    break
                case 8:
                    if (!smokeCapture("modern-populated-dark-small")) return
                    rootItem.neoInterfaceStyle = "Clean"
                    break
                case 9:
                    smokeCheck(pageStack.currentItem === modernDiveList, "populated Clean return")
                    if (!smokeCapture("clean-populated-dark-small")) return
                    rootItem.width = 1100
                    subsurfaceTheme.currentTheme = "Blue"
                    break
                case 10:
                    if (!smokeCapture("clean-populated-light-wide")) return
                    rootItem.neoInterfaceStyle = "Modern"
                    neoAppearance.sync()
                    break
                case 11:
                    rootItem.openNeoDiveDetails(NeoDashboard.recentPlans[0].id, false, true)
                    break
                case 12:
                    smokeCheck(pageStack.currentItem.navigationSection === "plans", "saved-plan scope")
                    smokeCheck(pageStack.currentItem.initialDiveId === NeoDashboard.recentPlans[0].id, "saved-plan identity")
                    smokeCheck(pageStack.currentItem.previousScopedRow === -1 && pageStack.currentItem.nextScopedRow === -1, "plans do not navigate into dives")
                    rootItem.showNeoHome()
                    break
                case 13:
                    rootItem.showPageFromDrawer(neoPlannerLab)
                    Backend.unit_system = Enums.METRIC
                    neoPlannerLab.plannedDate = "2027-02-01"
                    neoPlannerLab.plannedTime = "12:00:00"
                    neoPlannerLab.neoSmokeInputs([{type:"D12 232 bar",mix:"21/0",pressure:200,use:0}], [{depth:30,duration:25,gas:0,setpoint:0,divemode:0}])
                    break
                case 14:
                    smokeCheck(pageStack.currentItem === neoPlannerLab, "shared planner route")
                    smokeCheck(neoPlannerLab.useLspPlanner && neoPlannerLab.builtInProfiles.length === 7, "LSP default and all six presets")
                    for (var pi = 1; pi < 7; ++pi) {
                        neoPlannerLab.loadBuiltInProfile(pi)
                        var original = neoPlannerLab.builtInProfiles[pi].parameters
                        for (var key in original) smokeCheck(String(neoPlannerLab.lspParameters[key]) === String(original[key]), "complete original preset " + key)
                        smokeCheck(neoPlannerLab.planSaveAllowed && neoPlannerLab.timeline.length > 2, "preset calculates a physical profile: " + pi)
                    }
                    neoPlannerLab.loadBuiltInProfile(1)
                    neoPlannerLab.setLspParameter("ascentRate", 9.876543)
                    neoPlannerLab.generatePlan(false, true)
                    neoPlannerLab.savePreset("LSP UI precision")
                    neoPlannerLab.loadBuiltInProfile(2)
                    neoPlannerLab.loadPreset(0)
                    smokeCheck(Math.abs(neoPlannerLab.lspParameters.ascentRate-9.876543)<1e-9, "preset retains precise SI rates")
					smokeCheck(neoPlannerLab.planSaveAllowed && neoPlannerLab.lastLspRequest.levels[0].depth===30, "preset retains owned gas and level rows: " + neoPlannerLab.planNotes)
                    smokeCheck(neoPlannerLab.fullPlanExport().indexOf("Algorithm:") >= 0 && neoPlannerLab.fullPlanExport().indexOf("GASES") >= 0, "readable calculated dive settings")
                    smokeCheck(neoPlannerLab.fullPlanExport().indexOf("Effective LSP") < 0 && neoPlannerLab.fullPlanExport().indexOf("Source:") < 0, "no internal settings dump in preview export")
                    smokeLspRuntime=neoPlannerLab.runtimeSeconds
                    smokeLspProfile=JSON.parse(JSON.stringify(neoPlannerLab.profileData))
                    smokeCheck(manager.exportNeoPlannerText(smokeConfig.lspPreview, neoPlannerLab.fullPlanExport()).length > 0, "LSP TXT export")
                    smokeCheck(manager.exportNeoPlannerPdf(smokeConfig.lspPreview, neoPlannerLab.fullPlanExport(), neoPlannerLab.profileData).length > 0, "LSP PDF export")
                    break
                case 15:
                    Backend.unit_system = Enums.IMPERIAL
                    break
                case 16:
                    neoPlannerLab.generatePlan(false, true)
                    smokeCheck(neoPlannerLab.planSaveAllowed, "imperial LSP plan: " + neoPlannerLab.planNotes)
                    smokeCheck(Math.abs(neoPlannerLab.lastLspRequest.levels[0].depth-30)<1e-7, "imperial depth preserves SI geometry")
                    smokeCheck(Math.abs(neoPlannerLab.lastLspRequest.settings.ascentRate-9.876543)<1e-9, "imperial rate preserves precise SI preset")
                    smokeCheck(Math.abs(neoPlannerLab.runtimeSeconds-smokeLspRuntime)<=1, "units preserve LSP runtime")
                    Backend.unit_system = Enums.METRIC
                    break
                case 17:
                    neoPlannerLab.selectDiveMode(1)
                    smokeCheck(!neoPlannerLab.planSaveAllowed, "CCR without complete supplies refuses save")
                    neoPlannerLab.neoSmokeInputs([{type:"D12 232 bar",mix:"21/0",pressure:200,use:1}, {type:"AL80",mix:"100/0",pressure:200,use:0}, {type:"D12 232 bar",mix:"21/0",pressure:200,use:0}], [{depth:30,duration:25,gas:0,setpoint:1300,divemode:1}])
                    var oxygenRole=smokeFind(neoPlannerLab,"NeoPlannerGasRole1")
                    smokeCheck(oxygenRole !== null, "actual gas-role control")
                    oxygenRole.currentIndex=2
                    oxygenRole.activated(2)
                    smokeCheck(neoPlannerLab.planSaveAllowed && neoPlannerLab.lastLspRequest.settings.circuit==="CCR", "CCR with explicit oxygen, diluent and bailout supplies: " + neoPlannerLab.planNotes)
                    neoPlannerLab.selectDiveMode(0)
                    neoPlannerLab.neoSmokeInputs([{type:"D12 232 bar",mix:"21/0",pressure:200,use:0}, {type:"AL80",mix:"50/0",pressure:200,use:0}], [{depth:30,duration:25,gas:0,setpoint:0,divemode:0}, {depth:18,duration:40,gas:0,setpoint:0,divemode:0}])
                    var segmentGas=smokeFind(neoPlannerLab,"NeoPlannerSegmentGas1")
                    segmentGas.currentIndex=1
                    segmentGas.activated(1)
                    smokeCheck(neoPlannerLab.planSaveAllowed && neoPlannerLab.lastLspRequest.levels[1].o2===50, "actual multilevel gas selection: " + neoPlannerLab.planNotes)
                    neoPlannerLab.neoSmokeInputs([{type:"D12 232 bar",mix:"80/80",pressure:200,use:0}], [{depth:30,duration:25,gas:0,setpoint:0,divemode:0}])
                    smokeCheck(!neoPlannerLab.planSaveAllowed && !smokeFind(neoPlannerLab,"NeoSavePlannerPlan").enabled, "invalid gas disables actual Save action")
                    neoPlannerLab.neoSmokeInputs([{type:"D12 232 bar",mix:"21/0",pressure:200,use:0}], [{depth:30,duration:25,gas:0,setpoint:0,divemode:0}])
                    neoPlannerLab.calculateNdlReference()
                    smokeCheck(neoPlannerLab.ndlReference.length === 6 && neoPlannerLab.ndlReference[0].available, "actual LSP NDL action")
                    neoPlannerLab.contingencyScenario=0
                    neoPlannerLab.contingencyDelta=5
                    neoPlannerLab.calculateContingency()
                    smokeCheck(neoPlannerLab.contingencyResult.planSaveAllowed && neoPlannerLab.contingencyResult.runtimeSeconds>neoPlannerLab.runtimeSeconds, "actual LSP contingency")
                    rootItem.neoInterfaceStyle = "Clean"
                    break
                case 18:
                    // Interface changes deliberately return home via Qt.callLater.
                    // Wait for the observed home, not an assumed timer interval:
                    // heavy synchronous calculations can delay the queued call.
                    if (pageStack.currentItem !== modernDiveList) { rootItem.smokeStep--; return }
                    rootItem.showPageFromDrawer(neoPlannerLab)
                    break
                case 19:
                    smokeCheck(pageStack.currentItem === neoPlannerLab && neoPlannerLab.useLspPlanner, "Clean uses the same LSP planner; current=" + (pageStack.currentItem ? pageStack.currentItem.objectName + ":" + pageStack.currentItem.title : "null") + "; LSP=" + neoPlannerLab.useLspPlanner + "; style=" + rootItem.neoInterfaceStyle)
                    if (!smokeCapture("clean-lsp-planner-wide")) return
                    rootItem.width=390
                    break
                case 20:
                    if (!smokeCapture("clean-lsp-planner-small")) return
                    smokeLspProfile=JSON.parse(JSON.stringify(neoPlannerLab.profileData))
                    smokeCheck(neoPlannerLab.planSaveAllowed, "actual LSP Save is available")
                    smokeFind(neoPlannerLab,"NeoSavePlannerPlan").clicked()
                    break
                case 21:
                    smokeCheck(NeoDashboard.planCount === 2, "actual LSP Save adds one plan")
                    manager.exportToFile(0, smokeConfig.lspLog, false, false)
                    rootItem.openNeoDiveDetails(NeoDashboard.recentPlans[0].id, false, true)
                    break
                case 22:
                    var saved=pageStack.currentItem.currentItem
                    smokeCheck(saved && saved.isLspPlan, "saved LSP identity selects its own view")
                    smokeCheck(JSON.stringify(saved.modelData.planProfile) === JSON.stringify(smokeLspProfile), "saved LSP chart matches calculation")
                    smokeCheck(saved.modelData.planReportHeader.indexOf("Algorithm:")>=0 && saved.modelData.planReportHeader.indexOf("GASES")>=0, "saved report shows dive settings and gases")
                    smokeCheck(saved.modelData.planReportHeader.indexOf("Resolved LSP")<0 && saved.modelData.planReportHeader.indexOf("Source:")<0, "saved report hides internal calculation dump")
                    smokeCheck(smokeFind(saved,"NeoSavedLspProfile").visible, "saved LSP chart rendered")
                    var report=pageStack.currentItem.planReportText(saved.modelData)
                    smokeCheck(manager.exportNeoPlannerText(smokeConfig.lspSaved, report).length>0, "reopened LSP TXT")
                    smokeCheck(manager.exportNeoPlannerPdf(smokeConfig.lspSaved, report, saved.modelData.planProfile).length>0, "reopened LSP PDF")
                    rootItem.width=1100
                    rootItem.neoInterfaceStyle="Modern"
                    neoAppearance.sync()
                    rootItem.showNeoHome()
                    break
                case 23:
                    console.log("NEO_UI_PASS styles, themes, sizes, data, navigation, LSP presets/units/save/reopen/export")
                    smokeTimer.stop()
                    Qt.quit()
                    break
                case 24:
                    var restored=pageStack.currentItem.currentItem
                    smokeCheck(restored && restored.isLspPlan, "LSP identity survives restart")
                    smokeCheck(restored.modelData.planReportHeader.indexOf("9.876543")>=0, "resolved LSP settings survive restart")
                    smokeCheck(restored.modelData.planTimeline.length>2 && restored.modelData.planProfile.length>2, "LSP schedule and geometry survive restart")
                    console.log("NEO_UI_PASS restart persistence and LSP plan")
                    smokeTimer.stop()
                    Qt.quit()
                    break
                }
            } catch (error) {
                console.error("NEO_UI_FAIL " + error)
                smokeTimer.stop()
                Qt.quit()
            }
        }
    }
'''


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("executable", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    executable = args.executable.resolve()
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=False)
    profile = "neo-ui-" + uuid.uuid4().hex
    (output / "profile.txt").write_text(profile, encoding="utf-8")
    env = dict(os.environ, QT_QPA_PLATFORM="windows" if os.name == "nt" else "offscreen", QT_QUICK_BACKEND="software")
    env.pop("QML2_IMPORT_PATH", None)
    env.pop("QT_PLUGIN_PATH", None)
    flags = subprocess.CREATE_NO_WINDOW if os.name == "nt" else 0
    base = [str(executable), "--user=" + profile, "-v"]
    # Test the actual embedded QML before introducing the runtime assertions.
    with (output / "packaged.log").open("w", encoding="utf-8") as log:
        process = subprocess.Popen(base, cwd=executable.parent, env=env, stdout=log, stderr=log, creationflags=flags)
        try:
            deadline = time.monotonic() + 90
            while time.monotonic() < deadline:
                if process.poll() is not None:
                    raise RuntimeError(f"Packaged application exited during startup: {process.returncode}")
                captured = (output / "packaged.log").read_text(encoding="utf-8", errors="replace")
                if "initialization completed" in captured:
                    break
                time.sleep(0.25)
        finally:
            process.terminate()
            process.wait(timeout=10)
    if "initialization completed" not in (output / "packaged.log").read_text(encoding="utf-8", errors="replace"):
        raise RuntimeError("Packaged application did not load main.qml; inspect packaged.log and native dialogs")
    qml = output / "qml"
    shutil.copytree(ROOT / "mobile-widgets/qml", qml)
    # The development qmldir points at sibling map-widget sources. Flatten
    # those entries into this disposable tree without changing product imports.
    shutil.copytree(ROOT / "map-widget/qml", qml, dirs_exist_ok=True)
    shutil.copyfile(ROOT / "map-widget/qml/MapWidgetQt6.qml", qml / "MapWidget.qml")
    (qml / "qmldir").write_text("", encoding="utf-8")
    source = (qml / "main.qml").read_text(encoding="utf-8")
    if os.environ.get("NEO_NATIVE_UI_TRACE_NAVIGATION") == "1":
        source = source.replace("function showNeoHome() {", "function showNeoHome() { console.log('NEO_HOME_TRACE step=' + rootItem.smokeStep); console.trace()")
        source = source.replace("function showPageFromDrawer(page) {", "function showPageFromDrawer(page) { console.log('NEO_PAGE_TRACE step=' + rootItem.smokeStep + ' page=' + page.objectName); console.trace()")
    # This input helper exists only in the disposable test copy. Calculation,
    # save, preset persistence, chart/report and restart all use product code.
    planner = qml / "modern/pages/ModernPlannerLab.qml"
    planner_source = planner.read_text(encoding="utf-8")
    planner.write_text(planner_source.rstrip()[:-1] + r'''
    function neoSmokeInputs(gasRows, levelRows) {
        cylinders.clear(); segments.clear()
        for (var gas of gasRows) cylinders.append(gas)
        for (var level of levelRows) segments.append(level)
        updateGasNames(); generatePlan(false, true)
    }
}
''', encoding="utf-8")
    fixture = ET.parse(ROOT / "dives/neo-ui-regression.ssrf")
    plan = ET.SubElement(fixture.getroot().find("dives"), "dive", date="2026-10-11", time="09:00:00", duration="20:00 min")
    computer = ET.SubElement(plan, "divecomputer", model="planned dive")
    ET.SubElement(computer, "depth", max="18.0 m")
    for sample_time, depth in (("0:00 min", "0.0 m"), ("10:00 min", "18.0 m"), ("20:00 min", "0.0 m")):
        ET.SubElement(computer, "sample", time=sample_time, depth=depth)
    fixture_path = output / "fixture.ssrf"
    fixture.write(fixture_path, encoding="utf-8", xml_declaration=True)
    exports = [output / name for name in ("standard URL export", "legacy URL export", "raw path export")]
    for folder in exports:
        folder.mkdir()
    lsp_folders = {name: output / name for name in ("lsp-preview", "lsp-saved", "lsp-log")}
    for folder in lsp_folders.values():
        folder.mkdir()
    for restart in (False, True):
        config = dict(output=output.as_posix(), fixture=fixture_path.as_uri(),
                      expectedStyle="Modern" if restart else "Clean", restartOnly=restart,
                      exportUrl=exports[0].as_uri(), exportLegacyUrl="file://" + exports[1].as_posix(),
                      exportPath=str(exports[2]), lspPreview=lsp_folders["lsp-preview"].as_uri(),
                      lspSaved=lsp_folders["lsp-saved"].as_uri(), lspLog=lsp_folders["lsp-log"].as_uri())
        driver = DRIVER.replace("CONFIG", json.dumps(config))
        (qml / "main.qml").write_text(source.rstrip()[:-1] + driver + "\n}\n", encoding="utf-8")
        log_path = output / ("restart.log" if restart else "runtime.log")
        with log_path.open("w", encoding="utf-8") as log:
            result = subprocess.run(base + ["--testqml=" + qml.as_uri()], cwd=executable.parent, env=env,
                                    stdout=log, stderr=subprocess.STDOUT, timeout=120, creationflags=flags)
        text = log_path.read_text(encoding="utf-8", errors="replace")
        if result.returncode or "NEO_UI_PASS" not in text or "NEO_UI_FAIL" in text or re.search(r"ModernDiveEditor.qml:[0-9]+:.*ReferenceError", text):
            raise RuntimeError(text[-12000:])
    if len(list(output.glob("*.png"))) != 10:
        raise RuntimeError("Expected ten rendered page screenshots, including the LSP shared planner")
    exported = [(folder / "Subsurface_export.ssrf").read_bytes() for folder in exports]
    if len(set(exported)) != 1 or len(ET.fromstring(exported[0]).findall(".//dive")) != 13:
        raise RuntimeError("Standard URL, legacy URL and raw-path exports must preserve all dives and the plan")
    lsp_xml = ET.parse(lsp_folders["lsp-log"] / "Subsurface_export.ssrf")
    metadata = []
    for notes in lsp_xml.findall(".//dive/notes"):
        match = re.search(r"<!--NEO_PLAN:([A-Za-z0-9_-]+)-->", notes.text or "")
        if match:
            encoded = match.group(1)
            value = json.loads(base64.urlsafe_b64decode(encoded + "=" * (-len(encoded) % 4)))
            if value.get("engine", {}).get("id") == "lsp-d-planner-plus":
                metadata.append(value)
    if len(metadata) != 1:
        raise RuntimeError("Expected one actual LSP-generated plan in the canonical log export")
    plan_data = metadata[0]
    if abs(plan_data["engineRequest"]["settings"]["ascentRate"] - 9.876543) > 1e-9:
        raise RuntimeError("LSP precise settings did not survive save/export")
    if plan_data["profile"][-1]["depth"] != 0 or plan_data["profile"][-1]["time"] != plan_data["runtimeSeconds"]:
        raise RuntimeError("Saved LSP profile must prove a complete physical return to surface")
    for name in ("lsp-preview", "lsp-saved"):
        texts = list(lsp_folders[name].glob("*.txt"))
        pdfs = list(lsp_folders[name].glob("*.pdf"))
        if len(texts) != 1 or len(pdfs) != 1 or not pdfs[0].read_bytes().startswith(b"%PDF-"):
            raise RuntimeError(f"Missing actual LSP TXT/PDF exports: {name}")
        contents = texts[0].read_text(encoding="utf-8")
        if not all(label in contents for label in ("Algorithm:", "Gradient factors:", "Descent:", "GASES", "Air")):
            raise RuntimeError(f"LSP source identity missing from {name}")
    (output / "lsp-saved-metadata.json").write_text(json.dumps(plan_data, indent=2), encoding="utf-8")
    print(f"Native UI checks passed; logs and screenshots: {output}")


if __name__ == "__main__":
    main()
