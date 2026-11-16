#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Disconnect only an admitted own Firefox client while its audio is active.

No input injection, menu invocation, compositor restart or owner-profile access.
Server slot logs are observations; concurrent clients can make attribution
ambiguous, so they never automatically qualify an acoustic/audio-stop result.
"""
import argparse
import importlib.util
import json
import os
from pathlib import Path
import signal
import socket
import subprocess
import sys
import time


def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--run',type=Path,required=True);a=p.parse_args()
    root=Path(__file__).resolve().parent
    if os.uname().sysname!='Darwin' or not root.is_relative_to(Path('/private/var/db/sffx')):
        raise ValueError('Cold private SiliconForest packet required')
    if not a.run.is_absolute() or '..' in a.run.parts or not a.run.is_relative_to(root) or a.run.exists():
        raise ValueError('Fresh owned destination required')
    sys.path.insert(0,str(root/'tools'))
    from marionette import Marionette
    from lifecycle_observer import AudioFixture
    spec=importlib.util.spec_from_file_location('own_remote',root/'tools/remote-run.py')
    helpers=importlib.util.module_from_spec(spec);spec.loader.exec_module(helpers)
    a.run.mkdir();profile=a.run/'profile';profile.mkdir();home=a.run/'home';home.mkdir()
    with socket.socket() as reservation:
        reservation.bind(('127.0.0.1',0));port=reservation.getsockname()[1]
    prefs={'marionette.port':port,'media.autoplay.default':0,'termsofuse.bypassNotification':True,
           'browser.shell.checkDefaultBrowser':False,'browser.aboutwelcome.enabled':False,
           'browser.preonboarding.enabled':False,'app.update.auto':False,'browser.tabs.inTitlebar':0,
           'webgl.disable-angle':True,'dom.ipc.processPrelaunch.enabled':False}
    (profile/'user.js').write_text(''.join('user_pref(%s,%s);\n'%(json.dumps(k),json.dumps(v)) for k,v in prefs.items()))
    fixture=AudioFixture(a.run);owned={};client=None;process=None
    server=Path('/var/log/coreaudiod.log');offset=0;inode=None;server_rows=[]
    result=dict(synthetic_input=False,installed_outer_launcher=True,audio_stop_qualified=False,
                acoustic_stop_proved=False,cleanup_signals=[],timeline=[],port=port)
    def save():
        result['document']=fixture.snapshot();result['coreaudiod_observations']=server_rows
        (a.run/'result.json').write_text(json.dumps(result,indent=2)+'\n')
    def identities():
        current=helpers.process_identities()
        if process:
            owned.update({pid:current[pid][1] for pid in helpers.descendants(process.pid,current) if pid in current})
        return current
    def observe():
        nonlocal inode,offset
        current=identities();result['timeline'].append(dict(monotonic=time.monotonic(),epoch=time.time(),
            own_alive=[pid for pid,start in owned.items() if pid in current and current[pid][1]==start],
            launcher_exit=process.poll() if process else None))
        try:
            st=server.stat()
            if inode!=st.st_ino or st.st_size<offset:
                server_rows.append(dict(monotonic=time.monotonic(),rotation=True));inode=st.st_ino;offset=0
            with server.open('rb') as f:
                f.seek(offset);data=f.read();offset=f.tell()
            if data:server_rows.append(dict(monotonic=time.monotonic(),text=data.decode(errors='replace')))
        except OSError as e:result['server_log_error']=str(e)
    def pause(seconds):
        until=time.monotonic()+seconds
        while time.monotonic()<until:observe();time.sleep(.1)
    try:
        if server.exists():st=server.stat();inode=st.st_ino;offset=st.st_size
        result['coreaudiod_start']=dict(inode=inode,offset=offset)
        observers=[root/'lib/SFFirefoxMediaObserver.dylib',root/'lib/SFFirefoxWaylandLoss.dylib']
        if not all(path.is_file() for path in observers):raise ValueError('Both Linux-built observers required')
        env=os.environ.copy();env.update(SFFX_DIAGNOSTIC_OBSERVER=':'.join(map(str,observers)),
            SFFX_TEST_WAYLAND_LOSS='1',SF_FIREFOX_MEDIA_TRACE='1',SF_FIREFOX_MEDIA_TRACE_SECONDS='50',
            MOZ_REMOTE_ALLOW_SYSTEM_ACCESS='1',MOZ_NO_REMOTE='1',MOZ_CRASHREPORTER_DISABLE='1',
            HOME=str(home),CFFIXED_USER_HOME=str(home))
        command=['/Applications/Firefox.app/Contents/MacOS/firefox-sf','--no-remote','--profile',str(profile),'--marionette',fixture.url]
        with (a.run/'firefox.log').open('w') as out:
            process=subprocess.Popen(command,env=env,stdout=out,stderr=subprocess.STDOUT,start_new_session=True)
            result.update(command=command,launcher_pid=process.pid,launch_monotonic=time.monotonic())
            until=time.monotonic()+25
            while time.monotonic()<until:
                observe()
                if process.poll() is not None:raise RuntimeError('Own launcher exited before automation')
                try:client=Marionette(port=port,timeout=1);break
                except (OSError,EOFError):time.sleep(.2)
            if client is None:raise RuntimeError('Own automation port unavailable')
            client.socket.settimeout(12);session=client.start('none');parent=int(session['capabilities']['moz:processID']);current=identities()
            if parent not in owned or current[parent][1]!=owned[parent]:
                client.socket.close();client=None;raise RuntimeError('Refuse automation outside owned launch')
            result.update(parent_pid=parent,session=session)
            client.command('Marionette:SetContext',{'value':'content'})
            until=time.monotonic()+8
            while not client.command('WebDriver:ExecuteScript',dict(script='return typeof fxLifecycleStart === "function";',args=[],newSandbox=False)):
                if time.monotonic()>until:raise RuntimeError('Owned audio fixture not ready')
                pause(.1)
            result['audio_started']=client.command('WebDriver:ExecuteAsyncScript',dict(
                script='const done=arguments[arguments.length-1]; fxLifecycleStart().then(done,e=>done({error:String(e)}));',
                args=[],newSandbox=False,scriptTimeout=8000))
            if result['audio_started'].get('error') or result['audio_started'].get('paused') or result['audio_started'].get('muted'):
                raise RuntimeError('Unmuted own document did not start')
            pause(2);result['action_monotonic']=time.monotonic();result['action_epoch']=time.time()
            (a.run/'wayland-disconnect.trigger').write_text('Disconnect only this admitted own Firefox client\n')
            pause(8);result['exit_before_cleanup']=process.poll();result['before_cleanup_owned']=result['timeline'][-1]['own_alive']
            result['diagnostic_complete']=True;save()
    except Exception as error:result['error']=repr(error)
    finally:
        if client is not None:client.close()
        result['cleanup_monotonic']=time.monotonic()
        if process:
            for number in [signal.SIGTERM,signal.SIGKILL]:
                current=identities()
                for pid,start in sorted(owned.items(),reverse=True):
                    if pid in current and current[pid][1]==start:
                        try:os.kill(pid,number);result['cleanup_signals'].append(dict(pid=pid,signal=int(number)))
                        except ProcessLookupError:pass
                try:process.wait(timeout=2)
                except subprocess.TimeoutExpired:pass
            result['launcher_final_exit']=process.poll()
        observe();save();fixture.close()
    return int(bool(result.get('error') or not result.get('diagnostic_complete') or result.get('before_cleanup_owned')))


if __name__=='__main__':raise SystemExit(main())
