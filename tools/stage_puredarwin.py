#!/usr/bin/env python3
"""Plan/stage the experimental RAM probe in an EXACT, CLEAN PureDarwin checkout.

No fetching, building, installing, booting or Git commits. Dry run by default.
Execution requires two explicit switches. Stage in a separate healthy checkout,
never in the baseline checkout or on the filesystem under recovery.
"""
from __future__ import annotations
import argparse
import difflib
import hashlib
import json
from pathlib import Path
import subprocess
import sys

ROOT=Path(__file__).resolve().parents[1]
PIN='e7aa00a60f0b56ef35aa24252e81467787bf71ea'
HEADER='src/Kernel/fbsdkpi/pd_dri_cdev.h'
HEADER_BLOB='395083ab8450653ede86772e27e1d3eff4e4b759'
ANCHOR='src/Kernel/Extensions/CMakeLists.txt'
DEST='src/Kernel/Extensions/DLMemoryProbe'
MARKER='# DARWIN_LAB_EXPERIMENTAL_RAM_PROBE_V1'
LAB_FILES=(
    'contracts/buffer_lifetime.h','contracts/buffer_lifetime.c',
    'core/backing_store.h','core/backing_store.c',
    'core/mapping_ticket.h','core/mapping_ticket.c',
    'probe/memory_probe.h','probe/memory_probe.c',
    'probe/memory_probe_protocol.h','probe/memory_probe_ioctl.h')
ADAPTER_FILES=('CMakeLists.txt','Info.plist','DLMemoryProbe.cpp','provider_iokit.cpp','provider_iokit.h')
class StageError(ValueError): pass

def git(root:Path,*args:str,input_text:str|None=None)->str:
    p=subprocess.run(['git','-c','core.hooksPath=/dev/null','-C',str(root),*args],
                     input=input_text,capture_output=True,text=True,timeout=60,check=False)
    if p.returncode:raise StageError('Git operation failed: '+p.stderr.strip())
    return p.stdout

def blob(data:bytes)->str:
    return hashlib.sha1(b'blob '+str(len(data)).encode()+b'\0'+data).hexdigest()

def files()->dict[str,bytes]:
    output={}
    for name in ADAPTER_FILES:
        source=ROOT/'integrations/puredarwin/DLMemoryProbe'/name
        if source.is_symlink() or not source.is_file():raise StageError('Missing/linked adapter source: '+name)
        output[f'{DEST}/{name}']=source.read_bytes()
    for path in LAB_FILES:
        source=ROOT/path
        if source.is_symlink() or not source.is_file():raise StageError('Missing/linked lab source: '+path)
        output[f'{DEST}/lab/{path}']=source.read_bytes()
    return output

def verify(checkout:Path)->Path:
    if checkout.is_symlink():raise StageError('Checkout path must not be a symlink')
    root=checkout.resolve(strict=True)
    if not root.is_dir() or Path(git(root,'rev-parse','--show-toplevel').strip()).resolve()!=root:
        raise StageError('Pass the checkout root, not a nested directory')
    if git(root,'rev-parse','HEAD').strip()!=PIN:raise StageError('Checkout is not at the supported pinned commit')
    if git(root,'status','--porcelain','--untracked-files=all').strip():raise StageError('Checkout must be clean, including untracked files')
    for path in (HEADER,ANCHOR):
        p=root/path
        if not p.is_file() or any(x.is_symlink() for x in (p,*p.parents)):
            raise StageError('Missing or linked integration path: '+path)
    if blob((root/HEADER).read_bytes())!=HEADER_BLOB:raise StageError('Pinned DRM header blob mismatch')
    if (root/DEST).exists() or (root/DEST).is_symlink():raise StageError('Destination already exists')
    if MARKER in (root/ANCHOR).read_text():raise StageError('Already staged')
    return root

def prepare(checkout:Path)->tuple[dict,str]:
    root=verify(checkout)
    additions=files()
    old=(root/ANCHOR).read_text()
    new=old+('' if old.endswith('\n') else '\n')+f'\n{MARKER}\nadd_subdirectory(DLMemoryProbe)\n'
    patch=''.join(difflib.unified_diff(old.splitlines(keepends=True),new.splitlines(keepends=True),
                                     fromfile='a/'+ANCHOR,tofile='b/'+ANCHOR))
    for path,data in sorted(additions.items()):
        text=data.decode('utf-8')
        if not text.endswith('\n'):raise StageError('Staged source must end in newline: '+path)
        patch+=''.join(difflib.unified_diff([],text.splitlines(keepends=True),fromfile='/dev/null',tofile='b/'+path))
    git(root,'apply','--check','--whitespace=error-all','-',input_text=patch)
    report={'status':'READY_TO_STAGE_UNBUILT_SOURCE','checkout':str(root),'required_commit':PIN,
            'header_blob':HEADER_BLOB,'patch_sha256':hashlib.sha256(patch.encode()).hexdigest(),
            'files':{p:hashlib.sha256(d).hexdigest() for p,d in sorted(additions.items())},
            'parent_change':ANCHOR,'kernel_built':False,'kernel_booted':False,
            'mapped_reclamation_enabled':False,'executed':False,
            'limits':['Staging does not build, link, include in a kernel collection, or activate the driver.',
                      'The C++ IOKit adapter has not been compiled against the actual pinned SDK.',
                      'A changed Git checkout must include these new files in the Nix source snapshot.',
                      'Mapped backing remains quarantined until reboot; a mapped smoke test must report BLOCKED.']}
    return report,patch

def execute(checkout:Path,acknowledge:bool)->dict:
    if not acknowledge:raise StageError('Explicit experimental acknowledgement is required')
    report,patch=prepare(checkout)
    # Recheck clean pin/header immediately before the one patch application.
    root=verify(checkout)
    git(root,'apply','--whitespace=error-all','-',input_text=patch)
    # No git-add/commit/build: leave a reviewable, intentional source diff.
    report.update(status='STAGED_UNBUILT_SOURCE',executed=True)
    return report

def main(argv:list[str]|None=None)->int:
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--checkout',type=Path,required=True)
    p.add_argument('--execute',action='store_true')
    p.add_argument('--acknowledge-experimental',action='store_true')
    a=p.parse_args(argv)
    try:
        if a.execute:report=execute(a.checkout,a.acknowledge_experimental)
        else:report,_=prepare(a.checkout)
        print(json.dumps(report,indent=2));return 0
    except (OSError,ValueError,subprocess.SubprocessError) as e:
        print(json.dumps({'status':'BLOCKED','error':str(e),'kernel_built':False}),file=sys.stderr);return 2
if __name__=='__main__':raise SystemExit(main())
