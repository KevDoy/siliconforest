//! Managed application ownership. No native launch is admitted in this revision.
//!
//! The C ABI is root-owned. Default builds refuse; Apple-only explicit linking
//! may bind the frozen leaf. This does not enable a native session entry point.
use crate::launch_audit::{Gate, LaunchAudit};
use std::{
    collections::BTreeMap,
    ffi::{c_char, CString},
    io,
    num::NonZeroI32,
    os::fd::{AsRawFd, BorrowedFd},
    os::unix::net::UnixStream,
    time::{Duration, Instant},
};

pub const STATUS_LIMIT: usize = 256;
pub const MAX_APPLICATIONS: usize = 2;

#[repr(C)]
pub struct SpawnRequestV1 {
    pub version: u32,
    pub argc: u32,
    pub envc: u32,
    pub reserved: u32,
    pub executable: *const c_char,
    pub argv: *const *const c_char,
    pub envp: *const *const c_char,
    pub source_fds: [i32; 4],
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct SpawnResultV1 {
    pub version: u32,
    pub stage: u32,
    pub pid: i32,
    pub primary_error: i32,
    pub spawn_error: i32,
    pub attr_destroy_error: i32,
    pub actions_destroy_error: i32,
    pub fd_close_error: i32,
    pub fd_close_failures: u32,
    pub requested_flags: u32,
    pub observed_flags: u32,
    pub child_created: u32,
    pub duplicate_count: u32,
    pub reserved: u32,
}
const _: () = {
    use std::mem::{align_of, offset_of, size_of};
    assert!(size_of::<usize>() == 8);
    assert!(size_of::<SpawnRequestV1>() == 56 && align_of::<SpawnRequestV1>() == 8);
    assert!(offset_of!(SpawnRequestV1, executable) == 16);
    assert!(offset_of!(SpawnRequestV1, argv) == 24);
    assert!(offset_of!(SpawnRequestV1, envp) == 32);
    assert!(offset_of!(SpawnRequestV1, source_fds) == 40);
    assert!(size_of::<SpawnResultV1>() == 56 && align_of::<SpawnResultV1>() == 4);
    assert!(offset_of!(SpawnResultV1, pid) == 8);
    assert!(offset_of!(SpawnResultV1, child_created) == 44);
    assert!(offset_of!(SpawnResultV1, reserved) == 52);
};

#[derive(Clone, Debug, PartialEq, Eq, PartialOrd, Ord)]
pub struct Instance(String);
impl Instance {
    pub fn parse(s: &str) -> Result<Self, &'static str> {
        if s.len() != 48
            || !s
                .bytes()
                .all(|b| b.is_ascii_digit() || (b'a'..=b'f').contains(&b))
        {
            return Err("instance must be 48 lowercase hex bytes");
        }
        Ok(Self(s.into()))
    }
    pub fn as_str(&self) -> &str {
        &self.0
    }
}

/// Storage owns all strings/arrays. Borrowed source FDs outlive the shim call;
/// no ambient descriptors, PATH lookup or pre-exec callback is available.
pub struct Launch<'a> {
    strings: Vec<CString>,
    env: Vec<CString>,
    fds: [BorrowedFd<'a>; 4],
}
impl<'a> Launch<'a> {
    pub fn new(
        executable: &str,
        args: &[String],
        env: &[String],
        fds: [BorrowedFd<'a>; 4],
    ) -> Result<Self, &'static str> {
        if !executable.starts_with('/') || args.len() >= 32 || env.len() > 32 {
            return Err("invalid executable/argument/environment bound");
        }
        let mut total = 0usize;
        let mut string = |s: &str| {
            // Includes NUL in the C bound.
            if s.len() >= 1024 {
                return Err("spawn string too long");
            }
            total = total
                .checked_add(s.len() + 1)
                .ok_or("spawn size overflow")?;
            if total > 32768 {
                return Err("spawn string total too large");
            }
            CString::new(s).map_err(|_| "spawn string contains NUL")
        };
        let strings = std::iter::once(executable)
            .chain(args.iter().map(String::as_str))
            .map(&mut string)
            .collect::<Result<Vec<_>, _>>()?;
        let env = env
            .iter()
            .map(|s| string(s))
            .collect::<Result<Vec<_>, _>>()?;
        Ok(Self { strings, env, fds })
    }
    fn with_request<R>(&self, f: impl FnOnce(&SpawnRequestV1) -> R) -> R {
        let argv: Vec<_> = self
            .strings
            .iter()
            .map(|s| s.as_ptr())
            .chain([std::ptr::null()])
            .collect();
        let envp: Vec<_> = self
            .env
            .iter()
            .map(|s| s.as_ptr())
            .chain([std::ptr::null()])
            .collect();
        f(&SpawnRequestV1 {
            version: 1,
            argc: self.strings.len() as u32,
            envc: self.env.len() as u32,
            reserved: 0,
            executable: self.strings[0].as_ptr(),
            argv: argv.as_ptr(),
            envp: envp.as_ptr(),
            source_fds: self.fds.map(|fd| fd.as_raw_fd()),
        })
    }
}

