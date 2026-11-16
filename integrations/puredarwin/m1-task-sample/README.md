# Owned-child Mach sampling discriminator

FXPERF observed two idle Firefox families exit while sf-sample sampled their
parent; busy CNN sampling survived five times. This does not establish a kernel
cause. sf-sample suspends the entire task, enumerates threads, obtains states,
walks frames and aggregates its tree while suspended, then resumes the task.
It neither aborts threads nor writes their states. Long suspension, a Mach
state bug and application lifecycle remain distinct hypotheses.

probe.c accepts an exact kernelcache UUID, never an external target PID. It
forks a fresh own child with32 sleeping pthreads per case: enumeration alone,
suspend/enumerate/resume, plus register reads, then plus the actual bounded
frame walk. Each case lasts1s at2ms spacing and reports sample/error counts,
suspension duration and child wait status. No GPU, decoder, desktop or input
is used. SDK compilation and native execution are separate gates; a successful
child test cannot establish Firefox or every wait path is safe.

No kernel source or collection change is proposed. Native execution is pending.
The new helper is MIT; the frame walk follows the MIT sf-sample implementation.
