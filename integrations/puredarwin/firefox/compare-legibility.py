#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Compare actual text-paint gates with downloaded web fonts enabled/disabled.

Coverage need not be identical: fallback fonts change metrics and layout. A
missing ON run that paints OFF is a regression. Images still need review and
challenge/loading pages cannot qualify ordinary page content.
"""
import argparse
import json
from pathlib import Path


def main():
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--on',type=Path,required=True)
    ap.add_argument('--off',type=Path,required=True)
    ap.add_argument('--output',type=Path,required=True)
    args=ap.parse_args()
    def read(root):
        return {row['url']:row for row in json.loads((root/'legibility-text-pixel-coverage.json').read_text())}
    on,off=read(args.on),read(args.off)
    if on.keys()!=off.keys():raise ValueError('Paired captures must cover the same final URLs')
    rows=[]
    for url,a in on.items():
        b=off[url]
        def runs(row):
            found={}
            for run in row['runs']:
                if len(run['text'].strip())>=3 and run['area']>=20:
                    found.setdefault(' '.join(run['text'].split()),[]).append(run['ink_observed'])
            return found
        ar,br=runs(a),runs(b);common=ar.keys()&br.keys()
        missing=[text for text in sorted(common) if not all(ar[text]) and all(br[text])]
        rows.append(dict(url=url,on_eligible=a['eligible_runs'],on_painted=a['runs_with_ink'],
            off_eligible=b['eligible_runs'],off_painted=b['runs_with_ink'],
            on_coverage=a['coverage'],off_coverage=b['coverage'],common_texts=len(common),
            missing_only_with_web_fonts=missing,paint_controls_pass=bool(common) and not missing and
                a['paint_check_pass'] and b['paint_check_pass'],
            legibility_qualified=False,visual_review_required=True))
    result=dict(pages=rows,paint_controls_pass=bool(rows) and all(r['paint_controls_pass'] for r in rows),
        legibility_qualified=False,method=__doc__)
    args.output.write_text(json.dumps(result,indent=2)+'\n')
    print(json.dumps({k:v for k,v in result.items() if k!='method'},indent=2))
    return 0 if result['paint_controls_pass'] else 1


if __name__=='__main__':raise SystemExit(main())
