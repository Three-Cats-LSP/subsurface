#!/usr/bin/env python3
"""Verify native Windows startup sync without a real account or external traffic.

Synthetic credentials and a manifest live under a unique --user profile. An
unreachable loopback proxy prevents provider access. The QML driver observes the
real initialization lifecycle; it never invokes sync itself or activates a window.
"""
import argparse
import ctypes
from ctypes import wintypes
import datetime
import json
import os
from pathlib import Path
import shutil
import socket
import subprocess
import uuid
import winreg

ROOT = Path(__file__).resolve().parents[1]
DRIVER = r'''
    property bool lifecycleStarted: false
    property bool lifecycleFinished: false
    property bool lifecycleTooEarly: false
    property int lifecycleTicks: 0
    property bool lifecycleExpected: EXPECTED
    // Keep foreground events from hiding a missing startup connection.
    onVisibleChanged: if (visible) rootItem.hide()
    Connections {
        target: CloudSync
        function onSyncInProgressChanged() {
            if (CloudSync.syncInProgress) {
                rootItem.lifecycleStarted = true
                if (!manager.initialized) rootItem.lifecycleTooEarly = true
            } else if (rootItem.lifecycleStarted) {
                rootItem.lifecycleFinished = true
            }
        }
    }
    Timer {
        interval: 250; repeat: true; running: true
        onTriggered: {
            if (!manager.initialized) return
            lifecycleTicks++
            if (lifecycleTooEarly || (!lifecycleExpected && lifecycleStarted)) {
                console.error("NEO_LIFECYCLE_FAIL unexpected automatic sync")
                stop(); Qt.quit(); return
            }
            if (lifecycleExpected && lifecycleFinished) {
                if (CloudSync.lastError.length && !CloudSync.syncInProgress && NeoDashboard.diveCount === 0)
                    console.log("NEO_LIFECYCLE_PASS startup sync attempted and failure released busy state")
                else
                    console.error("NEO_LIFECYCLE_FAIL incomplete error recovery or changed data")
                stop(); Qt.quit(); return
            }
            if (lifecycleTicks >= 48) {
                if (!lifecycleExpected)
                    console.log("NEO_LIFECYCLE_PASS disabled startup sync remained idle")
                else
                    console.error("NEO_LIFECYCLE_FAIL startup sync never completed")
                stop(); Qt.quit()
            }
        }
    }
'''


class Credential(ctypes.Structure):
    _fields_ = [('Flags', wintypes.DWORD), ('Type', wintypes.DWORD),
                ('TargetName', wintypes.LPWSTR), ('Comment', wintypes.LPWSTR),
                ('LastWritten', wintypes.FILETIME), ('CredentialBlobSize', wintypes.DWORD),
                ('CredentialBlob', ctypes.POINTER(ctypes.c_ubyte)), ('Persist', wintypes.DWORD),
                ('AttributeCount', wintypes.DWORD), ('Attributes', ctypes.c_void_p),
                ('TargetAlias', wintypes.LPWSTR), ('UserName', wintypes.LPWSTR)]


