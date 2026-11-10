"""
kernel: time: hrtimer only with pd_mod_hrtimer

LinuxKPI's struct hrtimer is a FreeBSD callout wrapper (no timerqueue node, no
hrtimer_setup/hrtimer_start_range_ns/hrtimer_forward, no HRTIMER_MODE_*_SOFT/HARD).
"""
from pdpatch import *
gate("kernel/time.rs", r"^pub mod hrtimer;", "pd_mod_hrtimer")
