# SPDX-License-Identifier: MIT
"""Synthetic host classifier tests; no native display or font claims."""
import json
from pathlib import Path
import tempfile
import unittest

from legibility_pixels import analyze


class TextPaintClassifier(unittest.TestCase):
    def fixture(self, root, ink=True, source='renderer', missing_last=False):
        width=80;height=40;visible=bytearray([255])*(width*height*3);hidden=bytes(visible);runs=[]
        for index in range(5):
            x=4+index*14
            runs.append(dict(text='web',font='12px test-font',boxes=[dict(x=x,y=8,width=10,height=12)]))
            if ink and not (missing_last and index==4):
                for y in range(11,17):
                    for dx in range(3,7):
                        at=(y*width+x+dx)*3;visible[at:at+3]=b'\0\0\0'
        for name,data in [('visible',visible),('hidden',hidden)]:
            p=root/(name+'.ppm');p.write_bytes(b'P6\n80 40\n255\n'+data);p.with_suffix('.source.json').write_text(json.dumps(dict(source=source)))
        row=dict(url='https://synthetic.invalid/',calibration=dict(complete=True,scale=1,origin=[0,0]),runs=runs,
                 renderer_images=dict(visible='visible.ppm',text_hidden='hidden.ppm'))
        (root/'legibility.json').write_text(json.dumps([row]))

    def test_empty_text_paint_fails(self):
        with tempfile.TemporaryDirectory() as d:
            self.fixture(Path(d),ink=False);r=analyze(Path(d))[0]
            self.assertFalse(r['paint_check_pass']);self.assertEqual(r['runs_with_ink'],0)

    def test_every_visible_run_with_ink_passes_paint_only(self):
        with tempfile.TemporaryDirectory() as d:
            self.fixture(Path(d));r=analyze(Path(d))[0]
            self.assertTrue(r['paint_check_pass']);self.assertEqual(r['runs_with_ink'],5)
            self.assertFalse(r['legibility_qualified']);self.assertTrue(r['visual_review_required'])

    def test_one_missing_run_cannot_hide_in_majority(self):
        with tempfile.TemporaryDirectory() as d:
            self.fixture(Path(d),missing_last=True);r=analyze(Path(d))[0]
            self.assertFalse(r['paint_check_pass']);self.assertEqual(r['runs_with_ink'],4)

    def test_content_screenshot_cannot_qualify_renderer(self):
        with tempfile.TemporaryDirectory() as d:
            self.fixture(Path(d),source='content-readback')
            with self.assertRaisesRegex(ValueError,'Unverified'):analyze(Path(d))


if __name__=='__main__':unittest.main()
