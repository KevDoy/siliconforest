// SPDX-License-Identifier: GPL-2.0
//! PDRustLKPI: the Rust-for-Linux `kernel` crate on PureDarwin's LinuxKPI, exercised from a
//! kext. Everything here goes through `kernel::` (no direct `bindings::` calls except to read
//! errno constants), so each line it logs is evidence about one abstraction.
//!
//! The C++ side (kext/PDRustLKPI.cpp) brings the LinuxKPI runtime up and calls
//! `pd_rust_lkpi_run()`. Every wait has a timeout: a missing wakeup fails the test instead of
//! hanging the thread that started the kext.

#![no_std]

use core::sync::atomic::{AtomicU32, Ordering};

use kernel::{
    alloc::AllocError,
    bindings,
    error::to_result,
    new_condvar,
    new_mutex,
    new_spinlock,
    prelude::*,
    sync::{
        atomic::{ordering, Atomic},
        Arc,
        Completion,
        CondVar,
        CondVarTimeoutResult,
        Mutex,
        SpinLock, //
    },
    time::{delay::fsleep, msecs_to_jiffies, Delta, Instant, Monotonic},
    workqueue::{self, impl_has_work, new_work, Work, WorkItem},
};

/// What `pr_info!` and friends put in front of every line (`module!` normally defines it).
const __LOG_PREFIX: &[u8] = b"PDRustLKPI\0";

/// How long any single wait may take before the test gives up on it.
const WAIT_MS: u32 = 5000;

static FAILURES: AtomicU32 = AtomicU32::new(0);

fn check(what: &str, ok: bool) {
    if !ok {
        FAILURES.fetch_add(1, Ordering::Relaxed);
        pr_err!("rust: CHECK FAILED: {}\n", what);
    }
}

// --- alloc ---------------------------------------------------------------------------------

fn test_alloc() -> Result {
    let boxed = KBox::new(0x1234_5678_9abc_def0_u64, GFP_KERNEL)?;

    let mut v: KVec<u32> = KVec::new();
    for i in 0..100u32 {
        v.push(i, GFP_KERNEL)?;
    }
    let sum: u32 = v.iter().sum();

    // 80000 bytes: past a page, LinuxKPI serves it from contigmalloc
    let mut big: KVVec<u64> = KVVec::with_capacity(10_000, GFP_KERNEL)?;
    for i in 0..10_000u64 {
        big.push(i, GFP_KERNEL)?;
    }
    let big_sum: u64 = big.iter().sum();

    let zeroed = KBox::<[u8; 64]>::new_uninit(GFP_KERNEL | __GFP_ZERO)?;
    // SAFETY: allocated with __GFP_ZERO, and all-zero is a valid `[u8; 64]`.
    let zeroed = unsafe { zeroed.assume_init() };
    let all_zero = zeroed.iter().all(|&b| b == 0);

    pr_info!(
        "rust: alloc: KBox={:#x} KVec len={} sum={} KVVec len={} sum={} zeroed={}\n",
        *boxed,
        v.len(),
        sum,
        big.len(),
        big_sum,
        all_zero
    );
    check("alloc values", *boxed == 0x1234_5678_9abc_def0 && sum == 4950 && big_sum == 49_995_000 && all_zero);

    // kmalloc on PureDarwin is aligned to 8 (IOMalloc plus a size header). The helper behind
    // `Kmalloc` refuses anything stricter rather than return misaligned memory.
    let wide: Result<KBox<u128>, AllocError> = KBox::new(1u128 << 100, GFP_KERNEL);
    pr_info!(
        "rust: alloc: KBox<u128> (align 16) = {} (kmalloc alignment here is 8)\n",
        if wide.is_ok() { "Ok" } else { "Err(AllocError)" }
    );
    check("over-aligned kmalloc refused", wide.is_err());
    Ok(())
}

// --- locks, Arc, atomics -------------------------------------------------------------------

#[pin_data]
struct Locked {
    #[pin]
    mutex: Mutex<u64>,
    #[pin]
    spin: SpinLock<u32>,
}

struct CountsDrop(u32);

static DROPS: AtomicU32 = AtomicU32::new(0);

impl Drop for CountsDrop {
    fn drop(&mut self) {
        DROPS.fetch_add(1, Ordering::Relaxed);
    }
}

fn test_sync() -> Result {
    let locked = Arc::pin_init(
        pin_init!(Locked {
            mutex <- new_mutex!(1, "PDRustLKPI::mutex"),
            spin <- new_spinlock!(5, "PDRustLKPI::spin"),
        }),
        GFP_KERNEL,
    )?;
    {
        let mut g = locked.mutex.lock();
        *g += 2;
    }
    {
        let mut g = locked.spin.lock();
        *g += 2;
    }
    let m = *locked.mutex.lock();
    let s = *locked.spin.lock();
    pr_info!("rust: sync: Mutex<u64> 1+2={} SpinLock<u32> 5+2={}\n", m, s);
    check("lock values", m == 3 && s == 7);

    let a = Arc::new(CountsDrop(5), GFP_KERNEL)?;
    let b = a.clone();
    let same = Arc::ptr_eq(&a, &b);
    let value = b.0;
    drop(a);
    let after_first = DROPS.load(Ordering::Relaxed);
    drop(b);
    let after_second = DROPS.load(Ordering::Relaxed);
    pr_info!(
        "rust: sync: Arc value={} ptr_eq={} drops after first/second drop={}/{}\n",
        value,
        same,
        after_first,
        after_second
    );
    check("arc", value == 5 && same && after_first == 0 && after_second == 1);

    let counter = Atomic::<i32>::new(40);
    let old = counter.fetch_add(2, ordering::Full);
    let swapped = counter.xchg(7, ordering::Full);
    let wide = Atomic::<u64>::new(u64::MAX - 1);
    wide.add(1, ordering::Relaxed);
    pr_info!(
        "rust: sync: Atomic<i32> fetch_add old={} xchg old={} now={} Atomic<u64>={:#x}\n",
        old,
        swapped,
        counter.load(ordering::Relaxed),
        wide.load(ordering::Relaxed)
    );
    check("atomics", old == 40 && swapped == 42 && counter.load(ordering::Relaxed) == 7 && wide.load(ordering::Relaxed) == u64::MAX);
    Ok(())
}

