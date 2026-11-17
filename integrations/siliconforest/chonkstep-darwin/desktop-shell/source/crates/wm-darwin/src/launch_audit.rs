//! Bounded manager-owned observations, including attempts that create no child.
//! READY is an application assertion. Version 1 does not expose the individual
//! PTY/keymap/SSD/frame gates on fd3; those stay unknown here, never guessed.
use std::{
    fs::File,
    io::{self, Write},
    os::{fd::AsRawFd, unix::fs::MetadataExt},
    time::Instant,
};

pub const MAX_ATTEMPTS: usize = 128;
const MAX_JOURNAL_BYTES: usize = 2 * 1024 * 1024;
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
#[repr(usize)]
pub enum Gate {
    Admission,
    StatusNonblocking,
    SpawnReturned,
    PidOwned,
    AppReady,
    AppCleanup,
    StatusEof,
    AppReaped,
    TermAttempt,
    KillAttempt,
}
const GATES: [Gate; 10] = [
    Gate::Admission,
    Gate::StatusNonblocking,
    Gate::SpawnReturned,
    Gate::PidOwned,
    Gate::AppReady,
    Gate::AppCleanup,
    Gate::StatusEof,
    Gate::AppReaped,
    Gate::TermAttempt,
    Gate::KillAttempt,
];
#[derive(Clone, Debug)]
pub struct Attempt {
    pub sequence: usize,
    pub shell_request: Option<u64>,
    pub instance: Option<String>,
    pub pid: Option<i32>,
    pub created_ms: u64,
    /// First actual observation time, relative to this manager's origin.
    pub gates_ms: [Option<u64>; 10],
    pub failure: Option<&'static str>,
    pub failure_ms: Option<u64>,
    pub status_bytes: usize,
    pub raw_app_wait_status: Option<i32>,
    pub wait_error: Option<i32>,
    pub cleanup: Option<crate::applications::Cleanup>,
    pub native_return: Option<i32>,
    pub native_result: Option<crate::applications::SpawnResultV1>,
}
pub struct LaunchAudit {
    origin: Instant,
    attempts: Vec<Attempt>,
    journal: Option<File>,
    written: usize,
    failure: Option<&'static str>,
}
impl Default for LaunchAudit {
    fn default() -> Self {
        Self {
            origin: Instant::now(),
            attempts: vec![],
            journal: None,
            written: 0,
            failure: None,
        }
    }
}
impl LaunchAudit {
    /// The caller creates this file exclusively inside its owned private
    /// directory. Adoption checks the actual FD, never reopens a pathname.
    /// May only be bound before any requests; a late attachment loses evidence.
    pub fn attach_private_journal(&mut self, file: File) -> io::Result<()> {
        let m = file.metadata()?;
        let fd = file.as_raw_fd();
        let flags = unsafe { libc::fcntl(fd, libc::F_GETFL) };
        if !self.attempts.is_empty()
            || self.journal.is_some()
            || self.failure.is_some()
            || !m.is_file()
            || m.mode() & 0o7777 != 0o600
            || m.nlink() != 1
            || m.uid() != unsafe { libc::geteuid() }
            || m.len() != 0
            || flags < 0
            || flags & libc::O_ACCMODE == libc::O_RDONLY
        {
            return Err(io::Error::other("invalid private launch journal"));
        }
        let fdflags = unsafe { libc::fcntl(fd, libc::F_GETFD) };
        if fdflags < 0 || unsafe { libc::fcntl(fd, libc::F_SETFD, fdflags | libc::FD_CLOEXEC) } < 0
        {
            return Err(io::Error::last_os_error());
        }
        self.journal = Some(file);
        self.append(
            "CHONKLAUNCH/1 scope=manager app_internal_gates=unknown_until_final_app_record\n"
                .into(),
        );
        self.failure.map_or(Ok(()), |e| Err(io::Error::other(e)))
    }
    pub fn attempts(&self) -> &[Attempt] {
        &self.attempts
    }
    pub fn error(&self) -> Option<&'static str> {
        self.failure
    }
    pub fn durable(&self) -> bool {
        self.journal.is_some() && self.failure.is_none()
    }
    fn now_ms(&self) -> u64 {
        self.origin.elapsed().as_millis().min(u64::MAX as u128) as u64
    }
    pub fn begin(
        &mut self,
        shell_request: Option<u64>,
        instance: Option<&str>,
    ) -> Result<usize, &'static str> {
        if self.failure.is_some() || self.attempts.len() == MAX_ATTEMPTS {
            self.failure
                .get_or_insert("launch diagnostic capacity exhausted or journal failed");
            return Err(self.failure.unwrap());
        }
        // Only bounded validated identifiers can enter the journal.
        if instance.is_some_and(|s| crate::applications::Instance::parse(s).is_err()) {
            return Err("invalid launch diagnostic identity");
        }
        let sequence = self.attempts.len();
        let now = self.now_ms();
        self.attempts.push(Attempt {
            sequence,
            shell_request,
            instance: instance.map(str::to_owned),
            pid: None,
            created_ms: now,
            gates_ms: [None; 10],
            failure: None,
            failure_ms: None,
            status_bytes: 0,
            raw_app_wait_status: None,
            wait_error: None,
            cleanup: None,
            native_return: None,
            native_result: None,
        });
        self.append(format!("CHONKLAUNCH/1 attempt={sequence} ms={now} request={shell_request:?} instance={instance:?} event=requested\n"));
        Ok(sequence)
    }
    pub fn mark(&mut self, sequence: usize, gate: Gate) {
        let now = self.now_ms();
        let a = &mut self.attempts[sequence];
        if a.gates_ms[gate as usize].is_none() {
            a.gates_ms[gate as usize] = Some(now);
            self.append(format!(
                "CHONKLAUNCH/1 attempt={sequence} ms={now} event={gate:?}\n"
            ));
        }
    }
    pub fn fail(&mut self, sequence: usize, reason: &'static str) {
        let now = self.now_ms();
        let a = &mut self.attempts[sequence];
        if a.failure.is_none() {
            a.failure = Some(reason);
            a.failure_ms = Some(now);
            self.append(format!(
                "CHONKLAUNCH/1 attempt={sequence} ms={now} failure={reason:?}\n"
            ));
        }
    }
    pub fn spawn(
        &mut self,
        sequence: usize,
        pid: Option<i32>,
        native: Option<(i32, crate::applications::SpawnResultV1)>,
    ) {
        let a = &mut self.attempts[sequence];
        a.pid = pid;
        if let Some((returned, result)) = native {
            a.native_return = Some(returned);
            a.native_result = Some(result);
            self.mark(sequence, Gate::SpawnReturned);
            self.append(format!(
                "CHONKLAUNCH/1 attempt={sequence} native_return={returned} native={result:?}\n"
            ));
        }
        if pid.is_some() {
            self.mark(sequence, Gate::PidOwned);
        }
    }
    pub fn observe(
        &mut self,
        sequence: usize,
        status: &crate::applications::Status,
        wait: crate::applications::WaitState,
    ) {
        use crate::applications::WaitState;
        let a = &self.attempts[sequence];
        let before = (
            a.gates_ms,
            a.failure,
            a.raw_app_wait_status,
            a.wait_error,
            a.cleanup,
        );
        // Observation gates remain true even if a later malformed frame makes
        // the outcome fail. They are not an overall readiness/success verdict.
        if status.ready_observed() {
            self.mark(sequence, Gate::AppReady);
        }
        if status.cleanup().is_some() {
            self.mark(sequence, Gate::AppCleanup);
        }
        if status.eof_observed() {
            self.mark(sequence, Gate::StatusEof);
        }
        if let Some(e) = status.error() {
            self.fail(sequence, e);
        }
        if let WaitState::Reaped(raw) = wait {
            self.mark(sequence, Gate::AppReaped);
            self.attempts[sequence].raw_app_wait_status = Some(raw);
        }
        if let WaitState::Unknown(error) = wait {
            self.attempts[sequence].wait_error = Some(error);
            self.fail(sequence, "exclusive child wait ownership lost");
        }
        self.attempts[sequence].status_bytes = status.bytes_observed();
        self.attempts[sequence].cleanup = status.cleanup();
        let a = &self.attempts[sequence];
        if before
            != (
                a.gates_ms,
                a.failure,
                a.raw_app_wait_status,
                a.wait_error,
                a.cleanup,
            )
        {
            self.append(format!("CHONKLAUNCH/1 attempt={sequence} status_bytes={} raw_app_wait={:?} wait_error={:?} cleanup={:?}\n", a.status_bytes, a.raw_app_wait_status, a.wait_error, a.cleanup));
        }
    }
    /// Bounded final snapshot; partial pending status is counted but never
    /// interpreted or emitted as text. No PTY contents, environment or paths.
    pub fn snapshot(&self, out: &mut impl Write) -> io::Result<()> {
        writeln!(out, "CHONKLAUNCH/1 snapshot attempts={} journal_error={:?} app_internal_gates=unknown_until_final_app_record", self.attempts.len(), self.failure)?;
        for a in &self.attempts {
            writeln!(out, "attempt={} request={:?} instance={:?} pid={:?} created_ms={} failure={:?} failure_ms={:?} status_bytes={} wait={:?} wait_error={:?} cleanup={:?} native_return={:?} native={:?}", a.sequence, a.shell_request, a.instance, a.pid, a.created_ms, a.failure, a.failure_ms, a.status_bytes, a.raw_app_wait_status, a.wait_error, a.cleanup, a.native_return, a.native_result)?;
            for gate in GATES {
                writeln!(
                    out,
                    "attempt={} gate={gate:?} observed_ms={:?}",
                    a.sequence, a.gates_ms[gate as usize]
                )?;
            }
        }
        Ok(())
    }
    fn append(&mut self, line: String) {
        if self.failure.is_some() {
            return;
        }
        if line.len() > 1024 || self.written.saturating_add(line.len()) > MAX_JOURNAL_BYTES {
            self.failure = Some("launch journal bound exceeded");
            return;
        }
        if let Some(file) = self.journal.as_mut() {
            // Each accepted gate reaches the private file before the next
            // launch side effect. Write/sync failure is sticky; never truncate.
            if file
                .write_all(line.as_bytes())
                .and_then(|_| file.sync_data())
                .is_err()
            {
                self.failure = Some("launch journal write or sync failed");
                return;
            }
            self.written += line.len();
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::applications::{Instance, Status, WaitState};
    use std::os::unix::fs::PermissionsExt;
    fn status() -> Status {
        Status::new(Instance::parse(&"a".repeat(48)).unwrap(), 123).unwrap()
    }
    #[test]
    fn refusal_capacity_preserves_all_earlier_attempts() {
        let mut audit = LaunchAudit::default();
        for i in 0..MAX_ATTEMPTS {
            let key = audit.begin(Some(i as u64 + 1), None).unwrap();
            audit.fail(key, "dependency unavailable");
            audit.fail(key, "later failure must not erase first");
        }
        assert!(audit.begin(Some(129), None).is_err());
        assert_eq!(audit.attempts().len(), 128);
        assert_eq!(audit.attempts()[0].failure, Some("dependency unavailable"));
        assert_eq!(audit.attempts()[127].shell_request, Some(128));
        assert!(audit
            .attempts()
            .iter()
            .all(|a| a.pid.is_none() && a.gates_ms.iter().all(Option::is_none)));
        let mut bytes = vec![];
        audit.snapshot(&mut bytes).unwrap();
        assert!(bytes.len() < 128 * 2048);
        assert!(String::from_utf8(bytes)
            .unwrap()
            .contains("app_internal_gates=unknown"));
    }
    #[test]
    fn ready_then_error_and_reap_never_erase_observed_gates() {
        let mut audit = LaunchAudit::default();
        let key = audit.begin(None, Some(&"a".repeat(48))).unwrap();
        let mut state = status();
        state
            .feed(format!("CHONKTERM/1 READY {} 123\n", "a".repeat(48)).as_bytes())
            .unwrap();
        audit.observe(key, &state, WaitState::Running);
        state.feed(b"malformed\n").unwrap_err();
        audit.observe(key, &state, WaitState::Reaped(9));
        let a = &audit.attempts()[key];
        assert!(a.gates_ms[Gate::AppReady as usize].is_some());
        assert!(a.gates_ms[Gate::AppReaped as usize].is_some());
        assert!(a.gates_ms[Gate::AppCleanup as usize].is_none());
        assert!(a.failure.is_some());
        assert_eq!(a.raw_app_wait_status, Some(9));
        assert!(!state.ready());
    }
    #[test]
    fn private_journal_contains_gate_failure_and_actual_cleanup() {
        let file = tempfile::tempfile().unwrap();
        // Anonymous/unlinked files are intentionally refused (nlink==0).
        let mut audit = LaunchAudit::default();
        assert!(audit.attach_private_journal(file).is_err());
        let dir = tempfile::tempdir().unwrap();
        let path = dir.path().join("manager-status.log");
        let file = File::create_new(&path).unwrap();
        file.set_permissions(std::fs::Permissions::from_mode(0o600))
            .unwrap();
        audit.attach_private_journal(file).unwrap();
        let key = audit.begin(Some(1), Some(&"a".repeat(48))).unwrap();
        audit.mark(key, Gate::Admission);
        let mut state = status();
        state
            .feed(format!("CHONKTERM/1 CLEANUP {} 123 0 -1 1 0\n", "a".repeat(48)).as_bytes())
            .unwrap();
        state.finish().unwrap();
        audit.observe(key, &state, WaitState::Reaped(256));
        audit.fail(key, "application never became ready");
        let text = std::fs::read_to_string(&path).unwrap();
        assert!(text.contains("event=Admission") && text.contains("event=AppCleanup"));
        assert!(text.contains("raw_app_wait=Some(256)") && text.contains("complete: false"));
        assert!(text.contains("application never became ready"));
        assert!(!text.contains("event=AppReady"));
        let size = std::fs::metadata(&path).unwrap().len();
        for _ in 0..100 {
            audit.observe(key, &state, WaitState::Reaped(256));
        }
        assert_eq!(std::fs::metadata(path).unwrap().len(), size);
    }
    #[test]
    fn failed_journal_is_sticky_but_does_not_drop_existing_state() {
        let mut audit = LaunchAudit::default();
        let key = audit.begin(Some(1), None).unwrap();
        audit.mark(key, Gate::PidOwned);
        // Force the real append quota boundary without huge allocations.
        audit.written = MAX_JOURNAL_BYTES;
        audit.fail(key, "child cleanup failed");
        assert_eq!(audit.error(), Some("launch journal bound exceeded"));
        assert!(audit.begin(Some(2), None).is_err());
        assert_eq!(audit.attempts()[0].failure, Some("child cleanup failed"));
        assert!(audit.attempts()[0].gates_ms[Gate::PidOwned as usize].is_some());
    }
}
