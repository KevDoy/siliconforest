#!/usr/bin/env python3
"""Run offline tooling tests and the native C policy tests. Never boot a target."""
from __future__ import annotations
import argparse
from datetime import datetime, timezone
import io
import hashlib
import json
import os
from pathlib import Path
import platform
import re
import shutil
import subprocess
import sys
import unittest
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT))

def command(args: list[str], timeout: int=60) -> dict:
    try:
        result=subprocess.run(args,cwd=ROOT,capture_output=True,text=True,timeout=timeout,check=False)
        return {"command":args,"returncode":result.returncode,"stdout":result.stdout,"stderr":result.stderr}
    except (OSError,subprocess.TimeoutExpired) as exc:
        return {"command":args,"returncode":None,"stdout":"","stderr":str(exc)}

def main(argv: list[str]|None=None)->int:
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--sanitize',action='store_true')
    parser.add_argument('--cc',default=None)
    parser.add_argument('--report',type=Path,default=ROOT/'.build/checks.json')
    a=parser.parse_args(argv)
    started=datetime.now(timezone.utc).isoformat()
    build=ROOT/'.build'; build.mkdir(exist_ok=True)
    log=io.StringIO(); suite=unittest.defaultTestLoader.discover(str(ROOT/'tests'))
    result=unittest.TextTestRunner(stream=log,verbosity=2).run(suite)
    unit={"run":result.testsRun,"failures":len(result.failures),"errors":len(result.errors),"skipped":len(result.skipped),"passed":result.wasSuccessful()}
    print(log.getvalue(),end='')
    cc=a.cc or shutil.which('clang') or shutil.which('cc')
    c_results=[]; compiler=None; c_count=0
    if cc:
        compiler=command([cc,'--version'])
        flags=['-std=c11','-Wall','-Wextra','-Werror','-Wpedantic','-O1','-g']
        if a.sanitize:flags+=['-fsanitize=address,undefined','-fno-omit-frame-pointer']
        binary=build/('buffer-lifetime-tests.exe' if os.name=='nt' else 'buffer-lifetime-tests')
        compile_result=command([cc,*flags,'-Icontracts','contracts/buffer_lifetime.c','tests/c/test_buffer_lifetime.c','-o',str(binary)])
        c_results.append(compile_result)
        if compile_result['returncode']==0:
            run=command([str(binary)])
            c_results.append(run)
            match=re.search(r'^1\.\.(\d+)$',run['stdout'],re.M)
            c_count=int(match.group(1)) if match else 0
        # Freestanding compile only. Compiler runtime helpers may still be emitted.
        c_results.append(command([cc,'-std=c11','-ffreestanding','-fno-builtin','-Wall','-Wextra','-Werror','-Icontracts','-c','contracts/buffer_lifetime.c','-o',str(build/'lifetime-freestanding.o')]))
    for item in c_results:
        print('$',' '.join(item['command'])); print(item['stdout'],end=''); print(item['stderr'],end='',file=sys.stderr)
    passed=bool(result.wasSuccessful() and not result.skipped and cc and c_count and all(x['returncode']==0 for x in c_results))
    source_files=sorted({p for folder in ('tools','tests','contracts') for p in (ROOT/folder).rglob('*') if p.is_file() and p.suffix in {'.py','.c','.h'}})
    source_hashes={p.relative_to(ROOT).as_posix():hashlib.sha256(p.read_bytes()).hexdigest() for p in source_files}
    report={"schema_version":1,"tested_source_sha256":source_hashes,"started_at":started,"host":{"system":platform.system(),"machine":platform.machine(),"python":platform.python_version()},
            "status":"PASSED_LOCAL_TOOLING" if passed else "FAILED_OR_INCOMPLETE", "unit_tests":unit,
            "native_c_test_groups":c_count,"sanitizers_requested":a.sanitize,"compiler":compiler,"c_commands":c_results,
            "os_built":False,"xnu_booted":False,"gpu_tested":False,"hardware_accessed":False,
            "limits":["Native tests exercise an original policy model, not the Omacom or PureDarwin driver.",
                      "Synthetic C layout tests do not establish Darwin or Asahi ABI compatibility.",
                      "No live network acquisition or GitHub publication is tested by this command."]}
    a.report.parent.mkdir(parents=True,exist_ok=True)
    a.report.write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    a.report.with_suffix('.unittest.txt').write_text(log.getvalue(),encoding='utf-8')
    print(json.dumps({"status":report['status'],"python_tests":unit['run'],"c_test_groups":c_count,"report":str(a.report)},indent=2))
    return 0 if passed else 1
if __name__=='__main__':raise SystemExit(main())
