#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Offline native export-ID diagnostic, requiring this own run's parent admission."""
import argparse
import json
from pathlib import Path
from frame_export_analysis import analyze


def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--run',type=Path,required=True)
    p.add_argument('--output',type=Path,required=True);a=p.parse_args()
    calibration=json.loads((a.run/'frame-id-calibration.json').read_text())
    parent=calibration['owned_parent'];raw=json.loads((a.run/'browser/result.json').read_text())
    if (not calibration.get('owned_parent_start') or calibration.get('renderer_capture',{}).get('source')!='renderer' or
        raw['marionette']['capabilities']['moz:processID']!=parent):
        raise ValueError('Exact own parent and actual renderer admission required')
    files=json.loads((a.run/'browser/native-logs.json').read_text());texts=[];sources=[]
    import hashlib
    inherited=a.run/'browser/firefox.log'
    if inherited.is_file():
        data=inherited.read_bytes()
        if len(data)>8_000_000:raise ValueError('Inherited own native log exceeds bound')
        if data:
            texts.append(data.decode(errors='replace'))
            sources.append(dict(path='browser/firefox.log',bytes=len(data),sha256=hashlib.sha256(data).hexdigest(),
                                provenance='Original collected inherited stdout; private plan wrapper does not redirect through daily.main'))
    for row in files:
        path=Path(row['path'])
        if path.is_absolute() or '..' in path.parts:raise ValueError('Own collected relative log required')
        data=(a.run/'browser'/path).read_bytes()
        if len(data)!=row['bytes'] or hashlib.sha256(data).hexdigest()!=row['sha256']:
            raise ValueError('Original native log receipt mismatch')
        texts.append(data.decode(errors='replace'))
        sources.append(row)
    result=analyze('\n'.join(texts),parent)
    result['original_log_sources']=sources
    a.output.write_text(json.dumps(result,indent=2)+'\n');print(json.dumps({k:v for k,v in result.items()
        if k not in ['active_export_samples','missing_source_ids']},indent=2))
    return int(not result['identity_span_complete'])


if __name__=='__main__':raise SystemExit(main())