#[derive(Debug)]
pub enum SpawnFailure {
    Unsupported,
    InvalidResult,
    Native {
        returned: i32,
        result: SpawnResultV1,
    },
}
/// Internal facade; only the reviewed C leaf may implement the production call.
trait SpawnApi {
    fn call(&mut self, request: &SpawnRequestV1) -> Option<(i32, SpawnResultV1)>;
}
struct UnavailableShim;
impl SpawnApi for UnavailableShim {
    fn call(&mut self, _: &SpawnRequestV1) -> Option<(i32, SpawnResultV1)> {
        None
    }
}
#[cfg(all(feature = "native-managed-spawn", target_vendor = "apple"))]
struct FrozenDarwinShim;
#[cfg(all(feature = "native-managed-spawn", target_vendor = "apple"))]
unsafe extern "C" {
    fn chonk_darwin_spawn_v1(request: *const SpawnRequestV1, result: *mut SpawnResultV1) -> i32;
}
#[cfg(all(feature = "native-managed-spawn", target_vendor = "apple"))]
impl SpawnApi for FrozenDarwinShim {
    fn call(&mut self, request: &SpawnRequestV1) -> Option<(i32, SpawnResultV1)> {
        let mut result = SpawnResultV1::default();
        // CString arrays and BorrowedFds are live for the entire synchronous
        // call. No SDK opaque types or fallback spawn operation exists here.
        let returned = unsafe { chonk_darwin_spawn_v1(request, &mut result) };
        Some((returned, result))
    }
}
#[cfg(test)]
struct NativeSpawner;
#[cfg(test)]
impl NativeSpawner {
    pub fn spawn(&mut self, launch: &Launch<'_>) -> SpawnAttempt {
        #[cfg(all(feature = "native-managed-spawn", target_vendor = "apple"))]
        return spawn_using(&mut FrozenDarwinShim, launch);
        #[cfg(not(all(feature = "native-managed-spawn", target_vendor = "apple")))]
        spawn_using(&mut UnavailableShim, launch)
    }
}

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum WaitState {
    Running,
    Reaped(i32),
    Unknown(i32),
}
pub struct OwnedChild {
    pid: NonZeroI32,
    state: WaitState,
}
impl OwnedChild {
    fn new(pid: i32) -> Option<Self> {
        (pid > 0).then(|| Self {
            pid: NonZeroI32::new(pid).unwrap(),
            state: WaitState::Running,
        })
    }
    pub fn pid(&self) -> i32 {
        self.pid.get()
    }
    pub fn state(&self) -> WaitState {
        self.state
    }
    pub fn poll(&mut self) -> WaitState {
        self.poll_using(&mut UnixWait)
    }
    fn poll_using(&mut self, ops: &mut impl WaitApi) -> WaitState {
        if self.state == WaitState::Running {
            self.state = match ops.wait(self.pid()) {
                Ok(Some(status)) => WaitState::Reaped(status),
                Ok(None) | Err(libc::EINTR) => WaitState::Running,
                Err(error) => WaitState::Unknown(error),
            };
        }
        self.state
    }
    pub fn terminate(&mut self) -> io::Result<()> {
        self.signal_using(&mut UnixWait, libc::SIGTERM)
    }
    pub fn kill(&mut self) -> io::Result<()> {
        self.signal_using(&mut UnixWait, libc::SIGKILL)
    }
    fn signal_using(&mut self, ops: &mut impl WaitApi, signal: i32) -> io::Result<()> {
        if self.state != WaitState::Running {
            return Err(io::Error::other("child no longer exclusively wait-owned"));
        }
        ops.signal(self.pid(), signal)
            .map_err(io::Error::from_raw_os_error)
    }
}
trait WaitApi {
    fn wait(&mut self, pid: i32) -> Result<Option<i32>, i32>;
    fn signal(&mut self, pid: i32, signal: i32) -> Result<(), i32>;
}
struct UnixWait;
impl WaitApi for UnixWait {
    fn wait(&mut self, pid: i32) -> Result<Option<i32>, i32> {
        let mut status = 0;
        // Only this positive, directly returned child PID is ever waited on.
        let returned = unsafe { libc::waitpid(pid, &mut status, libc::WNOHANG) };
        if returned == 0 {
            return Ok(None);
        }
        if returned < 0 {
            return Err(io::Error::last_os_error()
                .raw_os_error()
                .unwrap_or(libc::EIO));
        }
        if returned != pid || !(libc::WIFEXITED(status) || libc::WIFSIGNALED(status)) {
            return Err(libc::EPROTO);
        }
        Ok(Some(status))
    }
    fn signal(&mut self, pid: i32, signal: i32) -> Result<(), i32> {
        if unsafe { libc::kill(pid, signal) } == 0 {
            Ok(())
        } else {
            Err(io::Error::last_os_error()
                .raw_os_error()
                .unwrap_or(libc::EIO))
        }
    }
}

