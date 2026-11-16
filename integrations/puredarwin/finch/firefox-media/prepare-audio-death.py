#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Freeze Linux-built observers and our no-input client-death diagnostic."""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import subprocess

p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--hal-observer',type=Path,required=True)
p.add_argument('--loss-observer',type=Path,required=True)
p.add_argument('--stage',type=Path,required=True)
a=p.parse_args();here=Path(__file__).resolve().parent
if a.stage.exists():raise ValueError('Fresh packet required')
a.stage.mkdir();(a.stage/'lib').mkdir();(a.stage/'tools').mkdir()
for source,name in [(a.hal_observer,'SFFirefoxMediaObserver.dylib'),(a.loss_observer,'SFFirefoxWaylandLoss.dylib')]:
    shutil.copy2(source,a.stage/'lib'/name)
subprocess.run(['python3',str(here.parent/'tools/portable-tests.py'),
    str(here.parents[1]/'homebrew/hb_macho.py'),str(a.stage/'lib'),str(a.stage/'portable-observers.json')],check=True)
shutil.copy2(here/'audio-death-browser.py',a.stage/'audio-death-browser.py')
for file in sorted((here.parents[1]/'firefox').iterdir()):
    if file.is_file() and file.suffix in ['.py','.html','.js','.json']:
        shutil.copy2(file,a.stage/'tools'/file.name)
rows={str(f.relative_to(a.stage)):hashlib.sha256(f.read_bytes()).hexdigest()
      for f in sorted(a.stage.rglob('*')) if f.is_file()}
(a.stage/'packet-files.json').write_text(json.dumps(rows,indent=2)+'\n')
print(json.dumps(dict(stage=str(a.stage),files=len(rows),observers={k:v for k,v in rows.items() if k.startswith('lib/')})))
