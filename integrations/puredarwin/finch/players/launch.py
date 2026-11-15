#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Finite staged VLC launch through open -a. No screen input; optional same-uid capture."""
import argparse,json,os,subprocess,time,pwd,hashlib,re,ctypes
from pathlib import Path


def main():
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--capture',action='store_true')
    ap.add_argument('--as-user',help='Existing unprivileged account; no account or global privilege changes')
    a=ap.parse_args()
    work=Path(__file__).resolve().parent
    if os.uname().sysname!='Darwin' or not str(work).startswith('/private/var/db/sffinch/'):
        raise ValueError('Run only from the reviewed M1 staging directory')
    env={k:v for k,v in os.environ.items() if not k.startswith('DYLD_')}
    env.update(XDG_RUNTIME_DIR='/private/var/run/gpu-desktop',WAYLAND_DISPLAY='wayland-0',SF_METAL_ENABLE_REAL_DEVICE='0')
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
    env['SF_VLC_LOG']=str(state/'vlc.log')
    with (work/'open.stdout').open('w') as out,(work/'open.stderr').open('w') as err:
        r=subprocess.run(argv,env=env,stdout=out,stderr=err,timeout=15,preexec_fn=drop)
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
                    for name in ['open.stdout','open.stderr','state/vlc.log']}, 'launch_user':a.as_user or 'current',
            'note':'Process presence/open success is not playback. No input injection or session restart. No other process was signalled.'}
    (work/'launch.json').write_text(json.dumps(report,indent=2)+'\n')
    print(json.dumps(report,indent=2))


if __name__=='__main__':main()