/// Failure and child ownership are independent. A Result<Child, Error> would
/// lose a successfully spawned PID on attr/actions/FD cleanup failure.
pub struct SpawnAttempt {
    pub child: Option<OwnedChild>,
    pub failure: Option<SpawnFailure>,
    pub native: Option<(i32, SpawnResultV1)>,
}
fn spawn_using(api: &mut impl SpawnApi, launch: &Launch<'_>) -> SpawnAttempt {
    let Some((returned, result)) = launch.with_request(|r| api.call(r)) else {
        return SpawnAttempt {
            child: None,
            failure: Some(SpawnFailure::Unsupported),
            native: None,
        };
    };
    // Adopt first; validation must not throw away ownership transferred by C.
    let child = if result.child_created == 1 {
        OwnedChild::new(result.pid)
    } else {
        None
    };
    let valid = result.version == 1
        && result.reserved == 0
        && result.child_created <= 1
        && ((result.child_created == 1 && result.pid > 0)
            || (result.child_created == 0 && result.pid == 0));
    let clean = valid
        && returned == 0
        && result.child_created == 1
        && result.stage == 10
        && [
            result.primary_error,
            result.spawn_error,
            result.attr_destroy_error,
            result.actions_destroy_error,
            result.fd_close_error,
        ]
        .iter()
        .all(|e| *e == 0)
        && result.fd_close_failures == 0
        && result.duplicate_count == 4
        // Exact c1a3bf82 leaf: CLOEXEC_DEFAULT | SETSIGDEF | SETSIGMASK |
        // SETPGROUP. Treat a changed policy as failure while retaining the PID.
        && result.requested_flags == 0x400e
        && result.requested_flags == result.observed_flags;
    let failure = if clean {
        None
    } else if !valid {
        Some(SpawnFailure::InvalidResult)
    } else {
        Some(SpawnFailure::Native { returned, result })
    };
    SpawnAttempt {
        child,
        failure,
        native: Some((returned, result)),
    }
}

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub struct Cleanup {
    pub complete: bool,
    pub raw_wait_status: i32,
    pub term_sent: bool,
    pub kill_sent: bool,
}
pub struct Status {
    expected: Instance,
    pid: i32,
    pending: Vec<u8>,
    ready: bool,
    cleanup: Option<Cleanup>,
    error: Option<&'static str>,
    eof: bool,
    received: usize,
}
impl Status {
    pub fn new(expected: Instance, pid: i32) -> Result<Self, &'static str> {
        if pid <= 0 {
            return Err("invalid owned child pid");
        }
        Ok(Self {
            expected,
            pid,
            pending: Vec::new(),
            ready: false,
            cleanup: None,
            error: None,
            eof: false,
            received: 0,
        })
    }
    pub fn ready(&self) -> bool {
        self.ready && self.error.is_none()
    }
    pub fn ready_observed(&self) -> bool {
        self.ready
    }
    pub fn eof_observed(&self) -> bool {
        self.eof
    }
    pub fn bytes_observed(&self) -> usize {
        self.received
    }
    pub fn cleanup(&self) -> Option<Cleanup> {
        self.cleanup
    }
    pub fn error(&self) -> Option<&'static str> {
        self.error
    }
    pub fn closed(&self) -> bool {
        self.eof && self.pending.is_empty() && self.error.is_none()
    }
    fn fail(&mut self, error: &'static str) -> Result<(), &'static str> {
        self.error.get_or_insert(error);
        Err(self.error.unwrap())
    }
    pub fn feed(&mut self, bytes: &[u8]) -> Result<(), &'static str> {
        self.received = self.received.saturating_add(bytes.len());
        if let Some(e) = self.error {
            return Err(e);
        }
        if self.eof {
            return self.fail("status bytes after EOF");
        }
        for &b in bytes {
            if self.cleanup.is_some() {
                return self.fail("status after CLEANUP");
            }
            if self.pending.len() + 1 > STATUS_LIMIT {
                return self.fail("oversize status frame");
            }
            if b == b'\n' {
                let line = std::mem::take(&mut self.pending);
                if let Err(e) = self.line(&line) {
                    return self.fail(e);
                }
            } else if (b' '..=b'~').contains(&b) {
                self.pending.push(b);
            } else {
                return self.fail("non-ASCII status frame");
            }
        }
        Ok(())
    }
    fn line(&mut self, line: &[u8]) -> Result<(), &'static str> {
        let text = std::str::from_utf8(line).map_err(|_| "invalid status encoding")?;
        let fields: Vec<_> = text.split(' ').collect();
        if fields.len() < 4
            || fields[0] != "CHONKTERM/1"
            || fields[2] != self.expected.as_str()
            || decimal(fields[3])? != self.pid
        {
            return Err("status identity/header mismatch");
        }
        match fields[1] {
            "READY" if fields.len() == 4 && !self.ready => {
                self.ready = true;
                Ok(())
            }
            "CLEANUP" if fields.len() == 8 => {
                let complete = boolean(fields[4])?;
                let status = decimal(fields[5])?;
                if status < -1
                    || (complete
                        && (status == -1
                            || !(libc::WIFEXITED(status) || libc::WIFSIGNALED(status))))
                {
                    return Err("invalid PTY cleanup status");
                }
                self.cleanup = Some(Cleanup {
                    complete,
                    raw_wait_status: status,
                    term_sent: boolean(fields[6])?,
                    kill_sent: boolean(fields[7])?,
                });
                Ok(())
            }
            _ => Err("invalid/duplicate/out-of-order status"),
        }
    }
    pub fn finish(&mut self) -> Result<(), &'static str> {
        self.eof = true;
        if let Some(e) = self.error {
            return Err(e);
        }
        if !self.pending.is_empty() {
            return self.fail("truncated status at EOF");
        }
        if self.cleanup.is_none() {
            return self.fail("missing CLEANUP at EOF");
        }
        Ok(())
    }
    pub fn drain(&mut self, stream: &mut UnixStream) -> Result<(), &'static str> {
        use std::io::Read;
        if let Some(error) = self.error {
            return Err(error);
        }
        // The owner sets nonblocking before handing a stream to this method.
        // Recheck it here so accidental blocking admission fails before read.
        let flags = unsafe { libc::fcntl(stream.as_raw_fd(), libc::F_GETFL) };
        if flags < 0 || flags & libc::O_NONBLOCK == 0 {
            return self.fail("status stream is not nonblocking");
        }
        if self.eof {
            return self.error.map_or(Ok(()), Err);
        }
        for _ in 0..4 {
            let mut bytes = [0u8; STATUS_LIMIT];
            match stream.read(&mut bytes) {
                Ok(0) => return self.finish(),
                Ok(n) => self.feed(&bytes[..n])?,
                Err(e) if e.kind() == io::ErrorKind::WouldBlock => break,
                Err(e) if e.kind() == io::ErrorKind::Interrupted => continue,
                Err(_) => return self.fail("status read failed"),
            }
        }
        Ok(())
    }
}
fn decimal(s: &str) -> Result<i32, &'static str> {
    let value: i32 = s.parse().map_err(|_| "invalid decimal status field")?;
    if value.to_string() != s {
        return Err("noncanonical status field");
    }
    Ok(value)
}
fn boolean(s: &str) -> Result<bool, &'static str> {
    match s {
        "0" => Ok(false),
        "1" => Ok(true),
        _ => Err("invalid status boolean"),
    }
}

