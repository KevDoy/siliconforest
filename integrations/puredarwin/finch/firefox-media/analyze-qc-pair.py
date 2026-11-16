#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Private matched QC copy admission. Actual four renderer images need review."""
import argparse
import importlib.util
import json
from pathlib import Path
import qc_copy_control as qc

spec=importlib.util.spec_from_file_location('installed_video',Path(__file__).with_name('analyze-installed-video.py'))
base=importlib.util.module_from_spec(spec);spec.loader.exec_module(base)

def arm(stage,mode):
    stage=Path(stage);run=stage/'target-browser/browser-run'
    launch=json.loads((run/'cadence-launch.json').read_text())
    raw=json.loads((run/'browser/result.json').read_text())
    identity=json.loads((stage/'qc-nv12/identity.json').read_text())
    original=base.analyze(run)
    # The three installed/graphics gates MUST be replaced by explicit private
    # admission, never promoted to a canonical or installed success.
    checks={k:v for k,v in original['checks'].items() if k not in ['installed_outer','canonical_graphics','actual_graphics']}
    checks['private_only']=launch.get('private_quartzcore_copy')==mode and not any(launch.get(k) for k in ['staged_media','graphics_override','private_frame_identity_graphics'])
    target=Path('/private/var/db/sffx')/stage.name
    try:qc.verify(raw['providers'],identity,target);checks['actual_private_cohort']=True
    except (ValueError,RuntimeError,KeyError):checks['actual_private_cohort']=False
    wrapper=json.loads((run/'browser/private-media-launch.json').read_text())
    checks['actual_switch']=wrapper.get('private_quartzcore_copy')==str(target/'qc-nv12') and wrapper.get('retained_installed_media') is True and wrapper.get('environment',{}).get('PD_CA_NV12_GPU_COPY')==('1' if mode=='on' else '0')
    clips=[]
    for index,(entry,clip) in enumerate(zip(raw.get('video_measurements',[]),original['clips'])):
        observed=entry['video'].get('quartzcore_uploads',{})
        delta=qc.compare(observed['before'],observed['after']);total=delta['total']
        checks[f'{index}_counter_receipt']=delta==observed['comparison'] and observed.get('gpu_copy')==mode
        checks[f'{index}_both_roles']=set(p['role'] for p in delta['processes'])=={'parent','gpu'}
        checks[f'{index}_nonvacuous']=total['yuv_frames']>=clip['expected_frames']*.8 and total['wide_cpu_frames']==0
        if mode=='on':
            checks[f'{index}_qc_gpu']=total['gpu_attempts']==total['gpu_frames']==total['yuv_frames'] and total['gpu_failures']==0
            checks[f'{index}_qc_cpu_zero']=all(total[k]==0 for k in ['cpu_frames','cpu_plane_calls','cpu_visible_bytes','cpu_source_span_bytes'])
        else:
            checks[f'{index}_qc_cpu_baseline']=total['cpu_frames']==total['yuv_frames'] and total['cpu_plane_calls']>=2*total['cpu_frames'] and total['cpu_visible_bytes']>0 and total['cpu_source_span_bytes']>=total['cpu_visible_bytes'] and total['gpu_attempts']==total['gpu_frames']==0
        checks[f'{index}_cpu_available']=clip.get('cpu_family_complete') is True and isinstance(clip.get('matched_family_cpu'),(float,int))
        clips.append(dict(clip,qc_uploads=delta))
    return dict(passed=all(checks.values()),checks=checks,clips=clips,launch=launch,
        identity=identity,providers=raw['providers'],installed_admission=original['passed'],
        installed_admission_must_remain_false=True,visual_review='REQUIRED actual two renderer images',
        scope='Owned private QC/CGL caller, not an installed provider, panel FPS or all browser memory copies.')

def compare(off,on):
    checks=dict(both_arms=off['passed'] and on['passed'],
        same_release=off['launch']['release']==on['launch']['release'],
        same_kernel=off['launch']['kernel_uuid']==on['launch']['kernel_uuid'],
        same_originals=off['identity']==on['identity'],
        installed_claim_refused=off['installed_admission'] is False and on['installed_admission'] is False)
    def providers(value):
        return sorted((p['role'],x['name'],x['uuid'],x['path'].split('/qc-nv12/',1)[-1]
                       if '/qc-nv12/' in x['path'] else x['path'])
            for p in value['providers']['processes'] for x in qc.image_rows(p['images']))
    checks['same_actual_providers']=providers(off)==providers(on)
    rows=[]
    for a,b in zip(off['clips'],on['clips']):
        checks['same_clip_'+str(a['expected_frames'])]=a['source'].rsplit('/',1)[-1]==b['source'].rsplit('/',1)[-1] and a['expected_frames']==b['expected_frames']
        rows.append(dict(source=a['source'],frames=a['expected_frames'],
            cpu_off=a['cpu_percent_one_core'],cpu_on=b['cpu_percent_one_core'],
            family_cpu_off=a['matched_family_cpu'],family_cpu_on=b['matched_family_cpu'],
            qc_off=a['qc_uploads']['total'],qc_on=b['qc_uploads']['total'],
            off_callback_gap_ms=a.get('rvfc_ms'),on_callback_gap_ms=b.get('rvfc_ms')))
    return dict(passed=all(checks.values()),checks=checks,clips=rows,
        visual_review='REQUIRED: independently inspect all four actual renderer images',
        limits='One fresh-profile OFF then ON pair; startup/background work and ordering can confound CPU. Counter-zero applies to observed decoder/CGL/QC transport, not every driver/browser copy. Not panel FPS, completed flips or asynchronous-fence qualification.')

if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--off',type=Path,required=True);p.add_argument('--on',type=Path,required=True);p.add_argument('--output',type=Path,required=True);a=p.parse_args()
    off=arm(a.off,'off');on=arm(a.on,'on');result=dict(off=off,on=on,pair=compare(off,on))
    a.output.write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(result['pair'],indent=2));raise SystemExit(0 if result['pair']['passed'] else 1)
