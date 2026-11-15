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
    ap.add_argument('--gpu-surfaces', action='store_true', help='experimental actualGPU IOSurface allocation')
    ap.add_argument('--gpu-presentation', action='store_true', help='experimental private whole-window GPU layers')
    ap.add_argument('--software', action='store_true')
    ap.add_argument('--no-sandbox', action='store_true', help='diagnosis only; labelled in result')
    ap.add_argument('--insert-library', help='private diagnostic DSO (e.g. the real SBPL compiler)')
    ap.add_argument('--pref', action='append', default=[], help='NAME=JSON diagnostic preference')
    ap.add_argument('--catcher', help='external Mach exception catcher for this launch only')
    ap.add_argument('--surface-session', help='private named-IOSurface-broker session wrapper')
    ap.add_argument('--surface-broker', help='private standalone iosurfaced; requires --surface-session')
    ap.add_argument('--capture-pages', action='store_true', help='capture each owned browser tab after diagnostics')
    ap.add_argument('--page-load-strategy', choices=['normal', 'eager', 'none'], default='normal',
                    help='explicit WebDriver strategy; measurements still verify real page/media state')
    ap.add_argument('--quiet-trace', action='store_true', help='disable verbose media/GL/layer traces for timing runs')
    ap.add_argument('--io-policy', choices=['passthrough', 'utility', 'standard', 'default'], help='process-local diagnostic interposer mode')
    ap.add_argument('--measure-video', action='store_true')
    ap.add_argument('--measure-all-videos', action='store_true', help='measure each owned video tab, with separate CPU/AVD counters')
    ap.add_argument('--browse-stability', action='store_true', help='announced30minute ordinary-site browsing with abnormal-content-exit observer')
    ap.add_argument('--jit-probe', action='store_true', help='actual Baseline/Ion frames from a separate Gecko profile')
    ap.add_argument('--streaming', action='store_true', help='owned public non-DRM YouTube playback')
    ap.add_argument('--media-trace', action='store_true', help='decoder identity logs without GL/window traces')
    ap.add_argument('--quality', action='store_true', help='owned new/private windows and content download')
    ap.add_argument('--native-ui', action='store_true', help='finite synthetic native seat/menu checks on the owned Firefox PID')
    ap.add_argument('--benchmark', choices=['speedometer', 'motionmark', 'jetstream'])
    ap.add_argument('--measure-fixture', action='store_true')
    ap.add_argument('--scroll-fixture', action='store_true', help='labelled programmatic scroll on the owned fixture')
    ap.add_argument('--no-startup-cache', action='store_true', help='diagnostic only: fail startup-cache disk writes in this isolated run')
    ap.add_argument('--sample-parent', action='store_true', help='also sample this owned browser parent')
    ap.add_argument('--sample-path', help='private sf-sample; samples only owned GPU helpers')
    ap.add_argument('--capture-after', type=float, help='one early renderer capture, seconds after exec')
    args = ap.parse_args()
    if os.uname().sysname != 'Darwin':
        raise RuntimeError('Target must be SiliconForest')
    if bool(args.surface_session) != bool(args.surface_broker):
        raise ValueError('Supply both private surface-session and surface-broker paths')
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
        prefs['gfx.webrender.software'] = True
    for setting in args.pref:
        name, separator, value = setting.partition('=')
        if not name or not separator:
            raise ValueError('Expected --pref NAME=JSON')
        prefs[name] = json.loads(value)
    (profile / 'user.js').write_text(''.join('user_pref(%s, %s);\n' % (json.dumps(k), json.dumps(v))
                                          for k, v in prefs.items()))
    env = os.environ.copy()
    env.update(HOME=str(home), CFFIXED_USER_HOME=str(home), XDG_RUNTIME_DIR='/private/var/run/gpu-desktop',
               WAYLAND_DISPLAY='wayland-0', MOZ_CRASHREPORTER_DISABLE='1', MOZ_NO_REMOTE='1',
               MOZ_LOG='PlatformDecoderModule:5,AppleMedia:5,DMABuf:4',
               PD_WS_TIMING='1', PD_CGL_TRACE='1', PD_SANDBOX_TRACE='1', PD_CA_TRACE='1',
               MOZ_REMOTE_ALLOW_SYSTEM_ACCESS='1')
    if args.gpu_surfaces:
        env['SFFX_IOSURFACE_GPU'] = '1'
    if args.gpu_presentation:
        env['SFFX_GPU_PRESENT'] = '1'
    if args.quiet_trace:
        for name in ['MOZ_LOG', 'PD_WS_TIMING', 'PD_CGL_TRACE', 'PD_SANDBOX_TRACE', 'PD_CA_TRACE']:
            env.pop(name, None)
    if args.media_trace:
        env['MOZ_LOG'] = 'PlatformDecoderModule:5,AppleMedia:5'
    if args.io_policy:
        env['SFFX_IO_POLICY'] = args.io_policy
    if args.no_startup_cache:
        # Mozilla's supported cache-path override. The parent deliberately does
        # not exist, so writes fail promptly; profile data remains isolated.
        env['MOZ_STARTUP_CACHE'] = str(args.run / 'cache-disabled-parent/startupCache')
    if args.framework_path:
        env['DYLD_FRAMEWORK_PATH'] = args.framework_path
    if args.library_path:
        env['DYLD_LIBRARY_PATH'] = args.library_path
    if args.insert_library:
        env['DYLD_INSERT_LIBRARIES'] = args.insert_library
    if args.catcher:
        env['PD_NO_CRASH_REPORT'] = '1'
    if args.surface_session:
        env['SFFX_IOSURFACE_SERVICE'] = 'org.puredarwin.iosurfaced.sffx.' + str(os.getpid())
    if args.no_sandbox:
        for kind in ['CONTENT', 'GPU', 'RDD', 'SOCKET_PROCESS', 'UTILITY', 'GMP']:
            env['MOZ_DISABLE_' + kind + '_SANDBOX'] = '1'
    cmd = [str(args.bundle / 'Contents/MacOS/firefox'), '--no-remote', '--profile', str(profile),
           '--marionette'] + (args.url or ['about:support'])
    result = dict(command=cmd, seconds=args.seconds, software_requested=args.software,
                  sandbox_disabled=args.no_sandbox, gpu_surfaces=args.gpu_surfaces, gpu_presentation=args.gpu_presentation, synthetic_input=args.scroll_fixture or bool(args.benchmark) or args.native_ui, prefs=prefs,
                  diagnostic_library=args.insert_library, startup_cache_write_disabled=args.no_startup_cache,
                  io_policy=args.io_policy, verbose_trace=not args.quiet_trace)
    result['surface_session'] = dict(wrapper=args.surface_session, broker=args.surface_broker,
                                    service=env.get('SFFX_IOSURFACE_SERVICE'))
    owned = {}
    start = time.monotonic()
    early_captured = False
    gpu_sampled = False
    with (args.run / 'firefox.log').open('w') as log:
        launch = ([args.catcher] if args.catcher else []) + cmd
        if args.surface_session:
            launch = [args.surface_session, args.surface_broker] + launch
        result['launcher'] = launch
        p = subprocess.Popen(launch, env=env, cwd=args.bundle / 'Contents/MacOS',
                             stdout=log, stderr=subprocess.STDOUT, start_new_session=True)
        result['pid'] = p.pid
        support_attempted = False
        session_started = False
        automation_failed = False
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
                if args.capture_after is not None and not early_captured and time.monotonic() - start >= args.capture_after:
                    early_captured = True
                    capture = subprocess.run(['/usr/bin/chonkstep-darwin-desktop', '--screencapture',
                                              str(args.run / 'screen-early.png')],
                                             capture_output=True, text=True, timeout=15, env=env)
                    result['early_capture'] = dict(status=capture.returncode, stdout=capture.stdout,
                                                   stderr=capture.stderr, elapsed=time.monotonic() - start)
                if args.sample_path and not gpu_sampled and time.monotonic() - start > 18:
                    gpu_sampled = True
                    current_ids = process_identities()
                    for pid in sorted(owned):
                        if pid not in current_ids or current_ids[pid][1] != owned[pid]:
                            continue
                        command = subprocess.run(['ps', '-p', str(pid), '-o', 'command='], capture_output=True, text=True).stdout
                        browser_command = str(args.bundle / 'Contents/MacOS/firefox')
                        is_parent = args.sample_parent and command.startswith(browser_command)
                        if pid != p.pid and (is_parent or 'gpu-helper.app/Contents/MacOS/Firefox GPU Helper' in command):
                            sample = subprocess.run([args.sample_path, str(pid), '2', '20', '-file',
                                                     str(args.run / ('gpu-sample-%d.txt' % pid))],
                                                    capture_output=True, text=True, timeout=12)
                            result['gpu_sample'] = dict(pid=pid, status=sample.returncode, stderr=sample.stderr)
                if not support_attempted and time.monotonic() - start > 8:
                    try:
                        from marionette import Marionette
                        client = Marionette(timeout=1)
                    except (OSError, EOFError, ValueError, ImportError):
                        pass
                    else:
                        support_attempted = True
                        try:
                            client.socket.settimeout(max(1, min(45, deadline - time.monotonic())))
                            result['marionette'] = client.start(args.page_load_strategy)
                            session_started = True
                            result['automation'] = 'Marionette owned-profile diagnostics and measurements'
                            client.socket.settimeout(max(1, min(10, deadline - time.monotonic())))
                            (args.run / 'support.json').write_text(json.dumps(client.support(), indent=2) + '\n')
                            (args.run / 'runtime.json').write_text(json.dumps(client.runtime(), indent=2) + '\n')
                            def snapshot():
                                current = process_identities()
                                owned.update({pid: current[pid][1] for pid in descendants(p.pid, current)
                                              if pid in current})
                                active = [pid for pid in sorted(owned)
                                          if pid in current and current[pid][1] == owned[pid]]
                                if not active:
                                    return ''
                                return subprocess.run(['ps', '-p', ','.join(map(str, active)),
                                    '-o', 'pid,ppid,time,rss,stat,command'], capture_output=True, text=True).stdout
                            if args.benchmark:
                                from benchmark import measure
                                client.socket.settimeout(max(1, min(90, deadline - time.monotonic())))
                                result['benchmark'] = measure(client, args.benchmark, deadline, args.run / 'benchmark.json')
                                deadline = min(deadline, time.monotonic() + 2)
                            if args.browse_stability:
                                from stability import measure
                                client.socket.settimeout(max(1, min(25, deadline - time.monotonic())))
                                result['stability'] = measure(client, args.run / 'stability.json', deadline, snapshot)
                                deadline = min(deadline, time.monotonic() + 2)
                            if args.jit_probe:
                                from jit_probe import measure
                                client.socket.settimeout(max(1, min(30, deadline - time.monotonic())))
                                result['jit_probe'] = measure(client, args.run / 'jit-probe.json')
                                deadline = min(deadline, time.monotonic() + 2)
                            if args.streaming:
                                from streaming import measure
                                client.socket.settimeout(max(1, min(50, deadline - time.monotonic())))
                                result['streaming'] = measure(client, args.run / 'streaming.json', deadline)
                                deadline = min(deadline, time.monotonic() + 2)
                            if args.quality:
                                from quality import measure
                                result['quality'] = measure(client, args.run / 'quality.json', deadline)
                                deadline = min(deadline, time.monotonic() + 2)
                            if args.native_ui:
                                from native_ui import measure
                                result['native_ui'] = measure(client, args.run / 'native-ui.json', deadline)
                                deadline = min(deadline, time.monotonic() + 2)
                            if args.measure_fixture:
                                client.socket.settimeout(max(1, min(16, deadline - time.monotonic())))
                                result['fixture_cpu_before'] = dict(elapsed=time.monotonic()-start, ps=snapshot())
                                result['fixture_measurement'] = client.measure_fixture(args.scroll_fixture)
                                result['fixture_cpu_after'] = dict(elapsed=time.monotonic()-start, ps=snapshot())
                            if args.measure_video:
                                client.socket.settimeout(max(1, min(76, deadline - time.monotonic())))
                                result['video_cpu_before'] = dict(elapsed=time.monotonic()-start, ps=snapshot())
                                result['video_measurement'] = client.measure_video()
                                result['video_cpu_after'] = dict(elapsed=time.monotonic()-start, ps=snapshot())
                            if args.measure_all_videos:
                                result['video_measurements'] = []
                                for handle in client.video_handles():
                                    client.socket.settimeout(max(1, min(76, deadline - time.monotonic())))
                                    counters = lambda: subprocess.run(['sysctl', 'kern.pd_avd.stats'],
                                        capture_output=True, text=True).stdout.strip()
                                    entry = dict(cpu_before=dict(elapsed=time.monotonic()-start, ps=snapshot()),
                                                 avd_before=counters())
                                    result['video_measurements'].append(entry)
                                    entry['video'] = client.measure_video(handle)
                                    entry['avd_after'] = counters()
                                    entry['cpu_after'] = dict(elapsed=time.monotonic()-start, ps=snapshot())
                        except Exception as error:
                            result['marionette_error'] = str(error)
                            automation_failed = True
                            deadline = min(deadline, time.monotonic() + 2)
                        finally:
                            try:
                                if not session_started:
                                    raise RuntimeError('Session handshake incomplete; connection closed without further commands')
                                if automation_failed:
                                    raise RuntimeError('Automation failed; connection closed without further commands or stale replies')
                                client.socket.settimeout(5)
                                def capture_page(index, url):
                                    shot = args.run / ('page-%d.png' % index)
                                    captured = subprocess.run(['/usr/bin/chonkstep-darwin-desktop', '--screencapture', str(shot)],
                                        capture_output=True, text=True, timeout=15, env=env)
                                    result.setdefault('page_captures', []).append(dict(url=url, status=captured.returncode, path=str(shot)))
                                pages = client.pages(capture_page if args.capture_pages else None)
                                (args.run / 'pages.json').write_text(json.dumps(pages, indent=2) + '\n')
                            except Exception as error:
                                result['page_diagnostics_error'] = str(error)
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
        except BaseException as error:
            automation_failed = True
            result['interruption'] = repr(error)
            raise
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
            result['automation_complete'] = session_started and not automation_failed
            result['browser_survived_run'] = result.get('exit_before_cleanup') is None
            result['measurement_passed'] = result['automation_complete'] and result['browser_survived_run']
            for field in ['benchmark', 'native_ui', 'jit_probe', 'stability', 'quality', 'streaming']:
                if field in result:
                    result['measurement_passed'] &= bool(result[field].get('complete'))
            videos = [entry['video'] for entry in result.get('video_measurements', [])]
            if 'video_measurement' in result:
                videos.append(result['video_measurement'])
            for video in videos:
                result['measurement_passed'] &= bool(video.get('ended')) and not video.get('error') and not video.get('playError')
            (args.run / 'result.json').write_text(json.dumps(result, indent=2) + '\n')
    print(json.dumps({k: v for k, v in result.items() if k not in ['samples', 'prefs']}, indent=2))
    return 0 if result['measurement_passed'] else 1


if __name__ == '__main__':
    raise SystemExit(main())
