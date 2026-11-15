#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Run only our fresh staged audio probe, with no GUI or input operations."""
import argparse,json,os,signal,subprocess,time
from pathlib import Path
from owner_input import OwnerInputGuard


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('mode',choices=['query','silence','latency'])
    args=parser.parse_args();root=Path.cwd().resolve()
    if os.uname().sysname!='Darwin' or not str(root).startswith('/private/var/db/sffinch/'):
        raise ValueError('Fresh reviewed M1 prefix required')
    report=dict(mode=args.mode,program_started=False,abort=None,signals_to_own_child=[])
    output=root/args.mode;output.mkdir()
    child=None
    try:
        guard=OwnerInputGuard();report['owner_before']=guard.before
        argv=[str(root/('bin/audio-latency-probe' if args.mode=='latency' else 'bin/audio-media-probe'))]+(['--silence'] if args.mode=='silence' else [])
        with (output/'stdout').open('wb') as out,(output/'stderr').open('wb') as err:
            guard.check()
            child=subprocess.Popen(argv,stdout=out,stderr=err,start_new_session=True)
            report.update(program_started=True,pid=child.pid,argv=argv)
            deadline=time.monotonic()+12
            while child.poll() is None:
                guard.check()
                if time.monotonic()>deadline:raise TimeoutError('Our audio probe exceeded12s')
                time.sleep(.05)
            report['owner_after']=guard.check()
    except (OSError,RuntimeError,TimeoutError) as error:
        report.update(abort=type(error).__name__,reason=str(error))
    finally:
        if child:
            if child.poll() is None:
                for number in [signal.SIGTERM,signal.SIGKILL]:
                    try:os.killpg(child.pid,number)
                    except ProcessLookupError:break
                    report['signals_to_own_child'].append(int(number))
                    try:child.wait(timeout=2);break
                    except subprocess.TimeoutExpired:pass
            report['returncode']=child.wait(timeout=2)
    if report['abort']:
        try:report['owner_marker_after_abort']=Path('/private/var/run/gpu-desktop/owner-input').read_text()
        except OSError as error:report['owner_marker_read_error']=str(error)
    report['stdout']=(output/'stdout').read_text(errors='replace') if (output/'stdout').exists() else ''
    report['stderr']=(output/'stderr').read_text(errors='replace') if (output/'stderr').exists() else ''
    report['scope']='Own CLI/silent buffers only; no GUI/input/install/other-process signal; no acoustic or browser shutdown claim.'
    (output/'receipt.json').write_text(json.dumps(report,indent=2)+'\n')
    print(json.dumps(report),flush=True)
    return int(bool(report['abort'] or report.get('returncode')!=0))


if __name__=='__main__':raise SystemExit(main())
