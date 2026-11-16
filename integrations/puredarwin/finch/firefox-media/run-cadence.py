#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Host collector. Caller holds gui.lock -> bench; fresh coordination gate.

control cold-copies a fresh packet; browser uses its recorded cold receipt.
No system installs, owner profile, input or process control outside our family.
"""
import argparse
from datetime import datetime,timezone
import json
import os
from pathlib import Path
import shlex
import subprocess
import tarfile

p=argparse.ArgumentParser(description=__doc__)
p.add_argument('stage',type=Path);p.add_argument('phase',choices=['control','browser','profile','rates','copy','shared'])
p.add_argument('--graphics-override',help='FX-qualified complete graphics cohort under the runtime overrides directory')
p.add_argument('--no-native-compositor',action='store_true',help='Explicit own-profile tile-path diagnostic')
p.add_argument('--timer-control',action='store_true',help='Explicit own-process PD CV timer comparison')
a=p.parse_args();repo=Path(__file__).resolve().parents[4]
if not os.environ.get('BENCH_OWNER'):raise ValueError('Caller must hold gui.lock then bench')
if not a.stage.name.startswith('firefox-media-cadence-packet-'):raise ValueError('Own cadence packet required')
dest='/private/var/db/sffx/'+a.stage.name
def command(argv,**kwargs):return subprocess.run(argv,check=True,**kwargs)
command(['python3',str(repo/'integrations/puredarwin/finch/tools/coord-gate.py'),
         '--max-seconds','170'],cwd=repo)
out=a.stage/('target-'+a.phase)
out.mkdir()
ssh=['ssh','-o','BatchMode=yes','-o','StrictHostKeyChecking=yes','-o','ConnectTimeout=8',
     '-o','UserKnownHostsFile='+str(Path.home()/'.ssh/known_hosts_m1_siliconforest'),
     '-o','HostKeyAlias=m1-siliconforest','root@10.1.1.211']
receipt=a.stage/'target-cold-verified.json'
def log(message):
    with (repo/'runs/m1-coord/log.txt').open('a') as f:
        f.write(datetime.now(timezone.utc).strftime('%Y-%m-%dT%H:%M:%SZ')+' CODEX '+message+'\n')
log('media cadence '+a.phase+' START; own CLI/browser profile only <=170s, no input/install/ownerprocess signals; no owneridlewait.')
try:
    if a.phase=='control':
        # Do not include this collector's growing output directory in the
        # transferred packet. put.py takes a fixed source snapshot first.
        import tempfile,shutil
        with tempfile.TemporaryDirectory() as temporary:
            source=Path(temporary)/'packet'
            shutil.copytree(a.stage,source,ignore=shutil.ignore_patterns('target-*'))
            command(['python3',str(repo/'integrations/puredarwin/firefox/put.py'),
                     '--source',str(source),'--destination',dest,'--receipt',str(receipt)],cwd=repo)
        argv=['python3',dest+'/cadence-control.py'];limit=100
    else:
        cold=json.loads(receipt.read_text())
        if cold.get('destination')!=dest or cold.get('cold_before_execution') is not True:
            raise ValueError('Missing prior cold receipt')
        argv=['python3',dest+'/cadence-browser.py','--run',dest+'/'+a.phase+'-run']
        if a.phase=='profile':argv+=['--profile-video']
        if a.phase=='rates':argv+=['--rate-control']
        if a.phase in ['copy','shared']:
            argv+=['--staged-media','--frame-mode','require' if a.phase=='shared' else 'copy','--without-trace']
        if a.graphics_override:
            if a.phase not in ['copy','shared']:raise ValueError('Graphics comparisons require staged-media copy/shared phase')
            argv+=['--graphics-override',a.graphics_override]
        if a.no_native_compositor:argv+=['--no-native-compositor']
        if a.timer_control:argv+=['--timer-control']
        limit=165
    with (out/'launcher.log').open('w') as stream:
        result=subprocess.run(ssh+[shlex.join(argv)],stdout=stream,stderr=subprocess.STDOUT,timeout=limit)
    (out/'runner-status.json').write_text(json.dumps(dict(returncode=result.returncode,
        diagnostic_only=True,synthetic_input=False,owner_idle_wait=False))+'\n')
finally:
    names=['cold-verified.json','control-results.json'] if a.phase=='control' else [a.phase+'-run']
    archive=out/'evidence.tar.gz'
    with archive.open('wb') as stream:
        r=subprocess.run(ssh+[shlex.join(['tar','-C',dest,'-czf','-']+names)],
                         stdout=stream,stderr=subprocess.PIPE,timeout=60)
    (out/'collection.stderr').write_bytes(r.stderr)
    if r.returncode==0:
        with tarfile.open(archive) as t:
            members=t.getmembers()
            (out/'evidence-links.json').write_text(json.dumps([dict(name=m.name,target=m.linkname)
                for m in members if m.issym() or m.islnk()])+'\n')
            t.extractall(out,members=[m for m in members if not m.issym() and not m.islnk()],filter='data')
    log('media cadence '+a.phase+' END; evidence collected, no installed changes/input.')
raise SystemExit(result.returncode or r.returncode)
