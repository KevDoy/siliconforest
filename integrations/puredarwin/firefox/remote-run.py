#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Finite target-only Firefox diagnostic. Run under workstation gui.lock.

The bundle must have been copied pristine and hashed before this program runs.
Only processes descended from this launch are signalled, and their PIDs are saved.
"""
import argparse
import json
import os
from pathlib import Path
import signal
import subprocess
import time


def process_identities():
    rows = subprocess.check_output(['ps', '-axo', 'pid=,ppid=,lstart='], text=True)
    return {int(p): (int(pp), start) for p, pp, start in (r.split(None, 2) for r in rows.splitlines())}


def descendants(pid, identities):
    parents = {p: pp for p, (pp, _) in identities.items()}
    owned = {pid}
    while True:
        more = {p for p, pp in parents.items() if pp in owned}
        if more <= owned:
            return owned
        owned |= more


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--bundle', type=Path, required=True)
    ap.add_argument('--run', type=Path, required=True)
    ap.add_argument('--seconds', type=int, default=40)
    ap.add_argument('--url', action='append', default=[])
    ap.add_argument('--framework-path')
    ap.add_argument('--library-path')
    ap.add_argument('--software', action='store_true')
    ap.add_argument('--no-sandbox', action='store_true', help='diagnosis only; labelled in result')
    args = ap.parse_args()
    if os.uname().sysname != 'Darwin':
        raise RuntimeError('Target must be SiliconForest')
    args.run.mkdir(parents=True, exist_ok=False)
    profile = args.run / 'profile'
    profile.mkdir()
    home = args.run / 'home'
    home.mkdir()
    prefs = {'browser.shell.checkDefaultBrowser': False, 'browser.aboutwelcome.enabled': False,
             'browser.preonboarding.enabled': False, 'browser.startup.homepage_override.mstone': 'ignore',
             'startup.homepage_welcome_url': '', 'startup.homepage_welcome_url.additional': '',
             'datareporting.policy.dataSubmissionEnabled': False, 'toolkit.telemetry.enabled': False,
             'app.update.auto': False, 'marionette.port': 28290}
    if args.software:
        prefs.update({'gfx.webrender.software': True, 'layers.gpu-process.enabled': False})
    (profile / 'user.js').write_text(''.join('user_pref(%s, %s);\n' % (json.dumps(k), json.dumps(v))
                                          for k, v in prefs.items()))
    env = os.environ.copy()
    env.update(HOME=str(home), CFFIXED_USER_HOME=str(home), XDG_RUNTIME_DIR='/private/var/run/gpu-desktop',
               WAYLAND_DISPLAY='wayland-0', MOZ_CRASHREPORTER_DISABLE='1', MOZ_NO_REMOTE='1',
               MOZ_LOG='PlatformDecoderModule:5,AppleMedia:5,DMABuf:4',
               PD_WS_TIMING='1', PD_CGL_TRACE='1', PD_SANDBOX_TRACE='1')
    if args.framework_path:
        env['DYLD_FRAMEWORK_PATH'] = args.framework_path
    if args.library_path:
        env['DYLD_LIBRARY_PATH'] = args.library_path
    if args.no_sandbox:
        for kind in ['CONTENT', 'GPU', 'RDD', 'SOCKET_PROCESS', 'UTILITY', 'GMP']:
            env['MOZ_DISABLE_' + kind + '_SANDBOX'] = '1'
    cmd = [str(args.bundle / 'Contents/MacOS/firefox'), '--no-remote', '--profile', str(profile),
           '--marionette', '--remote-allow-system-access'] + (args.url or ['about:support'])
    result = dict(command=cmd, seconds=args.seconds, software_requested=args.software,
                  sandbox_disabled=args.no_sandbox, synthetic_input=False, prefs=prefs)
    owned = {}
    start = time.monotonic()
    with (args.run / 'firefox.log').open('w') as log:
        p = subprocess.Popen(cmd, env=env, cwd=args.bundle / 'Contents/MacOS',
                             stdout=log, stderr=subprocess.STDOUT, start_new_session=True)
        result['pid'] = p.pid
        support_attempted = False
        try:
            deadline = start + args.seconds
            samples = []
            while p.poll() is None and time.monotonic() < deadline:
                identities = process_identities()
                owned.update({pid: identities[pid][1] for pid in descendants(p.pid, identities)
                              if pid in identities})
                samples.append(dict(elapsed=time.monotonic() - start,
                                    ps=subprocess.run(['ps', '-p', ','.join(map(str, sorted(owned))),
                                                       '-o', 'pid,ppid,time,rss,stat,command'],
                                                      capture_output=True, text=True).stdout))
                if not support_attempted and time.monotonic() - start > 8:
                    try:
                        from marionette import Marionette
                        client = Marionette(timeout=1)
                    except (OSError, ImportError):
                        pass
                    else:
                        support_attempted = True
                        try:
                            client.socket.settimeout(15)
                            result['marionette'] = client.start()
                            result['automation'] = 'Marionette read-only diagnostics'
                            (args.run / 'support.json').write_text(json.dumps(client.support(), indent=2) + '\n')
                        except Exception as error:
                            result['marionette_error'] = str(error)
                        finally:
                            client.close()
                time.sleep(1)
            result['exit_before_cleanup'] = p.poll()
            result['elapsed_before_cleanup'] = time.monotonic() - start
            result['samples'] = samples
            if p.poll() is None:
                shot = args.run / 'screen.png'
                capture = subprocess.run(['/usr/bin/chonkstep-darwin-desktop', '--screencapture', str(shot)],
                                         capture_output=True, text=True, timeout=15, env=env)
                result['capture'] = dict(status=capture.returncode, stdout=capture.stdout, stderr=capture.stderr)
        finally:
            # Refresh ownership before signalling. No global name matching or pkill.
            if p.poll() is None:
                identities = process_identities()
                owned.update({pid: identities[pid][1] for pid in descendants(p.pid, identities)
                              if pid in identities})
            result['owned_pids'] = sorted(owned)
            for sig in [signal.SIGTERM, signal.SIGKILL]:
                identities = process_identities()
                for pid in sorted(owned, reverse=True):
                    if pid not in identities or identities[pid][1] != owned[pid]:
                        continue
                    try:
                        os.kill(pid, sig)
                    except ProcessLookupError:
                        pass
                time.sleep(1)
            try:
                result['exit_after_cleanup'] = p.wait(timeout=5)
            except subprocess.TimeoutExpired:
                result['cleanup_timeout'] = True
            (args.run / 'result.json').write_text(json.dumps(result, indent=2) + '\n')
    print(json.dumps({k: v for k, v in result.items() if k not in ['samples', 'prefs']}, indent=2))


if __name__ == '__main__':
    main()
