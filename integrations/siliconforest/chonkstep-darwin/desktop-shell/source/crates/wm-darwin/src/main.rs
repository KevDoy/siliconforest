use std::{
    os::unix::fs::MetadataExt,
    path::PathBuf,
    sync::atomic::{AtomicBool, Ordering},
    time::{Duration, Instant},
};
use wm_darwin::smithay::reexports::wayland_server::{Display, ListeningSocket};
use wm_darwin::{presenter::Pdgop, server::Server};
use wm_darwin::hid::{self, NativeHid};
static STOP: AtomicBool = AtomicBool::new(false);
extern "C" fn stop(_: libc::c_int) {
    STOP.store(true, Ordering::Relaxed);
}

fn run() -> Result<(), String> {
    let args: Vec<String> = std::env::args().skip(1).collect();
    let (path, input_mode) = match hid::parse_cli(&args)? {
        hid::Cli::Help => {
            println!("{}", hid::usage_text());
            return Ok(());
        }
        hid::Cli::Run { socket, input } => (PathBuf::from(socket), input),
    };
    if !path.is_absolute() {
        return Err("socket must be an absolute path".into());
    }
    let parent = path.parent().ok_or("socket parent missing")?;
    let meta = std::fs::symlink_metadata(parent).map_err(|e| e.to_string())?;
    if !meta.is_dir() || meta.uid() != unsafe { libc::geteuid() } || meta.mode() & 0o077 != 0 {
        return Err(
            "socket directory must be owned by this uid, a directory, and mode 0700".into(),
        );
    }
    let listener = ListeningSocket::bind_absolute(path).map_err(|e| e.to_string())?;
    let mut display = Display::<Server>::new().map_err(|e| e.to_string())?;
    let mut input = match input_mode {
        hid::InputMode::Off => None,
        hid::InputMode::Both => Some(NativeHid::open()?),
        hid::InputMode::KeyboardOnly => Some(NativeHid::open_keyboard()?),
    };
    let mut presenter = Pdgop::open()?;
    let mut server = Server::new(&display.handle(), presenter.size())?;
    if let Some(queues) = &input {
        server.enable_pdhid_seat_caps(&display.handle(), queues.seat_caps())?;
        eprintln!("{}", queues.qualification_note());
    }
    // Signals only stop the main loop. Drawing stops before PDGOP is unmapped;
    // no drawing thread, kernel reboot, or storage action exists here.
    unsafe {
        if libc::signal(libc::SIGINT, stop as *const () as libc::sighandler_t) == libc::SIG_ERR
            || libc::signal(libc::SIGTERM, stop as *const () as libc::sighandler_t) == libc::SIG_ERR
        {
            return Err(format!(
                "signal handler installation failed: {}",
                std::io::Error::last_os_error()
            ));
        }
    }
    let start = Instant::now();
    while !STOP.load(Ordering::Relaxed) {
        while let Some(stream) = listener.accept().map_err(|e| e.to_string())? {
            Server::add_client(&mut display, stream).map_err(|e| e.to_string())?;
        }
        display
            .dispatch_clients(&mut server)
            .map_err(|e| e.to_string())?;
        if let Some(input)=&mut input{for event in input.drain()?{server.deliver_hid(event,start.elapsed().as_millis() as u32)?;}}
        server.repaint(&mut presenter, start.elapsed().as_millis() as u32)?;
        display.flush_clients().map_err(|e| e.to_string())?;
        std::thread::sleep(Duration::from_millis(if input.is_some(){8}else{16}));
    }
    Ok(())
}
fn main() {
    if let Err(error) = run() {
        eprintln!("chonkstep-darwin: {error}");
        std::process::exit(1);
    }
}
