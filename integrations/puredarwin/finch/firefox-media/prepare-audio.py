#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Create a fresh reproducible query/silent-lifecycle prefix from Linux output."""
import argparse,hashlib,json,shutil,subprocess
from pathlib import Path

p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--package',type=Path,required=True);p.add_argument('--stage',type=Path,required=True)
a=p.parse_args();here=Path(__file__).resolve().parent;tools=here.parent/'tools'
if a.stage.exists():raise ValueError('Fresh stage required')
a.stage.mkdir()
for directory in ['bin','lib']:
    shutil.copytree(a.package/directory,a.stage/directory);(a.stage/directory).chmod(0o755)
    subprocess.run(['python3',str(tools/'portable-tests.py'),str(here.parents[1]/'homebrew/hb_macho.py'),str(a.stage/directory),str(a.stage/('portable-'+directory+'.json'))],check=True)
for source,destination in [(here/'owned-cli.py','owned-cli.py'),(here/'cold-capture.py','compare-tests.py'),(tools/'owner_input.py','owner_input.py'),(tools/'compare-tests.py','compare-tests-engine.py')]:
    shutil.copy2(source,a.stage/destination)
manifest=[dict(name='firefox-audio-'+mode,argv=['@PYTHON@','owned-cli.py',mode],mode='exit',skip_lines=0,timeout=18,
               env={'SF_FIREFOX_MEDIA_TRACE':'1','SF_FIREFOX_MEDIA_TRACE_SECONDS':'10'}) for mode in ['query','silence']]
(a.stage/'manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
rows=[dict(path=str(f.relative_to(a.stage)),sha256=hashlib.sha256(f.read_bytes()).hexdigest()) for f in sorted(a.stage.rglob('*')) if f.is_file() and f.name!='cold-files.json']
(a.stage/'cold-files.json').write_text(json.dumps(rows,indent=2)+'\n')
print(json.dumps(dict(stage=str(a.stage),files=len(rows),cold_manifest_sha256=hashlib.sha256((a.stage/'cold-files.json').read_bytes()).hexdigest())))
