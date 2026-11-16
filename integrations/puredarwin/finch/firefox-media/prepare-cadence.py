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
p.add_argument('--codec-fixtures',type=Path,help='Our generated codec matrix with its Linux hash/probe manifest')
p.add_argument('--codec-profile',choices=['basic','highrate'],default='basic')
p.add_argument('--mse-fixtures',type=Path,help='Own Linux-generated representation-switch files and manifest')
p.add_argument('--mse-pcm-trace',type=Path,help='Linux-built output aggregate meter; required for MSE')
p.add_argument('--mse-hal-trace',type=Path,help='Linux-built HAL ring observer; required for MSE')
p.add_argument('--view-state',type=Path,help='Linux-built read-only AppKit getter observer/control prefix; MSE diagnostic only')
a=p.parse_args();here=Path(__file__).resolve().parent
if a.view_state and not a.mse_fixtures:raise ValueError('View-state diagnostic requires its own MSE case')
if bool(a.mse_fixtures)!=bool(a.mse_pcm_trace) or bool(a.mse_fixtures)!=bool(a.mse_hal_trace):
    raise ValueError('MSE requires its generated fixtures and both qualified native observers')
if bool(a.gpu_counter_spec)!=bool(a.policy_probe):raise ValueError('Counter specification and read-only helper required together')
if a.stage.exists():raise ValueError('Fresh destination required')
a.stage.mkdir();(a.stage/'bin').mkdir();(a.stage/'lib').mkdir();(a.stage/'media').mkdir()
for name in ['cadence-baseline','cadence-observed']:
    shutil.copy2(a.probes/'bin'/name,a.stage/'bin'/name)
shutil.copy2(a.probes/'lib/SFMediaCadenceTrace.dylib',a.stage/'lib/SFMediaCadenceTrace.dylib')
if a.mse_fixtures:
    manifest=json.loads((a.mse_fixtures/'manifest.json').read_text())
    target=a.stage/'media/mse';target.mkdir()
    for row in manifest['segments']:
        name=row['file']
        if Path(name).name!=name or not name.endswith('.mp4'):raise ValueError('Own MSE basename required')
        source=a.mse_fixtures/name;data=source.read_bytes()
        if len(data)!=row['bytes'] or hashlib.sha256(data).hexdigest()!=row['sha256']:
            raise ValueError('Generated MSE input changed')
        shutil.copy2(source,target/name)
    shutil.copy2(a.mse_fixtures/'manifest.json',target/'manifest.json')
    shutil.copy2(here/'mse-page.html',target/'index.html')
    shutil.copy2(a.mse_pcm_trace,a.stage/'lib/SFHALPCMTrace.dylib')
    shutil.copy2(a.mse_hal_trace,a.stage/'lib/SFFirefoxMediaObserver.dylib')
if a.view_state:
    shutil.copy2(a.view_state/'lib/SFFirefoxViewState.dylib',a.stage/'lib/SFFirefoxViewState.dylib')
    shutil.copy2(a.view_state/'bin/view-state-control',a.stage/'bin/view-state-control')
shutil.copy2(a.admission/'bin/vt-codec-admission',a.stage/'bin/vt-codec-admission')
shutil.copy2(a.fixture,a.stage/'media/hevc-main10-pq.mp4')
if a.codec_fixtures:
    manifest=json.loads((a.codec_fixtures/'manifest.json').read_text())
    rows={row['file']:row for row in manifest['fixtures']}
    names=['av1-1080p30.webm','vp9-1080p60.webm','hevc-main10-pq.mp4','vp9-profile2-pq.webm']
    if a.codec_profile=='highrate':names=['av1-1080p60.webm','vp9-4k30.webm']
    target=a.stage/'media/codecs';target.mkdir()
    for name in names:
        source=a.codec_fixtures/name
        if hashlib.sha256(source.read_bytes()).hexdigest()!=rows[name]['sha256']:
            raise ValueError('Generated codec fixture hash changed: '+name)
        shutil.copy2(source,target/name)
    shutil.copy2(a.codec_fixtures/'manifest.json',target/'manifest.json')
    (target/'selection.json').write_text(json.dumps(names)+'\n')
    shutil.copy2(here/'codec-capabilities.js',a.stage/'codec-capabilities.js')
if a.policy_probe:shutil.copy2(a.policy_probe,a.stage/'bin/thread-policy-probe')
for directory in ['bin','lib']:
    subprocess.run(['python3',str(here.parent/'tools/portable-tests.py'),
        str(here.parents[1]/'homebrew/hb_macho.py'),str(a.stage/directory),
        str(a.stage/('portable-'+directory+'.json'))],check=True)
for name in ['cadence-browser.py','cadence-control.py','cadence-rate.js','process_cpu.py','media-launch.py','media_images.py','video_selection.py','audio_logs.py']:
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