// --- errors --------------------------------------------------------------------------------

fn test_error() {
    let from_c = Error::from_errno(-(bindings::EINVAL as i32));
    let nomem = to_result(-(bindings::ENOMEM as i32));
    let fine = to_result(0);
    let is_einval = from_c == EINVAL;
    // XNU numbering reaches Rust through the bindings: EAGAIN is 35 here, 11 on Linux
    pr_info!(
        "rust: error: from_errno(-22)==EINVAL:{} to_errno={} to_result(-12)={:?} to_result(0)={:?} EAGAIN={}\n",
        is_einval,
        from_c.to_errno(),
        nomem,
        fine,
        EAGAIN.to_errno()
    );
    check("errors", is_einval && nomem == Err(ENOMEM) && fine.is_ok());
    let propagated: Result<u32> = (|| {
        to_result(-(bindings::EBUSY as i32))?;
        Ok(1)
    })();
    check("? propagates", propagated == Err(EBUSY));
}

// --- time ----------------------------------------------------------------------------------

fn test_time() {
    let start = Instant::<Monotonic>::now();
    fsleep(Delta::from_millis(20));
    let ms = start.elapsed().as_millis();
    let in_range = (15..2000).contains(&ms);
    pr_info!("rust: time: Monotonic elapsed over fsleep(20 ms) within 15..2000 ms: {}\n", in_range);
    if !in_range {
        pr_err!("rust: time: measured {} ms\n", ms);
    }
    check("monotonic time", in_range);
}

// --- workqueue, Completion, CondVar --------------------------------------------------------

#[pin_data]
struct Job {
    value: u32,
    result: Atomic<u32>,
    #[pin]
    done: Completion,
    #[pin]
    flag: Mutex<bool>,
    #[pin]
    flag_changed: CondVar,
    #[pin]
    work: Work<Job>,
}

impl_has_work! {
    impl HasWork<Self> for Job { self.work }
}

impl WorkItem for Job {
    type Pointer = Arc<Job>;

    fn run(this: Arc<Job>) {
        pr_info!("rust: workqueue: work item running, value={}\n", this.value);
        this.result.store(this.value + 1, ordering::Release);
        this.done.complete_all();
        // let the submitter get to its CondVar wait, then wake it
        fsleep(Delta::from_millis(50));
        *this.flag.lock() = true;
        this.flag_changed.notify_all();
    }
}

fn test_workqueue() -> Result {
    let job = Arc::pin_init(
        pin_init!(Job {
            value: 42,
            result: Atomic::new(0),
            done <- Completion::new(),
            flag <- new_mutex!(false, "PDRustLKPI::flag"),
            flag_changed <- new_condvar!("PDRustLKPI::flag_changed"),
            work <- new_work!("PDRustLKPI::work"),
        }),
        GFP_KERNEL,
    )?;

    let queued = workqueue::system().enqueue(job.clone()).is_ok();
    let completed = job.done.wait_for_completion_timeout(msecs_to_jiffies(WAIT_MS));
    let result = job.result.load(ordering::Acquire);
    pr_info!(
        "rust: workqueue: queued={} Completion within {} ms={} result={}\n",
        queued,
        WAIT_MS,
        completed,
        result
    );
    check("work item completed", queued && completed && result == 43);

    let mut woken = false;
    let mut timed_out = false;
    {
        let mut guard = job.flag.lock();
        while !*guard {
            match job.flag_changed.wait_timeout(&mut guard, msecs_to_jiffies(WAIT_MS)) {
                CondVarTimeoutResult::Timeout => {
                    timed_out = true;
                    break;
                }
                _ => woken = true,
            }
        }
        woken = woken || *guard;
    }
    pr_info!("rust: sync: CondVar wait ended: flag set={} timed out={}\n", woken && !timed_out, timed_out);
    check("condvar wakeup", woken && !timed_out);
    Ok(())
}

fn run_step(name: &str, r: Result) {
    if let Err(e) = r {
        pr_err!("rust: {} returned {:?}\n", name, e);
        FAILURES.fetch_add(1, Ordering::Relaxed);
    }
}

/// Runs every test once and returns the number of failed checks.
#[unsafe(no_mangle)]
pub extern "C" fn pd_rust_lkpi_run() -> u32 {
    pr_info!("rust: pr_info! through kernel::print: {} {:#x} {}\n", 42, 42, "hello");
    run_step("alloc", test_alloc());
    run_step("sync", test_sync());
    test_error();
    test_time();
    run_step("workqueue", test_workqueue());
    let failures = FAILURES.load(Ordering::Relaxed);
    pr_info!("rust: done, {} failed checks\n", failures);
    failures
}
