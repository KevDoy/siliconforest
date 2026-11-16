#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Target-only, finite installed-Firefox trace in a fresh isolated profile.

Caller holds gui.lock and bench and checks coord-gate immediately beforehand.
Uses the shared Firefox runner without changing its sources or installed files.
No pointer/key injection, default profile, preferences outside this run or signals
to processes outside the runner's identity-checked child family.
"""
import argparse
import json
import os
from pathlib import Path
import runpy
import socket
import subprocess
import sys
import types


def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--run',type=Path,required=True)
    p.add_argument('--profile-video',action='store_true')
    p.add_argument('--rate-control',action='store_true')
    p.add_argument('--staged-media',action='store_true')
    p.add_argument('--frame-mode',choices=['copy','require'],default='copy')
    p.add_argument('--without-trace',action='store_true')
    p.add_argument('--graphics-override',type=Path,help='FX-qualified complete cohort under the runtime overrides directory')
    p.add_argument('--no-native-compositor',action='store_true',help='Own-profile diagnostic: one full-window layer instead of native tile layers')
    p.add_argument('--timer-control',action='store_true',help='Own-process comparison using the existing PD_CV_TIMER escape hatch')
    a=p.parse_args()
    if a.frame_mode=='require' and not a.staged_media:
        raise ValueError('Strict sharing requires the reviewed staged VT producer')
    if a.graphics_override and (not a.staged_media or '..' in a.graphics_override.parts or
            not a.graphics_override.is_relative_to(Path('/usr/local/lib/siliconforest/firefox/160-system1/overrides'))):
        raise ValueError('Explicit reviewed runtime graphics cohort required')
    root=Path(__file__).resolve().parent
    private=Path('/private/var/db/sffx')
    if os.uname().sysname!='Darwin':raise ValueError('SiliconForest target only')
    if (not a.run.is_absolute() or '..' in a.run.parts or
            not a.run.is_relative_to(private) or a.run.exists()):
        raise ValueError('Fresh explicit private run required')
    library=root/'lib/SFMediaCadenceTrace.dylib'
    if not root.is_relative_to(private) or not library.is_file():
        raise ValueError('Cold-verified private trace packet required')
    # Current durable policy records input and missing markers, never waits for
    # owner idle. Keep this local adapter out of the shared Firefox runner.
    sys.path.insert(0,str(root))
    from owner_input import OwnerActivityRecorder
    class RecordOnlyGuard(OwnerActivityRecorder):
        def __init__(self,*args,require_idle=False,**kwargs):
            del require_idle
            super().__init__(*args,**kwargs)
    guard=types.ModuleType('owner_guard');guard.OwnerInputGuard=RecordOnlyGuard
    sys.modules['owner_guard']=guard
    tools=root/'tools';sys.path.insert(0,str(tools))
    # The shared runner defaults to one fixed port. Other diagnostic Firefox
    # families can exist: select a fresh loopback port for this profile instead.
    with socket.socket() as reservation:
        reservation.bind(('127.0.0.1',0))
        port=reservation.getsockname()[1]
    import marionette
    original_client=marionette.Marionette
    chosen_port=port
    class IsolatedClient(original_client):
        def __init__(self,port=None,timeout=10):
            if port is not None and port!=chosen_port:
                raise ValueError('A shared Marionette port is not allowed')
            super().__init__(port=chosen_port,timeout=timeout)

        def start(self,*args,**kwargs):
            result=super().start(*args,**kwargs)
            pid=int(result['capabilities']['moz:processID'])
            rows=subprocess.check_output(['ps','-axo','pid=,ppid=,lstart='],text=True)
            identities={int(p):(int(pp),start) for p,pp,start in
                        (row.split(None,2) for row in rows.splitlines())}
            current=pid;seen=set()
            while current in identities and current not in seen and current!=os.getpid():
                seen.add(current);current=identities[current][0]
            if current!=os.getpid():
                # No support/runtime/DOM command or Quit against another app.
                self.socket.close()
                raise RuntimeError('Marionette parent is outside this owned launch')
            self.owned_parent=pid
            self.owned_parent_start=identities[pid][1]
            return result

        def cpu_snapshot(self):
            import process_cpu
            rows=subprocess.check_output(['ps','-axo','pid=,ppid=,lstart='],text=True)
            identities={int(p):(int(pp),start) for p,pp,start in
                        (row.split(None,2) for row in rows.splitlines())}
            parent=self.owned_parent
            if parent not in identities or identities[parent][1]!=self.owned_parent_start:
                raise RuntimeError('Owned Firefox parent identity changed')
            owned={}
            for pid,(_,start) in identities.items():
                current=pid;seen=set()
                while current in identities and current not in seen and current!=parent:
                    seen.add(current);current=identities[current][0]
                if current==parent:owned[pid]=start
            return process_cpu.capture(self,parent,owned)

        def measure_video(self,handle=None):
            # Snapshot outside the fixture interval. No profiler, per-frame
            # sampling or dependency on zero counters being valid CPU time.
            import process_cpu
            cpu={}
            try:cpu['before']=self.cpu_snapshot()
            except Exception as error:cpu['before_error']=str(error)
            video=super().measure_video(handle)
            try:
                cpu['after']=self.cpu_snapshot()
                if 'before' in cpu:cpu['comparison']=process_cpu.compare(cpu['before'],cpu['after'])
            except Exception as error:cpu['after_error']=str(error)
            video['owned_process_cpu']=cpu
            if 'after' in cpu:
                import media_images
                directory=a.run/('media-provider-'+str(len(list(a.run.glob('media-provider-*')))))
                directory.mkdir()
                try:video['media_providers']=media_images.inspect(cpu['after']['processes'],directory,
                                    root/'Frameworks' if a.staged_media else None)
                except Exception as error:video['media_provider_error']=str(error)
                if a.staged_media:
                    try:
                        capture=subprocess.run(['/usr/bin/chonkstep-darwin-desktop','--screencapture',str(directory/'renderer.ppm')],
                                               capture_output=True,text=True,timeout=5,
                                               env=dict(os.environ,XDG_RUNTIME_DIR='/private/var/run/gpu-desktop',WAYLAND_DISPLAY='wayland-0'))
                        video['renderer_capture']=dict(returncode=capture.returncode,stdout=capture.stdout,stderr=capture.stderr,
                                                     file=str(directory/'renderer.ppm'),input_injected=False,
                                                     timing='After measurement; visibility/pixels still require inspection')
                    except Exception as error:video['renderer_capture_error']=str(error)
            return video
    marionette.Marionette=IsolatedClient
    if not a.without_trace:
        os.environ.update(SFFX_DIAGNOSTIC_OBSERVER=str(library),
                          SF_MEDIA_CADENCE_TRACE='1',SF_MEDIA_CADENCE_SECONDS='120')
    else:
        os.environ.pop('SFFX_DIAGNOSTIC_OBSERVER',None)
        os.environ.pop('SF_MEDIA_CADENCE_TRACE',None)
    runtime=Path('/usr/local/lib/siliconforest/firefox/160-system1')
    # Confirm the installed launcher supports the isolated-profile hook. A
    # missing hook is a refusal, never a claim that a trace was installed.
    launcher=Path('/Applications/Firefox.app/Contents/MacOS/firefox-sf')
    if not launcher.is_file() or 'SFFX_DIAGNOSTIC_OBSERVER' not in launcher.read_text():
        raise ValueError('Installed launcher lacks isolated diagnostic hook')
    manifest=json.loads((runtime/'release.json').read_text())
    if a.staged_media:os.environ['SF_VT_COPY_COUNTS']='1'
    if a.timer_control:os.environ['PD_CV_TIMER']='1'
    if a.graphics_override:os.environ['SF_MEDIA_GRAPHICS_OVERRIDE']=str(a.graphics_override)
    a.run.mkdir(parents=True)
    (a.run/'cadence-launch.json').write_text(json.dumps(dict(
        release=manifest,diagnostic_only=True,timing_qualified=False,
        synthetic_input=False,owner_idle_wait=False,rate_control=a.rate_control,
        staged_media=a.staged_media,frame_mode=a.frame_mode,boundary_trace=not a.without_trace,
        graphics_override=str(a.graphics_override) if a.graphics_override else None,
        native_compositor_disabled=a.no_native_compositor,
        cv_timer_control=a.timer_control,
        marionette_port=port,trace_library=str(library)),indent=2)+'\n')
    # The shared runner requires a nonexistent destination.
    destination=a.run/'browser'
    argv=[str(tools/'remote-run.py'),'--bundle','/Applications/Firefox.app',
          '--executable','firefox-sf','--run',str(destination),'--seconds','120',
          '--quiet-trace','--measure-all-videos','--page-load-strategy','none',
          '--sample-path','/private/var/db/sffx/probes-v3/sf-sample',
          '--expect-appkit-prefix','/System/Library/Frameworks/AppKit.framework',
          '--pref','media.autoplay.default=0','--pref','marionette.port='+str(port),
          '--pref','termsofuse.bypassNotification=true']
    argv+=['--vt-frame-mode',a.frame_mode]
    if a.no_native_compositor:argv+=['--pref','gfx.webrender.compositor=false']
    if (tools/'gpu-counters.json').is_file():
        argv+=['--gpu-transfer-spec',str(tools/'gpu-counters.json'),
               '--policy-probe-path',str(root/'bin/thread-policy-probe')]
    if a.staged_media:
        bundle=a.run/'Diagnostic.app'
        wrapper=bundle/'Contents/MacOS/firefox-sf'
        wrapper.parent.mkdir(parents=True)
        wrapper.write_text('#!/usr/bin/env python3\nimport os,sys\nos.execv(sys.executable,[sys.executable,'+
                           repr(str(root/'media-launch.py'))+']+sys.argv[1:])\n')
        wrapper.chmod(0o755)
        argv[argv.index('--bundle')+1]=str(bundle)
    if a.profile_video:argv+=['--profile-video']
    clips=([( 'pattern-1080p60-h264.mp4',rate) for rate in [0.5,1,2]] if a.rate_control else
           [('pattern-1080p60-h264.mp4',1),('pattern-4k30-hevc.mp4',1)])
    for clip,rate in clips:
        argv+=['--url','file://'+str(tools/'video-page.html')+
               '?src=file:///private/var/db/sffx/media-highrate-v1/'+clip+
               '&deferred=1&deadline=25000&rate='+str(rate)]
    sys.argv=argv
    runpy.run_path(str(tools/'remote-run.py'),run_name='__main__')


if __name__=='__main__':main()
