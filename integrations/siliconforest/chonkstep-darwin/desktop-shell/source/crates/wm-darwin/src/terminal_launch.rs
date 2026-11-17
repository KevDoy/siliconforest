//! Private launch inputs for the agreed ordinary terminal; no arbitrary command.
//! This module does not select/approve executable bytes or create a native CLI.
use crate::applications::{Instance, Launch, LaunchDisposition, OwnedApplications};
use std::{
    ffi::CString,
    fs::File,
    io,
    os::{
        fd::{AsFd, AsRawFd, FromRawFd},
        unix::{fs::MetadataExt, net::UnixStream},
    },
    path::{Component, Path, PathBuf},
    time::{Duration, Instant},
};

pub struct TerminalLauncher {
    binary: String,
    socket: String,
    records: PrivateDirectory,
    session: Instance,
    next: u64,
    end: Instant,
}
struct PrivateDirectory {
    file: File,
    path: PathBuf,
}
impl PrivateDirectory {
    fn open(path: &Path) -> io::Result<Self> {
        checked_path(path)?;
        let c = CString::new(path.as_os_str().as_encoded_bytes()).map_err(io::Error::other)?;
        let fd = unsafe {
            libc::open(
                c.as_ptr(),
                libc::O_RDONLY | libc::O_DIRECTORY | libc::O_NOFOLLOW | libc::O_CLOEXEC,
            )
        };
        if fd < 0 {
            return Err(io::Error::last_os_error());
        }
        Self::check(unsafe { File::from_raw_fd(fd) }, path.to_owned())
    }
    fn check(file: File, path: PathBuf) -> io::Result<Self> {
        let m = file.metadata()?;
        if !m.is_dir() || m.uid() != unsafe { libc::geteuid() } || m.mode() & 0o7777 != 0o700 {
            return Err(io::Error::other("records directory must be owned mode0700"));
        }
        Ok(Self { file, path })
    }
    fn verify_name(&self) -> io::Result<()> {
        let open = self.file.metadata()?;
        let named = std::fs::symlink_metadata(&self.path)?;
        if !named.is_dir()
            || named.ino() != open.ino()
            || named.dev() != open.dev()
            || named.uid() != unsafe { libc::geteuid() }
            || named.mode() & 0o7777 != 0o700
        {
            return Err(io::Error::other("private directory name changed"));
        }
        Ok(())
    }
    fn child(&self, instance: &Instance) -> io::Result<Self> {
        self.verify_name()?;
        let c = CString::new(instance.as_str()).unwrap();
        if unsafe { libc::mkdirat(self.file.as_raw_fd(), c.as_ptr(), 0o700) } != 0 {
            return Err(io::Error::last_os_error());
        }
        let fd = unsafe {
            libc::openat(
                self.file.as_raw_fd(),
                c.as_ptr(),
                libc::O_RDONLY | libc::O_DIRECTORY | libc::O_NOFOLLOW | libc::O_CLOEXEC,
            )
        };
        if fd < 0 {
            return Err(io::Error::last_os_error());
        }
        Self::check(
            unsafe { File::from_raw_fd(fd) },
            self.path.join(instance.as_str()),
        )
    }
    fn file(&self, name: &'static str) -> io::Result<File> {
        let c = CString::new(name).unwrap();
        let fd = unsafe {
            libc::openat(
                self.file.as_raw_fd(),
                c.as_ptr(),
                libc::O_WRONLY | libc::O_CREAT | libc::O_EXCL | libc::O_NOFOLLOW | libc::O_CLOEXEC,
                0o600,
            )
        };
        if fd < 0 {
            return Err(io::Error::last_os_error());
        }
        let file = unsafe { File::from_raw_fd(fd) };
        let m = file.metadata()?;
        if !m.is_file()
            || m.nlink() != 1
            || m.uid() != unsafe { libc::geteuid() }
            || m.mode() & 0o7777 != 0o600
        {
            return Err(io::Error::other("private launch file invariant failed"));
        }
        Ok(file)
    }
}
fn checked_path(path: &Path) -> io::Result<&str> {
    let text = path
        .to_str()
        .ok_or_else(|| io::Error::other("non-UTF8 launch path"))?;
    if !path.is_absolute()
        || text.len() > 900
        || text.as_bytes().contains(&0)
        || path
            .components()
            .any(|c| matches!(c, Component::ParentDir | Component::CurDir))
    {
        return Err(io::Error::other("invalid bounded absolute launch path"));
    }
    Ok(text)
}
struct Prepared {
    instance: Instance,
    directory: PrivateDirectory,
    args: Vec<String>,
    env: Vec<String>,
    stdin: File,
    stdout: File,
    stderr: File,
    parent: UnixStream,
    child: UnixStream,
    seconds: u64,
}
impl TerminalLauncher {
    /// Root's future admitted entry supplies exact artifact-bound paths. This
    /// function neither trusts PATH nor substitutes a different executable.
    /// Runtime authorization remains outside this source-only library API.
    pub fn new(
        binary: &Path,
        socket: &Path,
        records: &Path,
        session: Instance,
        lifetime: Duration,
        manager: &mut OwnedApplications,
    ) -> io::Result<Self> {
        if lifetime < Duration::from_secs(11) || lifetime > Duration::from_secs(180) {
            return Err(io::Error::other(
                "session lifetime must reserve bounded cleanup",
            ));
        }
        let binary = checked_path(binary)?.to_owned();
        let socket = checked_path(socket)?.to_owned();
        let records = PrivateDirectory::open(records)?;
        let journal = records.file("manager-status.log")?;
        manager.audit.attach_private_journal(journal)?;
        Ok(Self {
            binary,
            socket,
            records,
            session,
            next: 0,
            end: Instant::now() + lifetime,
        })
    }
    fn identity(&mut self) -> io::Result<Instance> {
        self.next = self
            .next
            .checked_add(1)
            .ok_or_else(|| io::Error::other("instance counter exhausted"))?;
        Instance::parse(&format!(
            "{}{:016x}",
            &self.session.as_str()[..32],
            self.next
        ))
        .map_err(io::Error::other)
    }
    fn prepare(&self, instance: Instance, now: Instant) -> io::Result<Prepared> {
        let remaining = self.end.saturating_duration_since(now).as_secs();
        let seconds = remaining
            .checked_sub(10)
            .filter(|n| *n >= 1)
            .ok_or_else(|| io::Error::other("session cleanup reserve reached"))?
            .min(120);
        let directory = self.records.child(&instance)?;
        let args = vec![
            "--socket".into(),
            self.socket.clone(),
            "--instance".into(),
            instance.as_str().into(),
            "--records".into(),
            checked_path(&directory.path)?.into(),
            "--seconds".into(),
            seconds.to_string(),
        ];
        let stdin = File::open("/dev/null")?;
        let stdout = directory.file("application-stdout.log")?;
        let stderr = directory.file("application-stderr.log")?;
        let (parent, child) = UnixStream::pair()?;
        for stream in [&parent, &child] {
            let fd = stream.as_raw_fd();
            let flags = unsafe { libc::fcntl(fd, libc::F_GETFD) };
            if flags < 0 || unsafe { libc::fcntl(fd, libc::F_SETFD, flags | libc::FD_CLOEXEC) } < 0
            {
                return Err(io::Error::last_os_error());
            }
        }
        parent.set_nonblocking(true)?;
        Ok(Prepared {
            instance,
            directory,
            args,
            env: vec![
                "LC_ALL=C".into(),
                "PATH=/bin:/usr/bin:/sbin:/usr/sbin".into(),
                "HOME=/private/tmp".into(),
                "TERM=dumb".into(),
            ],
            stdin,
            stdout,
            stderr,
            parent,
            child,
            seconds,
        })
    }
    pub fn request(
        &mut self,
        request: u64,
        manager: &mut OwnedApplications,
    ) -> Result<LaunchDisposition, String> {
        let instance = self.identity().map_err(|e| e.to_string())?;
        let sequence = manager
            .audit
            .begin(Some(request), Some(instance.as_str()))
            .map_err(str::to_owned)?;
        let result = (|| {
            if !manager.audit.durable() {
                return Err("private launch journal unavailable".into());
            }
            let p = self
                .prepare(instance, Instant::now())
                .map_err(|e| e.to_string())?;
            p.directory.verify_name().map_err(|e| e.to_string())?;
            let launch = Launch::new(
                &self.binary,
                &p.args,
                &p.env,
                [
                    p.stdin.as_fd(),
                    p.stdout.as_fd(),
                    p.stderr.as_fd(),
                    p.child.as_fd(),
                ],
            )
            .map_err(str::to_owned)?;
            // C only borrows these FDs. Both child endpoint and all log sources
            // close on every return path; the retained parent receives real EOF.
            manager
                .launch_prepared(
                    sequence,
                    p.instance,
                    &launch,
                    p.parent,
                    Duration::from_secs(p.seconds),
                )
                .map_err(|e| format!("managed spawn refused: {e:?}"))
        })();
        if result.is_err() {
            manager
                .audit
                .fail(sequence, "launch preparation or spawn refused");
        }
        result
    }
    pub fn deadline_reached(&self) -> bool {
        Instant::now() >= self.end
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::os::unix::fs::PermissionsExt;
    fn fixture() -> (tempfile::TempDir, TerminalLauncher, OwnedApplications) {
        let dir = tempfile::tempdir().unwrap();
        std::fs::set_permissions(dir.path(), std::fs::Permissions::from_mode(0o700)).unwrap();
        let mut manager = OwnedApplications::default();
        let launcher = TerminalLauncher::new(
            Path::new("/reviewed/chonk-terminal-darwin"),
            Path::new("/private/session/wayland"),
            dir.path(),
            Instance::parse(&"a".repeat(48)).unwrap(),
            Duration::from_secs(120),
            &mut manager,
        )
        .unwrap();
        (dir, launcher, manager)
    }
    #[test]
    fn exact_frozen_cli_private_fds_and_no_instance_reuse() {
        let (_root, mut launcher, _manager) = fixture();
        let id = launcher.identity().unwrap();
        let prepared = launcher.prepare(id.clone(), Instant::now()).unwrap();
        assert_eq!(
            &prepared.args[..4],
            [
                "--socket",
                "/private/session/wayland",
                "--instance",
                id.as_str()
            ]
        );
        assert_eq!(prepared.args[4], "--records");
        assert_eq!(prepared.args[6], "--seconds");
        assert!((1..=120).contains(&prepared.seconds));
        for file in [&prepared.stdout, &prepared.stderr] {
            let m = file.metadata().unwrap();
            assert_eq!(m.mode() & 0o7777, 0o600);
            assert_eq!(m.nlink(), 1);
        }
        assert!(launcher.prepare(id, Instant::now()).is_err());
        assert_ne!(prepared.instance, launcher.identity().unwrap());
    }
    #[test]
    fn deadline_and_private_directory_refusals_are_before_spawn() {
        let (root, mut launcher, mut manager) = fixture();
        launcher.end = Instant::now();
        assert!(launcher.request(1, &mut manager).is_err());
        assert!(manager.applications().is_empty());
        assert_eq!(manager.audit.attempts().len(), 1);
        assert!(manager.audit.attempts()[0].failure.is_some());
        assert!(std::fs::read_dir(root.path())
            .unwrap()
            .all(|e| e.unwrap().file_name() == "manager-status.log"));
        std::fs::set_permissions(root.path(), std::fs::Permissions::from_mode(0o755)).unwrap();
        assert!(PrivateDirectory::open(root.path()).is_err());
        let link = root.path().with_extension("link");
        std::os::unix::fs::symlink(root.path(), &link).unwrap();
        assert!(PrivateDirectory::open(&link).is_err());
        std::fs::remove_file(link).unwrap();
    }
    #[test]
    fn unadmitted_native_launch_has_no_child_and_keeps_private_failure() {
        let (root, mut launcher, mut manager) = fixture();
        assert!(launcher.request(1, &mut manager).is_err());
        assert!(manager.applications().is_empty());
        assert_eq!(
            manager.audit.attempts()[0].failure,
            Some("native spawn shim unavailable")
        );
        let journal = std::fs::read_to_string(root.path().join("manager-status.log")).unwrap();
        assert!(
            journal.contains("StatusNonblocking")
                && journal.contains("native spawn shim unavailable")
        );
        assert!(!journal.contains("event=PidOwned"));
    }
}
