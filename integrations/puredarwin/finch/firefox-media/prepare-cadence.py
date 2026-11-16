#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Prepare our Linux-built observer, public controls and no-input browser runner."""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import subprocess

p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--probes',type=Path,required=True)
p.add_argument('--admission',type=Path,required=True)
p.add_argument('--fixture',type=Path,required=True)
p.add_argument('--stage',type=Path,required=True)
p.add_argument('--providers',type=Path,help='Reviewed media payload only; graphics and broker remain selected by FX')
p.add_argument('--gpu-counter-spec',type=Path,help='Read-only CGL counters: exact runtime provider path/UUID and Linux symbol offset')
p.add_argument('--policy-probe',type=Path,help='Linux-built read-only Mach helper, required with --gpu-counter-spec')
a=p.parse_args();here=Path(__file__).resolve().parent
if bool(a.gpu_counter_spec)!=bool(a.policy_probe):raise ValueError('Counter specification and read-only helper required together')
if a.stage.exists():raise ValueError('Fresh destination required')
a.stage.mkdir();(a.stage/'bin').mkdir();(a.stage/'lib').mkdir();(a.stage/'media').mkdir()
for name in ['cadence-baseline','cadence-observed']:
    shutil.copy2(a.probes/'bin'/name,a.stage/'bin'/name)
shutil.copy2(a.probes/'lib/SFMediaCadenceTrace.dylib',a.stage/'lib/SFMediaCadenceTrace.dylib')
shutil.copy2(a.admission/'bin/vt-codec-admission',a.stage/'bin/vt-codec-admission')
shutil.copy2(a.fixture,a.stage/'media/hevc-main10-pq.mp4')
if a.policy_probe:shutil.copy2(a.policy_probe,a.stage/'bin/thread-policy-probe')
for directory in ['bin','lib']:
    subprocess.run(['python3',str(here.parent/'tools/portable-tests.py'),
        str(here.parents[1]/'homebrew/hb_macho.py'),str(a.stage/directory),
        str(a.stage/('portable-'+directory+'.json'))],check=True)
for name in ['cadence-browser.py','cadence-control.py','cadence-rate.js','process_cpu.py','media-launch.py','media_images.py']:
    shutil.copy2(here/name,a.stage/name)
shutil.copy2(here.parent/'tools/owner_input.py',a.stage/'owner_input.py')
tools=here.parents[1]/'firefox';(a.stage/'tools').mkdir()
for file in sorted(tools.iterdir()):
    if file.is_file() and file.suffix in ['.py','.html','.js','.json']:
        shutil.copy2(file,a.stage/'tools'/file.name)
page=a.stage/'tools/video-page.html'
page.write_text(page.read_text()+'\n<script src="../cadence-rate.js"></script>\n')
if a.gpu_counter_spec:shutil.copy2(a.gpu_counter_spec,a.stage/'tools/gpu-counters.json')
if a.providers:
    frameworks=a.providers/'Frameworks'
    names={p.name for p in frameworks.iterdir()}
    if not names or names-{'CoreVideo.framework','VideoToolbox.framework','AudioToolbox.framework'}:
        raise ValueError('Only reviewed media frameworks may be packaged')
    shutil.copytree(frameworks,a.stage/'Frameworks',symlinks=True)
rows={str(f.relative_to(a.stage)):hashlib.sha256(f.read_bytes()).hexdigest()
      for f in sorted(a.stage.rglob('*')) if f.is_file()}
(a.stage/'packet-files.json').write_text(json.dumps(rows,indent=2)+'\n')
print(json.dumps(dict(stage=str(a.stage),files=len(rows),trace_sha256=rows['lib/SFMediaCadenceTrace.dylib'])))
