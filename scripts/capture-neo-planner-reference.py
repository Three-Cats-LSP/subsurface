"""Capture native Windows planner evidence without approving new goldens.

Uses a checksum-verified package and a fresh profile with no cloud credentials.
The output is a Subsurface-engine regression reference, not external parity.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import uuid
import zipfile

ROOT = Path(__file__).resolve().parents[1]
DRIVER = r'''
    property var referenceCases: CASES
    property string referenceEvidence: OUTPUT_DIRECTORY
    Timer {
        interval: 250; repeat: true; running: true
        property bool done: false
        onTriggered: {
            if (!rootItem.initialized || done) return
            done = true
            try {
                CloudSync.autoSyncEnabled = false
                Backend.unit_system = Enums.METRIC
                for (var c of rootItem.referenceCases) {
                    // Display-unit setters map to the manifest's fixed native
                    // preferences. Read them back in the evidence as well.
                    Backend.planner_deco_mode = c.algorithm === 'vpmb' ? Enums.VPMB : c.algorithm === 'recreational' ? Enums.RECREATIONAL : Enums.BUEHLMANN
                    Backend.planner_gflow = c.gfLow === undefined ? 30 : c.gfLow
                    Backend.planner_gfhigh = c.gfHigh === undefined ? 75 : c.gfHigh
                    Backend.vpmb_conservatism = c.vpmbConservatism === undefined ? 0 : c.vpmbConservatism
                    Backend.descrate = 18
                    Backend.ascrate75 = 9; Backend.ascrate50 = 9
                    Backend.ascratestops = 9; Backend.ascratelast6m = 9
                    Backend.drop_stone_mode = false; Backend.last_stop6m = false
                    Backend.switch_at_req_stop = false; Backend.min_switch_duration = 1
                    Backend.surface_segment = 0; Backend.bottomsac = 20; Backend.decosac = 17
                    Backend.problemsolvingtime = 4; Backend.sacfactor = 40
                    // These setters accept bar while their getters display
                    // hundredths of bar, matching the actual planner controls.
                    Backend.o2narcotic = true; Backend.bottompo2 = 1.4; Backend.decopo2 = 1.6
                    Backend.safetystop = true; Backend.dobailout = false
                    Backend.doo2breaks = false; Backend.display_variations = false
                    var mode = c.mode === 'ccr' ? 1 : c.mode === 'pscr' ? 2 : 0
                    var cylinders = c.cylinders || [{type:'D12 232 bar', mix:c.mix || '21/0', pressure:200, use:mode === 1 ? 1 : 0}]
                    var segments = c.segments || [{depth:c.depthMeters, duration:c.durationMinutes, gas:0, setpoint:mode === 1 ? 1300 : 0, divemode:mode}]
                    var keys = ['planner_deco_mode','planner_gflow','planner_gfhigh','vpmb_conservatism','descrate','ascrate75','ascrate50','ascratestops','ascratelast6m','drop_stone_mode','last_stop6m','switch_at_req_stop','min_switch_duration','surface_segment','bottomsac','decosac','problemsolvingtime','sacfactor','o2narcotic','bottompo2','decopo2','safetystop','dobailout','doo2breaks','display_variations']
                    var effective = {}; for (var key of keys) effective[key] = Backend[key]
                    if (effective.bottompo2 !== 140 || effective.decopo2 !== 160) throw new Error('pO2 preference conversion mismatch')
                    var previousEvidence = null
                    if (c.previousDiveFile) {
                        // Import a fixed sampled prior dive in a fresh profile,
                        // then use the actual native tissue initialization.
                        if (c !== rootItem.referenceCases[rootItem.referenceCases.length - 1] || NeoDashboard.diveCount !== 0) throw new Error('Repetitive fixture is not isolated')
                        if (!manager.importDiveLogFile(c.previousDiveUrl)) throw new Error('Prior profile import failed')
                        manager.exportToFile(0, rootItem.referenceEvidence, false, false)
                        previousEvidence = {sha256:c.previousDiveSha256,surfaceIntervalSeconds:600}
                    }
                    var r = Backend.divePlannerPointsModel.calculatePlan(cylinders, segments, '2026-01-01', c.startTime || '12:00:00', mode, c.waterDensity || 10300, c.surfacePressureMbar || 1013, false)
                    if (!r.profile.length || r.planSaveAllowed !== c.planSaveAllowed) throw new Error('Unexpected validity: ' + c.id + ' ' + r.notes)
                    console.log('NEO_PLANNER_REFERENCE', JSON.stringify({id:c.id,effective:effective,runtimeSeconds:r.runtimeSeconds,otu:r.otu,planSaveAllowed:r.planSaveAllowed,exceedsNDL:r.exceedsNDL,schedule:r.schedule,gasAnalysis:r.gasAnalysis,timeline:r.timeline,previousDiveEvidence:previousEvidence}))
                }
                console.log('NEO_PLANNER_REFERENCE_PASS')
            } catch (error) { console.error('NEO_PLANNER_REFERENCE_FAIL', error) }
            Qt.quit()
        }
    }
'''


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--package', required=True, type=Path, help='Directory containing verified-package.json and app/')
    ap.add_argument('--manifest', type=Path, default=ROOT / 'tests/data/neo-planner-regression.json')
    ap.add_argument('--output', required=True, type=Path, help='New evidence directory; never overwritten')
    ap.add_argument('--case-id', help='Capture one scenario in its own fresh process/profile')
    args = ap.parse_args()
    assert os.name == 'nt', 'This capture uses the native Windows package'
    package = args.package.resolve()
    proof = json.loads((package / 'verified-package.json').read_text())
    exe = package / 'app/subsurface-mobile.exe'
    assert hashlib.sha256(exe.read_bytes()).hexdigest() == proof['executableSha256']
    revision = subprocess.check_output(['git', 'rev-parse', proof['source']], cwd=ROOT, text=True).strip()
    assert revision == proof['source']
    manifest = json.loads(args.manifest.read_text())
    cases = [c for c in manifest['scenarios'] if args.case_id is None or c['id'] == args.case_id]
    assert cases and len({x['id'] for x in cases}) == len(cases)
    for case in cases:
        if case.get('previousDiveFile'):
            prior = (ROOT / case['previousDiveFile']).resolve()
            assert prior.parent == ROOT / 'tests/data'
            assert hashlib.sha256(prior.read_bytes()).hexdigest() == case['previousDiveSha256']
            case['previousDiveUrl'] = prior.as_uri()
    out = args.output.resolve()
    out.mkdir(parents=True, exist_ok=False)
    (out / 'driver.py').write_bytes(Path(__file__).read_bytes())
    (out / 'input.json').write_text(json.dumps(manifest, indent=2))
    archive = out / 'source.zip'
    subprocess.run(['git', 'archive', '--format=zip', '--output=' + str(archive), revision,
                    'mobile-widgets/qml', 'map-widget/qml'], cwd=ROOT, check=True)
    with zipfile.ZipFile(archive) as z:
        z.extractall(out / 'source')  # Exact local tracked-source archive.
    qml = out / 'qml'
    shutil.copytree(out / 'source/mobile-widgets/qml', qml)
    shutil.copytree(out / 'source/map-widget/qml', qml, dirs_exist_ok=True)
    shutil.copyfile(out / 'source/map-widget/qml/MapWidgetQt6.qml', qml / 'MapWidget.qml')
    (qml / 'qmldir').write_text('')
    main = (qml / 'main.qml').read_text(encoding='utf-8')
    injected = DRIVER.replace('CASES', json.dumps(cases)).replace('OUTPUT_DIRECTORY', json.dumps('file://' + out.as_posix()))
    (qml / 'main.qml').write_text(main.rstrip()[:-1] + injected + '\n}\n', encoding='utf-8')
    profile = 'neo-planner-reference-' + uuid.uuid4().hex
    (out / 'profile.txt').write_text(profile)
    env = {k: v for k, v in os.environ.items() if not k.startswith('SUBSURFACE_NEO_') and k not in ('QT_PLUGIN_PATH', 'QML2_IMPORT_PATH', 'QML_IMPORT_PATH')}
    env.update(QT_QPA_PLATFORM='windows', QT_QUICK_BACKEND='software')
    with (out / 'native.log').open('w', encoding='utf-8') as log:
        process = subprocess.Popen([str(exe), '--user=' + profile, '--testqml=' + qml.as_uri()], cwd=exe.parent,
                                   env=env, stdout=log, stderr=subprocess.STDOUT, creationflags=subprocess.CREATE_NO_WINDOW)
        try:
            code = process.wait(timeout=240)
        finally:
            if process.poll() is None:
                process.kill()
                process.wait()
    log = (out / 'native.log').read_text(encoding='utf-8', errors='replace')
    assert code == 0 and 'NEO_PLANNER_REFERENCE_PASS' in log and not re.search(r'NEO_PLANNER_REFERENCE_FAIL|ReferenceError|TypeError|SyntaxError', log), 'Inspect retained native.log'
    captured = [json.loads(line.split('NEO_PLANNER_REFERENCE ', 1)[1]) for line in log.splitlines() if 'NEO_PLANNER_REFERENCE ' in line]
    assert [x['id'] for x in captured] == [x['id'] for x in cases]
    (out / 'capture.json').write_text(json.dumps(dict(source=revision, executableSha256=proof['executableSha256'], profile=profile, scenarios=captured), indent=2))
    print('Captured', len(captured), 'Windows native scenarios; review evidence before updating expected values.')


if __name__ == '__main__':
    main()
