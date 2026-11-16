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
    a=p.parse_args()
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
            return result
    marionette.Marionette=IsolatedClient
    os.environ.update(SFFX_DIAGNOSTIC_OBSERVER=str(library),
                      SF_MEDIA_CADENCE_TRACE='1',SF_MEDIA_CADENCE_SECONDS='120')
    runtime=Path('/usr/local/lib/siliconforest/firefox/160-system1')
    # Confirm the installed launcher supports the isolated-profile hook. A
    # missing hook is a refusal, never a claim that a trace was installed.
    launcher=Path('/Applications/Firefox.app/Contents/MacOS/firefox-sf')
    if not launcher.is_file() or 'SFFX_DIAGNOSTIC_OBSERVER' not in launcher.read_text():
        raise ValueError('Installed launcher lacks isolated diagnostic hook')
    manifest=json.loads((runtime/'release.json').read_text())
    a.run.mkdir(parents=True)
    (a.run/'cadence-launch.json').write_text(json.dumps(dict(
        release=manifest,diagnostic_only=True,timing_qualified=False,
        synthetic_input=False,owner_idle_wait=False,rate_control=a.rate_control,
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
