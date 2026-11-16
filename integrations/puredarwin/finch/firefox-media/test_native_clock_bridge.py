# SPDX-License-Identifier: MIT
import unittest
import native_clock_bridge as c


class Bridge(unittest.TestCase):
    def test_shared_brackets_intersect(self):
        r=c.summarize([dict(epoch_before_ms=1000,epoch_after_ms=1002,native_monotonic_ms=10),
                       dict(epoch_before_ms=1001,epoch_after_ms=1002,native_monotonic_ms=11)])
        self.assertEqual(r['epoch_minus_native_ms'],990.5);self.assertEqual(r['uncertainty_ms'],.5)

    def test_inconsistent_clocks_refused(self):
        with self.assertRaises(ValueError):c.summarize([dict(epoch_before_ms=1000,epoch_after_ms=1001,native_monotonic_ms=10),
                       dict(epoch_before_ms=1100,epoch_after_ms=1101,native_monotonic_ms=11)])

    def test_missing_backwards_nonfinite_refused(self):
        for rows in [[],[dict(epoch_before_ms=2,epoch_after_ms=1,native_monotonic_ms=1)],
                     [dict(epoch_before_ms=float('nan'),epoch_after_ms=2,native_monotonic_ms=1)]]:
            with self.assertRaises(ValueError):c.summarize(rows)


if __name__=='__main__':unittest.main()
