#!/usr/bin/env python3
"""Test sensitivity to deliberate backing-store regressions in disposable copies.

A mutation counts as detected ONLY if it compiles and the tests fail at runtime.
Compile errors or timeouts are failures of this check, not successful detections.
Never modifies working-tree sources. No network or hardware access.
"""
from __future__ import annotations
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import subprocess
import sys
try:
    from .lab import ROOT
except ImportError:
    from lab import ROOT

MUTATIONS=[
    ('ignore_device_lease','if (v.owner_alive || v.host || v.device)', 'if (v.owner_alive || v.host)'),
    ('ignore_host_lease','if (v.owner_alive || v.host || v.device)', 'if (v.owner_alive || v.device)'),
    ('forget_byte_accounting','s->resident_bytes-=v.bytes;', '/* deliberately forget accounting */'),
    ('swallow_release_failure','return out->backend_failures ? DL_STORE_BACKEND_ERROR : DL_OK;', 'return DL_OK;'),
    ('drop_failed_release','if (!freed) { out->backend_failures++; out->retained++; continue; }',
     'if (!freed) { out->backend_failures++; /* deliberately drop backing ownership */ }'),
    ('ignore_owner','if (v.owner_alive || v.host || v.device)', 'if (v.host || v.device)'),
]

def run(cc: str, directory: Path) -> dict:
    directory.mkdir(parents=True,exist_ok=True)
    source=(ROOT/'core/backing_store.c').read_text()
    results=[]
    flags=['-std=c11','-O1','-g','-Wall','-Wextra','-Werror','-Icontracts','-Icore']
    # The unmodified baseline MUST pass using the same compiler/flags/tests.
    for name,before,after in [('unmodified',None,None),*MUTATIONS]:
        path=directory/(name+'.c'); binary=directory/name
        if before is not None and source.count(before)!=1:
            raise ValueError(f'{name}: mutation anchor is no longer unique')
        content=source if before is None else source.replace(before,after,1)
        path.write_text(content)
        command=[cc,*flags,'contracts/buffer_lifetime.c',str(path),'tests/c/test_backing_store.c','-o',str(binary)]
        build=subprocess.run(command,cwd=ROOT,capture_output=True,text=True,timeout=30)
        item={'name':name,'compile_command':command,'compile_returncode':build.returncode,'compile_stderr':build.stderr}
        if build.returncode==0:
            proc=subprocess.run([str(binary)],cwd=directory,capture_output=True,text=True,timeout=30)
            item.update(run_returncode=proc.returncode,stdout=proc.stdout,stderr=proc.stderr)
            item['status']=('BASELINE_PASSED' if proc.returncode==0 else 'BASELINE_FAILED') if before is None else (
                'DETECTED_AT_RUNTIME' if proc.returncode!=0 else 'MISSED_REGRESSION')
        else: item['status']='COMPILE_FAILED_NOT_A_DETECTION'
        results.append(item)
    passed=results[0]['status']=='BASELINE_PASSED' and all(r['status']=='DETECTED_AT_RUNTIME' for r in results[1:])
    return {'schema_version':1,'status':'PASSED_MUTATION_CHECKS' if passed else 'FAILED',
            'source_sha256':hashlib.sha256(source.encode()).hexdigest(),
            'test_sha256':hashlib.sha256((ROOT/'tests/c/test_backing_store.c').read_bytes()).hexdigest(),
            'detected':sum(r['status']=='DETECTED_AT_RUNTIME' for r in results),'mutations':len(MUTATIONS),'results':results,
            'limits':['Six deliberate source mutations are examples, not exhaustive correctness proof.',
                      'All execution is native host code with simulated device events, not an XNU driver.',
                      'A failing test process is expected for each mutated program.']}


def main(argv=None)->int:
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--cc');p.add_argument('--report',type=Path,default=ROOT/'.build/mutations.json')
    p.add_argument('--build-dir',type=Path,default=ROOT/'.build/mutations')
    a=p.parse_args(argv)
    try:
        cc=a.cc or shutil.which('clang') or shutil.which('cc')
        if not cc: raise ValueError('C compiler required')
        report=run(cc,a.build_dir.resolve())
        a.report.parent.mkdir(parents=True,exist_ok=True)
        a.report.write_text(json.dumps(report,indent=2)+'\n')
        print(json.dumps({'status':report['status'],'detected':report['detected'],'mutations':report['mutations']}))
        return 0 if report['status']=='PASSED_MUTATION_CHECKS' else 1
    except (ValueError,OSError,subprocess.SubprocessError) as exc:
        a.report.parent.mkdir(parents=True,exist_ok=True)
        a.report.write_text(json.dumps({'status':'FAILED_OR_INCOMPLETE','error':str(exc)})+'\n')
        print(str(exc),file=sys.stderr);return 2
if __name__=='__main__':raise SystemExit(main())
