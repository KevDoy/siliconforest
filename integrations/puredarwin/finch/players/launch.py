#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Finite staged VLC launch through open -a. No screen input; optional same-uid capture."""
import argparse,json,os,subprocess,time,pwd,hashlib,re,ctypes,socket
from pathlib import Path


def main():
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--framework-prefix',action='store_true',help='Use only reviewed Frameworks/lib inside this stage')
    ap.add_argument('--hardware',action='store_true',help='Prefer actual VideoToolbox with its require-hardware option')
    ap.add_argument('--capture',action='store_true')
    ap.add_argument('--no-statusicon',action='store_true',help='Disable VLC optional status-menu icon while AppKit lacks its backend')
    ap.add_argument('--as-user',help='Existing unprivileged account; no account or global privilege changes')
    ap.add_argument('--audio-prefix',action='store_true',help='Use the reviewed AudioToolbox-only prefix inside this stage')
    ap.add_argument('--headless',action='store_true',help='VLC dummy interface/video plus real auhal; no GUI evidence')
    a=ap.parse_args()
    if a.headless and a.capture:raise ValueError('A headless run cannot produce a player-window capture')
    work=Path(__file__).resolve().parent
    if os.uname().sysname!='Darwin' or not str(work).startswith('/private/var/db/sffinch/'):
        raise ValueError('Run only from the reviewed M1 staging directory')
    env={k:v for k,v in os.environ.items() if not k.startswith('DYLD_')}
    env.update(XDG_RUNTIME_DIR='/private/var/run/gpu-desktop',WAYLAND_DISPLAY='wayland-0',SF_METAL_ENABLE_REAL_DEVICE='0')
    if a.framework_prefix:
        if not (work/'Frameworks').is_dir():raise ValueError('Missing own Frameworks prefix')
        env['DYLD_FRAMEWORK_PATH']=str(work/'Frameworks')
        if (work/'lib').is_dir():env['DYLD_LIBRARY_PATH']=str(work/'lib')
    if a.audio_prefix:
        prefix=work/'audio-prefix/System/Library/Frameworks'
        if not (prefix/'AudioToolbox.framework/AudioToolbox').is_file():
            raise ValueError('Missing reviewed own AudioToolbox prefix')
        env['DYLD_FRAMEWORK_PATH']=str(prefix)
    for record in json.loads((work/'bundle-hashes.json').read_text()):
        path=work/record['path']
        if not path.resolve().is_relative_to(work):raise ValueError('Bundle path escapes own stage')
        if hashlib.file_digest(path.open('rb'),'sha256').hexdigest()!=record['sha256']:
            raise ValueError('Cold bundle hash mismatch: '+record['path'])
    drop=None
    state=work/'state';state.mkdir(exist_ok=True)
    if a.as_user:
        account=pwd.getpwnam(a.as_user)
        if account.pw_uid==0:raise ValueError('VLC requires an unprivileged account')
        os.chown(state,account.pw_uid,account.pw_gid);state.chmod(0o700)
        def drop():
            os.setgroups([]);os.setgid(account.pw_gid);os.setuid(account.pw_uid)
    argv=['open' ,'-a',str(work/'Applications/VLC.app'),'--args','--ignore-config','--no-plugins-cache',
          '--no-media-library','--no-metadata-network-access','--play-and-exit',
          '-vv',str(work/'fixtures/movie.mov')]
    if not a.headless:argv[4:4]=['--intf','macosx']
    if a.no_statusicon:argv[4:4]=['--no-macosx-statusicon']
    if a.headless:argv[4:4]=['--intf','dummy','--extraintf','oldrc','--rc-unix',str(state/'rc.sock'),'--rc-fake-tty','--vout','dummy','--aout','auhal','--codec','avcodec']
    if a.hardware:
        if a.headless:argv[argv.index('avcodec')]='videotoolbox,avcodec'
        else:argv[4:4]=['--codec','videotoolbox,avcodec']
        argv[4:4]=['--videotoolbox-hw-decoder-only']
    env['SF_VLC_LOG']=str(state/'vlc.log')
    env['SF_VLC_HAL_LOG']='1'
    if a.headless:env['SF_VLC_HEADLESS_TIMEOUT']='1'
    with (work/'open.stdout').open('w') as out,(work/'open.stderr').open('w') as err:
        r=subprocess.run(argv,env=env,stdout=out,stderr=err,timeout=15,preexec_fn=drop)
    rc_stats=[]
    if a.headless:
        # Read the actual vendor control interface's statistics. This is an
        # owned CLI socket, no screen input and no playback-changing command.
        deadline=time.monotonic()+22
        while time.monotonic()<deadline:
            if (state/'rc.sock').exists() and len(rc_stats)<12:
                try:
                    with socket.socket(socket.AF_UNIX,socket.SOCK_STREAM) as control:
                        control.settimeout(0.2);control.connect(str(state/'rc.sock'))
                        control.sendall(b'stats\n');data=bytearray()
                        while len(data)<65536:
                            try:
                                block=control.recv(min(8192,65536-len(data)))
                            except socket.timeout:break
                            if not block:break
                            data.extend(block)
                        rc_stats.append({'elapsed_until_deadline':round(deadline-time.monotonic(),3),'stdout':data.decode(errors='replace')})
                except OSError:pass
            time.sleep(0.25)
    else:
        time.sleep(8)
    listing=subprocess.run(['ps','-A','-o','pid=','-o','args='],capture_output=True,text=True,check=True).stdout
    own=[line for line in listing.splitlines() if str(work/'Applications/VLC.app/Contents/MacOS/VLC') in line]
    entry_pid=None;entry_identity=None
    logfile=state/'vlc.log'
    match=re.search(r'SF_VLC_ENTRY pid=(\d+)',logfile.read_text(errors='replace') if logfile.exists() else '')
    if match:
        entry_pid=int(match[1])
        row=next((line for line in listing.splitlines() if line.strip().split(None,1)[0]==str(entry_pid)),None)
        entry_identity={'pid':entry_pid,'ps_line':row,'path':None}
        try:
            lib=ctypes.CDLL('/usr/lib/libSystem.B.dylib');lib.proc_pidpath.argtypes=[ctypes.c_int,ctypes.c_void_p,ctypes.c_uint32];lib.proc_pidpath.restype=ctypes.c_int
            buf=ctypes.create_string_buffer(4096);count=lib.proc_pidpath(entry_pid,buf,len(buf))
            if count>0:entry_identity['path']=buf.value.decode(errors='replace')
        except Exception as error:entry_identity['path_error']=str(error)
        if row and entry_identity.get('path')==str(work/'Applications/VLC.app/Contents/MacOS/SiliconForest-VLC') and row not in own:own.append(row)
    capture=None
    if a.capture and own:
        p=subprocess.run(['/usr/bin/chonkstep-darwin-desktop','--screencapture',str(work/'vlc.png')],env=env,capture_output=True,text=True,timeout=20)
        capture={'returncode':p.returncode,'stdout':p.stdout,'stderr':p.stderr,'file_exists':(work/'vlc.png').exists()}
    report={'open_argv':argv,'open_returncode':r.returncode,'vlc_process_lines':own,'capture':capture,'entry_identity':entry_identity,
            'logs':{name:(work/name).read_text(errors='replace') if (work/name).exists() else None
                    for name in ['open.stdout','open.stderr','state/vlc.log']}, 'launch_user':a.as_user or 'current', 'audio_prefix':str(prefix) if a.audio_prefix else None, 'vendor_rc_stats':rc_stats,
            'framework_prefix':a.framework_prefix, 'hardware_requested':a.hardware, 'status_icon_disabled':a.no_statusicon, 'mode':'headless dummy video/real auhal' if a.headless else 'GUI attempt',
            'note':'Process presence/open success is not playback. No input injection or session restart. No other process was signalled.'}
    (work/'launch.json').write_text(json.dumps(report,indent=2)+'\n')
    print(json.dumps(report,indent=2))


if __name__=='__main__':main()
