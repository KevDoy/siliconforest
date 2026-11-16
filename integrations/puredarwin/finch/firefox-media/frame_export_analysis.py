# SPDX-License-Identifier: MIT
"""Interpret owned native export IDs; never infer compositor or panel delivery."""
import math
import re
from statistics import median
from frame_marker import decode_levels


def distribution(values):
    if not values:return None
    values=sorted(values)
    def percentile(f):
        point=(len(values)-1)*f;lo=math.floor(point);hi=math.ceil(point)
        return values[lo]+(values[hi]-values[lo])*(point-lo)
    return dict(count=len(values),minimum=values[0],p50=median(values),p95=percentile(.95),
                p99=percentile(.99),maximum=values[-1])


def analyze(text,parent):
    samples=[];foreign=0
    for line in text.splitlines():
        if not line.startswith('FXFRAMEID '):continue
        f=dict(re.findall(r'([A-Za-z0-9_]+)=(\S+)',line))
        if int(f['pid'])!=parent:foreign+=1;continue
        before=float(f['before_ms']);after=float(f['after_ms']);sequence=int(f['sequence'])
        if not math.isfinite(before+after) or before<0 or after<before:raise ValueError('Invalid export clock interval')
        if samples and (sequence<=samples[-1]['sequence'] or before<samples[-1]['before_ms']):
            raise ValueError('Own export sequence/clock is not ordered')
        frame=int(f['frame']);error=int(f['gl_error']);size=int(f['readback_bytes']);surface=int(f['surface'])
        if not 0<size<=65536 or not 0<surface<2**32:raise ValueError('Invalid readback extent or surface identity')
        valid=not error and frame>=0
        if valid:
            if f['reason']!='ok':raise ValueError('Native frame/reason contradiction')
            levels=[int(x) for x in f['rows'].split(',')]
            if len(levels)!=36 or decode_levels([levels[:18],levels[18:]])!=frame:
                raise ValueError('Independent raw marker decoding disagrees with native frame')
        elif frame!=-1 or f['reason'] not in ['gl_error','marker_rejected']:
            raise ValueError('Invalid native rejection row')
        samples.append(dict(sequence=sequence,surface=surface,frame=frame,valid=valid,gl_error=error,
            before_ms=before,after_ms=after,readback_ms=after-before,readback_bytes=size))
        if len(samples)>4096:raise ValueError('Unexpected own diagnostic sample count')
    if not samples:raise ValueError('No owned native frame-ID export rows')
    valid=[s for s in samples if s['valid']];positive=next((i for i,s in enumerate(valid) if s['frame']>0),None)
    start=None;end=None
    if positive is not None:
        zeros=[i for i,s in enumerate(valid[:positive]) if s['frame']==0]
        if zeros:start=zeros[-1]
        end=next((i for i,s in enumerate(valid) if i>=positive and s['frame']==899),None)
    active=valid[start:end+1] if start is not None and end is not None else []
    changed=[]
    for sample in active:
        if not changed or sample['frame']!=changed[-1]['frame']:changed.append(sample)
    observed={s['frame'] for s in active};elapsed=active[-1]['after_ms']-active[0]['after_ms'] if len(active)>1 else None
    changes=[b['after_ms']-a['after_ms'] for a,b in zip(changed,changed[1:])]
    return dict(scope='Private post-completion PARENT EXPORT images only; not completed compositor flips/panel fps',
        own_parent=parent,samples=len(samples),foreign_pid_rows=foreign,valid_samples=len(valid),
        rejected_marker_samples=sum(not s['valid'] and not s['gl_error'] for s in samples),
        gl_error_samples=sum(bool(s['gl_error']) for s in samples),
        identity_span_complete=bool(active),first_to_last_identity_span_ms=elapsed,
        distinct_source_ids=len(observed),missing_source_ids=sorted(set(range(900))-observed),
        all900_ids_observed=observed==set(range(900)),
        adjacent_repeats=len(active)-len(changed),regressions=sum(b['frame']<a['frame'] for a,b in zip(changed,changed[1:])),
        forward_skips=sum(max(0,b['frame']-a['frame']-1) for a,b in zip(changed,changed[1:])),
        diagnostic_distinct_id_hz=(len(observed)-1)*1000/elapsed if elapsed and len(observed)>1 else None,
        changed_id_interval_ms=distribution(changes),
        readback_cost_ms=distribution([s['readback_ms'] for s in samples]),
        diagnostic_readback_bytes=sum(s['readback_bytes'] for s in samples),
        active_export_samples=active,panel_delivery_proved=False,production_performance_qualified=False,
        limits='Raw36 levels independently decoded; native PID supplied by the admitted own fixture. Excludes preroll0 repeats and after-end899 repeats using the last0 before positive progress/first899. No JS/native clock bridge, actual-provider admission or completed-flip association is established by this parser. Instrumentation cost is retained; discontinuities remain observations.')
