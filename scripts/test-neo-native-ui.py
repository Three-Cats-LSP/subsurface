#!/usr/bin/env python3
"""Exercise the native desktop QML shell with a disposable --user profile.

The first launch uses packaged QML. The following launches use the existing
--testqml development entry point with an instrumented copy of the same source.
No test hooks or sample data are added to the shipping application.
"""
import argparse
import json
import os
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
    function smokeCheck(value, message) {
        if (!value) throw new Error(message)
    }
    function smokeCapture(name) {
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
                        console.log("NEO_UI_PASS restart persistence")
                        smokeTimer.stop()
                        Qt.quit()
                        return
                    }
                    smokeCheck(NeoDashboard.diveCount === 0, "isolated empty log")
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
                    rootItem.openNeoDiveDetails(dive.id, false, false)
                    break
                case 6:
                    smokeCheck(pageStack.currentItem.objectName === "ModernDiveDetails", "dive detail route")
                    smokeCheck(pageStack.currentItem.navigationSection === "dives", "dive navigation scope")
                    smokeCheck(pageStack.currentItem.initialDiveId === NeoDashboard.recentDives[0].id, "canonical dive identity")
                    rootItem.showNeoHome()
                    subsurfaceTheme.currentTheme = "Dark"
                    rootItem.width = 390
                    break
                case 7:
                    if (!smokeCapture("modern-populated-dark-small")) return
                    rootItem.neoInterfaceStyle = "Clean"
                    break
                case 8:
                    smokeCheck(pageStack.currentItem === modernDiveList, "populated Clean return")
                    if (!smokeCapture("clean-populated-dark-small")) return
                    rootItem.width = 1100
                    subsurfaceTheme.currentTheme = "Blue"
                    break
                case 9:
                    if (!smokeCapture("clean-populated-light-wide")) return
                    rootItem.neoInterfaceStyle = "Modern"
                    neoAppearance.sync()
                    break
                case 10:
                    rootItem.openNeoDiveDetails(NeoDashboard.recentPlans[0].id, false, true)
                    break
                case 11:
                    smokeCheck(pageStack.currentItem.navigationSection === "plans", "saved-plan scope")
                    smokeCheck(pageStack.currentItem.initialDiveId === NeoDashboard.recentPlans[0].id, "saved-plan identity")
                    smokeCheck(pageStack.currentItem.previousScopedRow === -1 && pageStack.currentItem.nextScopedRow === -1, "plans do not navigate into dives")
                    rootItem.showNeoHome()
                    break
                case 12:
                    console.log("NEO_UI_PASS styles, themes, sizes, data, navigation")
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
    fixture = ET.parse(ROOT / "dives/neo-ui-regression.ssrf")
    plan = ET.SubElement(fixture.getroot().find("dives"), "dive", date="2026-10-11", time="09:00:00", duration="20:00 min")
    computer = ET.SubElement(plan, "divecomputer", model="planned dive")
    ET.SubElement(computer, "depth", max="18.0 m")
    for sample_time, depth in (("0:00 min", "0.0 m"), ("10:00 min", "18.0 m"), ("20:00 min", "0.0 m")):
        ET.SubElement(computer, "sample", time=sample_time, depth=depth)
    fixture_path = output / "fixture.ssrf"
    fixture.write(fixture_path, encoding="utf-8", xml_declaration=True)
    for restart in (False, True):
        config = dict(output=output.as_posix(), fixture=fixture_path.as_uri(),
                      expectedStyle="Modern" if restart else "Clean", restartOnly=restart)
        driver = DRIVER.replace("CONFIG", json.dumps(config))
        (qml / "main.qml").write_text(source.rstrip()[:-1] + driver + "\n}\n", encoding="utf-8")
        result = subprocess.run(base + ["--testqml=" + qml.as_uri()], cwd=executable.parent, env=env,
                                stdout=subprocess.PIPE, stderr=subprocess.STDOUT, timeout=120, creationflags=flags)
        text = result.stdout.decode("utf-8", errors="replace")
        (output / ("restart.log" if restart else "runtime.log")).write_text(text, encoding="utf-8")
        if result.returncode or "NEO_UI_PASS" not in text or "NEO_UI_FAIL" in text:
            raise RuntimeError(text[-12000:])
    if len(list(output.glob("*.png"))) != 8:
        raise RuntimeError("Expected eight rendered page screenshots")
    print(f"Native UI checks passed; logs and screenshots: {output}")


if __name__ == "__main__":
    main()
