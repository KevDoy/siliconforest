# SPDX-License-Identifier: MIT
import unittest
import frame_export_analysis as a
import frame_marker as m


def row(sequence,frame,time,pid=100):
    bits=m.bits(frame);levels=[224 if x else 32 for x in bits]+[32 if x else 224 for x in bits]
    return f'FXFRAMEID pid={pid} sequence={sequence} surface=3 size=1920x1472 fbo=1 before_ms={time} after_ms={time+.1} readback_bytes=10368 frame={frame} gl_error=0 reason=ok rows='+','.join(map(str,levels))+'\n'


class Export(unittest.TestCase):
    def test_all900_native_ids(self):
        result=a.analyze(''.join(row(i,i,i*1000/60) for i in range(900)),100)
        self.assertTrue(result['all900_ids_observed']);self.assertAlmostEqual(result['diagnostic_distinct_id_hz'],60)
        self.assertFalse(result['panel_delivery_proved']);self.assertFalse(result['production_performance_qualified'])

    def test_sparse_ids_cannot_be_all_frames(self):
        result=a.analyze(row(0,0,0)+row(1,2,1000/30)+row(2,899,15000),100)
        self.assertFalse(result['all900_ids_observed']);self.assertEqual(result['distinct_source_ids'],3)
        self.assertEqual(len(result['missing_source_ids']),897)

    def test_preroll_and_after_end_duplicates_excluded(self):
        result=a.analyze(row(0,0,0)+row(1,0,1000)+row(2,1,1017)+row(3,899,16000)+row(4,899,20000),100)
        self.assertEqual(result['first_to_last_identity_span_ms'],15000)

    def test_native_raw_disagreement_refused(self):
        with self.assertRaises(ValueError):a.analyze(row(0,0,0).replace('frame=0','frame=1'),100)

    def test_foreign_pid_does_not_fill_missing_own_id(self):
        result=a.analyze(row(0,0,0)+row(0,1,10,pid=101)+row(1,899,15000),100)
        self.assertEqual(result['foreign_pid_rows'],1);self.assertIn(1,result['missing_source_ids'])

    def test_regressions_retained(self):
        result=a.analyze(row(0,0,0)+row(1,2,16)+row(2,1,33)+row(3,899,15000),100)
        self.assertEqual(result['regressions'],1)

    def test_clock_and_empty_span_refused(self):
        with self.assertRaises(ValueError):a.analyze(row(0,0,20)+row(1,1,10),100)
        with self.assertRaises(ValueError):a.analyze('',100)
        self.assertFalse(a.analyze(row(0,1,0)+row(1,899,15000),100)['identity_span_complete'])


if __name__=='__main__':unittest.main()