pub struct Application {
    pub child: OwnedChild,
    pub status: Status,
    pub launch_failure: Option<SpawnFailure>,
    stream: UnixStream,
    audit_sequence: usize,
    lifecycle: Lifecycle,
}
impl Application {
    pub fn cleanup_complete(&self) -> bool {
        matches!(self.child.state(), WaitState::Reaped(_))
            && self.status.closed()
            && self.status.cleanup().is_some_and(|c| c.complete)
    }
}
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum Completion {
    Pending,
    Complete,
    Incomplete(&'static str),
}
/// All waits/signals happen on the owning event loop; no detached reaper or
/// blocking Drop. Failure retains the PID/state for the session's final audit.
struct Lifecycle {
    ready_deadline: Instant,
    application_deadline: Instant,
    term_at: Option<Instant>,
    kill_at: Option<Instant>,
    reaped_at: Option<Instant>,
    completion: Completion,
}
impl Lifecycle {
    fn new(now: Instant, lifetime: Duration) -> Self {
        Self {
            ready_deadline: now + Duration::from_secs(25),
            application_deadline: now + lifetime,
            term_at: None,
            kill_at: None,
            reaped_at: None,
            completion: Completion::Pending,
        }
    }
}
impl Application {
    pub fn completion(&self) -> Completion {
        self.lifecycle.completion
    }
    fn service(
        &mut self,
        now: Instant,
        closing: bool,
        audit: &mut LaunchAudit,
        ops: &mut impl WaitApi,
    ) {
        let _ = self.status.drain(&mut self.stream);
        self.child.poll_using(ops);
        audit.observe(self.audit_sequence, &self.status, self.child.state());
        // Late reaps are still recorded after an incomplete deadline, but no
        // new signals or attempted success reclassification follow that result.
        if self.lifecycle.completion != Completion::Pending {
            return;
        }
        let incomplete = match self.child.state() {
            WaitState::Unknown(_) => Some("exclusive child wait ownership lost"),
            WaitState::Reaped(_) => {
                let reaped = *self.lifecycle.reaped_at.get_or_insert(now);
                if self.cleanup_complete() {
                    self.lifecycle.completion = Completion::Complete;
                    return;
                }
                if self.status.error().is_some()
                    || self.status.eof_observed()
                    || now.saturating_duration_since(reaped) >= Duration::from_secs(2)
                {
                    Some("application reaped without complete sealed cleanup")
                } else {
                    None
                }
            }
            WaitState::Running => {
                let ready_expired = !self.status.ready() && now >= self.lifecycle.ready_deadline;
                if ready_expired {
                    audit.fail(self.audit_sequence, "application READY deadline expired");
                }
                let stopping = closing
                    || ready_expired
                    || now >= self.lifecycle.application_deadline
                    || self.launch_failure.is_some()
                    || self.status.error().is_some()
                    || audit.error().is_some();
                if stopping && self.lifecycle.term_at.is_none() {
                    // Record intent first; I/O failure cannot skip ownership cleanup.
                    self.lifecycle.term_at = Some(now);
                    audit.mark(self.audit_sequence, Gate::TermAttempt);
                    if self.child.signal_using(ops, libc::SIGTERM).is_err() {
                        audit.fail(self.audit_sequence, "owned application TERM failed");
                    }
                }
                if self.lifecycle.kill_at.is_none()
                    && self.lifecycle.term_at.is_some_and(|at| {
                        now.saturating_duration_since(at) >= Duration::from_secs(5)
                    })
                {
                    self.lifecycle.kill_at = Some(now);
                    audit.mark(self.audit_sequence, Gate::KillAttempt);
                    if self.child.signal_using(ops, libc::SIGKILL).is_err() {
                        audit.fail(self.audit_sequence, "owned application KILL failed");
                    }
                }
                if self
                    .lifecycle
                    .kill_at
                    .is_some_and(|at| now.saturating_duration_since(at) >= Duration::from_secs(3))
                {
                    Some("owned application reap deadline expired")
                } else {
                    None
                }
            }
        };
        if let Some(error) = incomplete {
            audit.fail(self.audit_sequence, error);
            self.lifecycle.completion = Completion::Incomplete(error);
        }
    }
}
#[derive(Default)]
pub struct OwnedApplications {
    applications: BTreeMap<Instance, Application>,
    pub audit: LaunchAudit,
    closing: bool,
}
pub enum LaunchDisposition {
    Created { pid: i32, launch_failed: bool },
}
impl OwnedApplications {
    pub fn applications(&self) -> &BTreeMap<Instance, Application> {
        &self.applications
    }
    pub fn poll(&mut self) {
        let now = Instant::now();
        for app in self.applications.values_mut() {
            app.service(now, self.closing, &mut self.audit, &mut UnixWait);
        }
    }
    pub fn begin_shutdown(&mut self) {
        self.closing = true;
    }
    pub fn shutdown_complete(&self) -> bool {
        self.closing
            && self
                .applications
                .values()
                .all(|a| a.completion() == Completion::Complete)
    }
    pub fn shutdown_incomplete(&self) -> bool {
        self.applications
            .values()
            .any(|a| matches!(a.completion(), Completion::Incomplete(_)))
    }
    /// Cleanup completeness is deliberately independent of launch/readiness
    /// outcome. A caller must not turn a cleaned-up failed attempt into PASS.
    pub fn has_failures(&self) -> bool {
        self.audit.error().is_some()
            || self.audit.attempts().iter().any(|a| a.failure.is_some())
            || self.applications.values().any(|a| {
                a.launch_failure.is_some()
                    || a.status.error().is_some()
                    || matches!(a.completion(), Completion::Incomplete(_))
            })
    }
    pub fn refuse_unadmitted_request(&mut self, request: u64) -> Result<(), &'static str> {
        let sequence = self.audit.begin(Some(request), None)?;
        self.audit
            .fail(sequence, "native spawn/app runtime admission pending");
        Ok(())
    }
    pub fn launch(
        &mut self,
        instance: Instance,
        launch: &Launch<'_>,
        status: UnixStream,
    ) -> Result<LaunchDisposition, SpawnFailure> {
        #[cfg(all(feature = "native-managed-spawn", target_vendor = "apple"))]
        {
            // Source/link permission is not runtime permission. A future
            // explicitly admitted session must bind private diagnostics first.
            if !self.audit.durable() {
                return Err(SpawnFailure::InvalidResult);
            }
            return self.launch_using(instance, launch, status, &mut FrozenDarwinShim);
        }
        #[cfg(not(all(feature = "native-managed-spawn", target_vendor = "apple")))]
        self.launch_using(instance, launch, status, &mut UnavailableShim)
    }
    fn launch_using(
        &mut self,
        instance: Instance,
        launch: &Launch<'_>,
        status: UnixStream,
        api: &mut impl SpawnApi,
    ) -> Result<LaunchDisposition, SpawnFailure> {
        let sequence = self
            .audit
            .begin(None, Some(instance.as_str()))
            .map_err(|_| SpawnFailure::InvalidResult)?;
        self.launch_at(
            sequence,
            instance,
            launch,
            status,
            Duration::from_secs(120),
            api,
        )
    }
    pub(crate) fn launch_prepared(
        &mut self,
        sequence: usize,
        instance: Instance,
        launch: &Launch<'_>,
        status: UnixStream,
        lifetime: Duration,
    ) -> Result<LaunchDisposition, SpawnFailure> {
        #[cfg(all(feature = "native-managed-spawn", target_vendor = "apple"))]
        return self.launch_at(
            sequence,
            instance,
            launch,
            status,
            lifetime,
            &mut FrozenDarwinShim,
        );
        #[cfg(not(all(feature = "native-managed-spawn", target_vendor = "apple")))]
        self.launch_at(
            sequence,
            instance,
            launch,
            status,
            lifetime,
            &mut UnavailableShim,
        )
    }
    fn launch_at(
        &mut self,
        sequence: usize,
        instance: Instance,
        launch: &Launch<'_>,
        status: UnixStream,
        lifetime: Duration,
        api: &mut impl SpawnApi,
    ) -> Result<LaunchDisposition, SpawnFailure> {
        if lifetime < Duration::from_secs(1)
            || lifetime > Duration::from_secs(120)
            || self
                .audit
                .attempts()
                .get(sequence)
                .is_none_or(|a| a.instance.as_deref() != Some(instance.as_str()))
        {
            return Err(SpawnFailure::InvalidResult);
        }
        // Refuse before calling the leaf. Unknown ownership continues to hold
        // a slot; only observed terminal wait state frees the live limit.
        if self.closing
            || self.applications.contains_key(&instance)
            || self.applications.len() >= 128
            || self
                .applications
                .values()
                .filter(|a| !matches!(a.child.state(), WaitState::Reaped(_)))
                .count()
                >= MAX_APPLICATIONS
        {
            self.audit
                .fail(sequence, "identity reused or application bound reached");
            return Err(SpawnFailure::InvalidResult);
        }
        self.audit.mark(sequence, Gate::Admission);
        if status.set_nonblocking(true).is_err() {
            self.audit.fail(sequence, "status nonblocking setup failed");
            return Err(SpawnFailure::InvalidResult);
        }
        self.audit.mark(sequence, Gate::StatusNonblocking);
        if self.audit.error().is_some() {
            return Err(SpawnFailure::InvalidResult);
        }
        let attempt = spawn_using(api, launch);
        self.audit.spawn(
            sequence,
            attempt.child.as_ref().map(OwnedChild::pid),
            attempt.native,
        );
        if let Some(failure) = &attempt.failure {
            self.audit.fail(
                sequence,
                match failure {
                    SpawnFailure::Unsupported => "native spawn shim unavailable",
                    SpawnFailure::InvalidResult => "invalid C spawn result",
                    SpawnFailure::Native { .. } => "C spawn or local cleanup failure",
                },
            );
        }
        match attempt.child {
            Some(child) => {
                let pid = child.pid();
                let observed = Status::new(instance.clone(), pid)
                    .expect("positive PID checked before ownership transfer");
                let failed = attempt.failure.is_some();
                self.applications.insert(
                    instance,
                    Application {
                        child,
                        status: observed,
                        launch_failure: attempt.failure,
                        stream: status,
                        audit_sequence: sequence,
                        lifecycle: Lifecycle::new(Instant::now(), lifetime),
                    },
                );
                Ok(LaunchDisposition::Created {
                    pid,
                    launch_failed: failed,
                })
            }
            None => Err(attempt.failure.unwrap_or(SpawnFailure::InvalidResult)),
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::{io::Write, os::fd::AsFd};
    fn id() -> Instance {
        Instance::parse(&"a".repeat(48)).unwrap()
    }
    fn line(kind: &str, suffix: &str) -> String {
        format!("CHONKTERM/1 {kind} {} 123{suffix}\n", id().as_str())
    }
    #[test]
    fn identity_and_bounds() {
        for bad in ["", &"a".repeat(47), &"A".repeat(48), &"g".repeat(48)] {
            assert!(Instance::parse(bad).is_err());
        }
        let mut s = Status::new(id(), 123).unwrap();
        assert!(s.feed(&vec![b'a'; 256]).is_ok());
        assert!(s.feed(b"\n").is_err());
    }
    #[test]
    fn fragmented_ready_cleanup_and_eof() {
        let all = line("READY", "") + &line("CLEANUP", " 1 9 1 1");
        for split in 0..=all.len() {
            let mut s = Status::new(id(), 123).unwrap();
            s.feed(&all.as_bytes()[..split]).unwrap();
            s.feed(&all.as_bytes()[split..]).unwrap();
            assert!(s.ready());
            assert_eq!(s.cleanup().unwrap().raw_wait_status, 9);
            s.finish().unwrap();
        }
    }
    #[test]
    fn cleanup_without_ready_and_no_fake_completion() {
        let mut s = Status::new(id(), 123).unwrap();
        s.feed(line("CLEANUP", " 0 -1 1 0").as_bytes()).unwrap();
        s.finish().unwrap();
        assert!(!s.ready());
        assert!(!s.cleanup().unwrap().complete);
        for suffix in [
            " 1 -1 1 1",
            " 2 0 0 0",
            " 1 0 0 2",
            " 1 0 0",
            " 1 00 0 0",
            " 1 +0 0 0",
            " 1 0 0 0 ",
        ] {
            assert!(
                Status::new(id(), 123)
                    .unwrap()
                    .feed(line("CLEANUP", suffix).as_bytes())
                    .is_err(),
                "{suffix}"
            );
        }
    }
    #[test]
    fn sticky_duplicates_truncation_foreign_identity() {
        let ready = line("READY", "");
        for data in [
            ready.clone() + &ready,
            ready.replace("123", "124"),
            ready.replace(&"a".repeat(48), &"b".repeat(48)),
            ready.replace('\n', "\r\n"),
            line("CLEANUP", " 1 0 0 0") + &ready,
        ] {
            let mut s = Status::new(id(), 123).unwrap();
            assert!(s.feed(data.as_bytes()).is_err());
            assert!(s.feed(line("CLEANUP", " 1 0 0 0").as_bytes()).is_err());
        }
        for data in ["", ready.trim_end(), &ready] {
            let mut s = Status::new(id(), 123).unwrap();
            s.feed(data.as_bytes()).unwrap();
            assert!(s.finish().is_err());
        }
    }
    #[test]
    fn real_unix_stream_fragmentation() {
        let (mut read, mut write) = UnixStream::pair().unwrap();
        read.set_nonblocking(true).unwrap();
        let mut s = Status::new(id(), 123).unwrap();
        let all = line("READY", "") + &line("CLEANUP", " 1 0 0 0");
        for b in all.bytes() {
            write.write_all(&[b]).unwrap();
            s.drain(&mut read).unwrap();
        }
        drop(write);
        s.drain(&mut read).unwrap();
        assert!(s.ready());
        assert!(s.cleanup().unwrap().complete);
    }
    struct FakeSpawn {
        result: SpawnResultV1,
        returned: i32,
    }
    impl SpawnApi for FakeSpawn {
        fn call(&mut self, req: &SpawnRequestV1) -> Option<(i32, SpawnResultV1)> {
            assert_eq!(req.version, 1);
            assert_eq!(req.argc, 2);
            assert!(req.source_fds.iter().all(|fd| *fd >= 0));
            unsafe {
                assert!((*req.argv.add(2)).is_null());
                assert!((*req.envp).is_null());
            }
            Some((self.returned, self.result))
        }
    }
    fn successful() -> SpawnResultV1 {
        SpawnResultV1 {
            version: 1,
            stage: 10,
            pid: 123,
            child_created: 1,
            duplicate_count: 4,
            requested_flags: 0x400e,
            observed_flags: 0x400e,
            ..Default::default()
        }
    }
    #[test]
    fn error_after_spawn_keeps_exact_child() {
        let file = std::fs::File::open("/dev/null").unwrap();
        let launch = Launch::new("/reviewed/app", &["arg".into()], &[], [file.as_fd(); 4]).unwrap();
        for field in 0..5 {
            let mut result = successful();
            match field {
                0 => result.attr_destroy_error = 5,
                1 => result.actions_destroy_error = 5,
                2 => {
                    result.fd_close_error = 5;
                    result.fd_close_failures = 1
                }
                3 => result.observed_flags = 0,
                4 => result.version = 99,
                _ => unreachable!(),
            }
            let attempt = spawn_using(
                &mut FakeSpawn {
                    result,
                    returned: 5,
                },
                &launch,
            );
            assert_eq!(attempt.child.unwrap().pid(), 123);
            assert!(attempt.failure.is_some());
        }
        let attempt = spawn_using(
            &mut FakeSpawn {
                result: successful(),
                returned: 0,
            },
            &launch,
        );
        assert!(attempt.failure.is_none());
        assert_eq!(attempt.child.unwrap().pid(), 123);
        let unsupported = NativeSpawner.spawn(&launch);
        assert!(unsupported.child.is_none());
        assert!(matches!(
            unsupported.failure,
            Some(SpawnFailure::Unsupported)
        ));
    }
    struct FakeWait {
        result: Result<Option<i32>, i32>,
        signals: Vec<(i32, i32)>,
    }
    impl WaitApi for FakeWait {
        fn wait(&mut self, _: i32) -> Result<Option<i32>, i32> {
            self.result
        }
        fn signal(&mut self, pid: i32, sig: i32) -> Result<(), i32> {
            self.signals.push((pid, sig));
            Ok(())
        }
    }
    #[test]
    fn exclusive_wait_owner_no_signals_after_reap_or_loss() {
        for outcome in [Ok(Some(0)), Err(libc::ECHILD), Err(libc::EIO)] {
            let mut child = OwnedChild::new(123).unwrap();
            let mut ops = FakeWait {
                result: outcome,
                signals: vec![],
            };
            child.signal_using(&mut ops, libc::SIGTERM).unwrap();
            child.poll_using(&mut ops);
            assert!(child.signal_using(&mut ops, libc::SIGKILL).is_err());
            assert_eq!(ops.signals, [(123, libc::SIGTERM)]);
        }
        let mut child = OwnedChild::new(123).unwrap();
        let mut ops = FakeWait {
            result: Err(libc::EINTR),
            signals: vec![],
        };
        assert_eq!(child.poll_using(&mut ops), WaitState::Running);
    }
    #[test]
    fn manager_keeps_child_on_error_and_refuses_identity_reuse() {
        let file = std::fs::File::open("/dev/null").unwrap();
        let launch = Launch::new("/reviewed/app", &["arg".into()], &[], [file.as_fd(); 4]).unwrap();
        let (mut manager, mut shim) = (
            OwnedApplications::default(),
            FakeSpawn {
                result: SpawnResultV1 {
                    attr_destroy_error: 5,
                    ..successful()
                },
                returned: 5,
            },
        );
        let (status, _peer) = UnixStream::pair().unwrap();
        assert!(matches!(
            manager.launch_using(id(), &launch, status, &mut shim),
            Ok(LaunchDisposition::Created {
                pid: 123,
                launch_failed: true
            })
        ));
        assert_eq!(manager.applications()[&id()].child.pid(), 123);
        assert!(manager.applications()[&id()].launch_failure.is_some());
        assert!(manager.has_failures());
        let (status, _peer) = UnixStream::pair().unwrap();
        assert!(manager
            .launch_using(id(), &launch, status, &mut shim)
            .is_err());
        // Fake child is never passed to actual waitpid/kill in this host test.
    }
    #[test]
    fn cleanup_requires_eof_and_reap_independently() {
        let (status, _peer) = UnixStream::pair().unwrap();
        let mut app = Application {
            child: OwnedChild::new(123).unwrap(),
            status: Status::new(id(), 123).unwrap(),
            launch_failure: None,
            stream: status,
            audit_sequence: 0,
            lifecycle: Lifecycle::new(Instant::now(), Duration::from_secs(120)),
        };
        app.status
            .feed(line("CLEANUP", " 1 9 1 1").as_bytes())
            .unwrap();
        assert!(!app.cleanup_complete());
        app.child.state = WaitState::Reaped(0);
        assert!(!app.cleanup_complete());
        app.status.finish().unwrap();
        assert!(app.cleanup_complete());
        app.status.feed(b"x").unwrap_err();
        assert!(!app.cleanup_complete());
    }
    #[test]
    fn native_unavailable_and_bad_strings_create_no_owned_child() {
        let file = std::fs::File::open("/dev/null").unwrap();
        assert!(Launch::new("relative", &[], &[], [file.as_fd(); 4]).is_err());
        assert!(Launch::new("/good", &["nul\0bad".into()], &[], [file.as_fd(); 4]).is_err());
        assert!(Launch::new("/good", &["a".repeat(1024)], &[], [file.as_fd(); 4]).is_err());
        let launch = Launch::new("/reviewed/app", &[], &[], [file.as_fd(); 4]).unwrap();
        let (status, _peer) = UnixStream::pair().unwrap();
        let mut manager = OwnedApplications::default();
        assert!(matches!(
            manager.launch(id(), &launch, status),
            Err(SpawnFailure::Unsupported)
        ));
        assert!(manager.applications().is_empty());
        assert!(manager.has_failures());
        assert_eq!(manager.audit.attempts().len(), 1);
        assert_eq!(
            manager.audit.attempts()[0].failure,
            Some("native spawn shim unavailable")
        );
        assert!(manager.audit.attempts()[0].gates_ms[Gate::PidOwned as usize].is_none());
    }
    fn fake_application(now: Instant) -> (Application, UnixStream, LaunchAudit) {
        let (stream, peer) = UnixStream::pair().unwrap();
        stream.set_nonblocking(true).unwrap();
        let mut audit = LaunchAudit::default();
        let sequence = audit.begin(None, Some(id().as_str())).unwrap();
        (
            Application {
                child: OwnedChild::new(123).unwrap(),
                status: Status::new(id(), 123).unwrap(),
                launch_failure: None,
                stream,
                audit_sequence: sequence,
                lifecycle: Lifecycle::new(now, Duration::from_secs(120)),
            },
            peer,
            audit,
        )
    }
    #[test]
    fn no_ready_has_finite_term_kill_reap_and_keeps_failed_child() {
        let now = Instant::now();
        let (mut app, _peer, mut audit) = fake_application(now);
        let mut ops = FakeWait {
            result: Ok(None),
            signals: vec![],
        };
        for seconds in [0, 24, 25, 25, 29, 30, 30, 32, 33, 40] {
            app.service(
                now + Duration::from_secs(seconds),
                false,
                &mut audit,
                &mut ops,
            );
        }
        assert_eq!(ops.signals, [(123, libc::SIGTERM), (123, libc::SIGKILL)]);
        assert_eq!(
            app.completion(),
            Completion::Incomplete("owned application reap deadline expired")
        );
        assert_eq!(app.child.pid(), 123);
        assert_eq!(app.child.state(), WaitState::Running);
        assert_eq!(
            audit.attempts()[0].failure,
            Some("application READY deadline expired")
        );
        assert!(audit.attempts()[0].gates_ms[Gate::AppReady as usize].is_none());
        ops.result = Ok(Some(9));
        app.service(now + Duration::from_secs(41), true, &mut audit, &mut ops);
        assert_eq!(app.child.state(), WaitState::Reaped(9));
        assert!(matches!(app.completion(), Completion::Incomplete(_)));
        assert_eq!(ops.signals.len(), 2);
    }
    #[test]
    fn normal_close_before_ready_needs_actual_cleanup_eof_and_reap() {
        let now = Instant::now();
        let (mut app, mut peer, mut audit) = fake_application(now);
        peer.write_all(line("CLEANUP", " 1 15 1 0").as_bytes())
            .unwrap();
        let mut ops = FakeWait {
            result: Ok(Some(0)),
            signals: vec![],
        };
        app.service(now, false, &mut audit, &mut ops);
        assert_eq!(app.completion(), Completion::Pending);
        drop(peer);
        app.service(now + Duration::from_secs(1), false, &mut audit, &mut ops);
        assert_eq!(app.completion(), Completion::Complete);
        assert!(!app.status.ready());
        assert!(ops.signals.is_empty());
        assert!(audit.attempts()[0].failure.is_none());
    }
    #[test]
    fn launch_failure_and_end_session_do_not_skip_term_or_invent_cleanup() {
        for failed_launch in [false, true] {
            let now = Instant::now();
            let (mut app, _peer, mut audit) = fake_application(now);
            if failed_launch {
                app.launch_failure = Some(SpawnFailure::InvalidResult);
            }
            let mut ops = FakeWait {
                result: Ok(None),
                signals: vec![],
            };
            app.service(now, !failed_launch, &mut audit, &mut ops);
            assert_eq!(ops.signals, [(123, libc::SIGTERM)]);
            ops.result = Err(libc::ECHILD);
            app.service(now + Duration::from_secs(10), true, &mut audit, &mut ops);
            assert!(matches!(app.completion(), Completion::Incomplete(_)));
            assert_eq!(ops.signals.len(), 1);
            assert!(!app.cleanup_complete());
        }
    }
}