def write_synthetic_credential(api, target, payload):
    blob = json.dumps(payload).encode('utf-8')
    buffer = (ctypes.c_ubyte * len(blob)).from_buffer_copy(blob)
    credential = Credential()
    credential.Type = 1
    credential.TargetName = target
    credential.Persist = 2
    credential.CredentialBlobSize = len(blob)
    credential.CredentialBlob = ctypes.cast(buffer, ctypes.POINTER(ctypes.c_ubyte))
    if not api.CredWriteW(ctypes.byref(credential), 0):
        raise ctypes.WinError(ctypes.get_last_error())


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('executable', type=Path)
    parser.add_argument('output', type=Path)
    args = parser.parse_args()
    executable = args.executable.resolve()
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=False)
    profile = 'neo-cloud-lifecycle-' + uuid.uuid4().hex
    (output / 'profile.txt').write_text(profile, encoding='utf-8')
    prefix = 'SubsurfaceNeo/Cloud/profile/' + profile + '/'
    providers = ('google-drive', 'dropbox')
    targets = [prefix + key for provider in providers for key in (provider, 'sync-state-' + provider)]
    api = ctypes.WinDLL('advapi32', use_last_error=True)
    api.CredWriteW.argtypes = [ctypes.POINTER(Credential), wintypes.DWORD]
    api.CredWriteW.restype = wintypes.BOOL
    api.CredDeleteW.argtypes = [wintypes.LPCWSTR, wintypes.DWORD, wintypes.DWORD]
    api.CredDeleteW.restype = wintypes.BOOL
    qml = output / 'qml'
    shutil.copytree(ROOT / 'mobile-widgets/qml', qml)
    shutil.copytree(ROOT / 'map-widget/qml', qml, dirs_exist_ok=True)
    shutil.copyfile(ROOT / 'map-widget/qml/MapWidgetQt6.qml', qml / 'MapWidget.qml')
    (qml / 'qmldir').write_text('', encoding='utf-8')
    source = (qml / 'main.qml').read_text(encoding='utf-8')
    settings = 'Software\\Subsurface\\Subsurface-' + profile
    env = dict(os.environ, QT_QPA_PLATFORM='windows', QT_QUICK_BACKEND='software')
    for name in ('QT_PLUGIN_PATH', 'QML2_IMPORT_PATH', 'QML_IMPORT_PATH',
                 'SUBSURFACE_NEO_GOOGLE_CLIENT_FILE', 'SUBSURFACE_NEO_DROPBOX_CLIENT_ID'):
        env.pop(name, None)
    # Keep the port reserved but never listen, so only local refused connections
    # are possible throughout both runs, including their startup/update requests.
    with socket.socket() as proxy:
        proxy.bind(('127.0.0.1', 0))
        with winreg.CreateKey(winreg.HKEY_CURRENT_USER, settings + '\\Network') as key:
            for name, value, kind in [('proxy_type', 3, winreg.REG_DWORD),
                                      ('proxy_host', '127.0.0.1', winreg.REG_SZ),
                                      ('proxy_port', proxy.getsockname()[1], winreg.REG_DWORD)]:
                winreg.SetValueEx(key, name, 0, kind, value)
        try:
            expiry = datetime.datetime.now(datetime.timezone.utc) + datetime.timedelta(days=1)
            for index in range(0, len(targets), 2):
                write_synthetic_credential(api, targets[index], {
                    'access_token': 'synthetic-credential-no-provider-access',
                    'expires_at': expiry.isoformat(timespec='milliseconds').replace('+00:00', 'Z')})
                write_synthetic_credential(api, targets[index + 1], {
                    'schema': 1, 'revision': 'synthetic-baseline', 'sha256': '0' * 64,
                    'created_at': '2026-01-01T00:00:00.000Z'})
            for provider, enabled in (('google-drive', False), ('google-drive', True), ('dropbox', True)):
                with winreg.CreateKey(winreg.HKEY_CURRENT_USER, settings + '\\subsurface-neo\\cloud') as key:
                    winreg.SetValueEx(key, 'primary-provider', 0, winreg.REG_SZ, provider)
                    winreg.SetValueEx(key, 'auto-sync', 0, winreg.REG_DWORD, int(enabled))
                driver = DRIVER.replace('EXPECTED', 'true' if enabled and provider == 'google-drive' else 'false')
                (qml / 'main.qml').write_text(source.rstrip()[:-1] + driver + '\n}\n', encoding='utf-8')
                log_path = output / (provider + ('-enabled.log' if enabled else '-disabled.log'))
                with log_path.open('w', encoding='utf-8') as log:
                    result = subprocess.run([str(executable), '--user=' + profile, '--testqml=' + qml.as_uri()],
                                            cwd=executable.parent, env=env, stdout=log, stderr=subprocess.STDOUT,
                                            timeout=150, creationflags=subprocess.CREATE_NO_WINDOW)
                captured = log_path.read_text(encoding='utf-8', errors='replace')
                if result.returncode or 'NEO_LIFECYCLE_PASS' not in captured or 'NEO_LIFECYCLE_FAIL' in captured:
                    raise RuntimeError(f'Native cloud lifecycle failed; inspect {log_path}')
        finally:
            for target in targets:
                if not api.CredDeleteW(target, 1, 0) and ctypes.get_last_error() != 1168:
                    raise ctypes.WinError(ctypes.get_last_error())
    print(f'Native cloud lifecycle checks passed; evidence: {output}')


if __name__ == '__main__':
    main()
