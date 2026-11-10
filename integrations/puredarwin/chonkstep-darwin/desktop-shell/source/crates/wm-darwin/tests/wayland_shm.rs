#![cfg(feature = "darwin-backend")]
//! Host-only real Wayland wire test; its memory presenter is NOT native evidence.
use std::{
    io::{Read, Seek, SeekFrom, Write},
    os::fd::AsFd,
    os::unix::net::UnixStream,
    sync::{
        atomic::{AtomicBool, Ordering},
        mpsc, Arc,
    },
    thread,
    time::{Duration, Instant},
};
use wayland_client::{
    delegate_noop,
    protocol::{
        wl_buffer, wl_callback, wl_compositor, wl_output, wl_registry, wl_shm, wl_shm_pool,
        wl_surface, wl_seat, wl_keyboard, wl_pointer,
    },
    Connection, Dispatch, QueueHandle,
};
use wayland_protocols::xdg::{
    decoration::zv1::client::{zxdg_decoration_manager_v1, zxdg_toplevel_decoration_v1},
    shell::client::{xdg_surface, xdg_toplevel, xdg_wm_base},
};
use wm_core::Size;
use wm_darwin::smithay::reexports::wayland_server::Display;
use wm_darwin::{presenter::Presenter, server::Server};

#[derive(Default)]
struct Client {
    compositor: Option<wl_compositor::WlCompositor>,
    shm: Option<wl_shm::WlShm>,
    shell: Option<xdg_wm_base::XdgWmBase>,
    decoration: Option<zxdg_decoration_manager_v1::ZxdgDecorationManagerV1>,
    releases: u32,
    frames: u32,
    configures: u32,
    server_decor: bool,
    globals: Vec<String>,
    seat: Option<wl_seat::WlSeat>, keyboard:Option<wl_keyboard::WlKeyboard>,pointer:Option<wl_pointer::WlPointer>,
    keys:Vec<(u32,wayland_client::WEnum<wl_keyboard::KeyState>)>,
    buttons:Vec<(u32,wayland_client::WEnum<wl_pointer::ButtonState>)>,
    keymap:String,keyboard_enters:u32,pointer_serial:Option<u32>,button_serial:Option<u32>,modifier_masks:Vec<u32>,closes:u32,
    held_on_enter:Vec<Vec<u32>>,keyboard_leaves:u32,
}
impl Dispatch<wl_registry::WlRegistry, ()> for Client {
    fn event(
        s: &mut Self,
        r: &wl_registry::WlRegistry,
        e: wl_registry::Event,
        _: &(),
        _: &Connection,
        q: &QueueHandle<Self>,
    ) {
        if let wl_registry::Event::Global {
            name,
            interface,
            version,
        } = e
        {
            s.globals.push(interface.clone());
            match interface.as_str() {
                "wl_compositor" => s.compositor = Some(r.bind(name, version.min(5), q, ())),
                "wl_shm" => s.shm = Some(r.bind(name, 1, q, ())),
                "xdg_wm_base" => s.shell = Some(r.bind(name, version.min(6), q, ())),
                "zxdg_decoration_manager_v1" => s.decoration = Some(r.bind(name, 1, q, ())),
                "wl_output" => {
                    let _: wl_output::WlOutput = r.bind(name, version.min(4), q, ());
                }
                "wl_seat" => s.seat=Some(r.bind(name,version.min(9),q,())),
                _ => {}
            }
        }
    }
}
impl Dispatch<xdg_wm_base::XdgWmBase, ()> for Client {
    fn event(
        _: &mut Self,
        p: &xdg_wm_base::XdgWmBase,
        e: xdg_wm_base::Event,
        _: &(),
        _: &Connection,
        _: &QueueHandle<Self>,
    ) {
        if let xdg_wm_base::Event::Ping { serial } = e {
            p.pong(serial);
        }
    }
}
impl Dispatch<xdg_surface::XdgSurface, ()> for Client {
    fn event(
        s: &mut Self,
        p: &xdg_surface::XdgSurface,
        e: xdg_surface::Event,
        _: &(),
        _: &Connection,
        _: &QueueHandle<Self>,
    ) {
        if let xdg_surface::Event::Configure { serial } = e {
            s.configures += 1;
            p.ack_configure(serial);
        }
    }
}
impl Dispatch<wl_buffer::WlBuffer, ()> for Client {
    fn event(
        s: &mut Self,
        _: &wl_buffer::WlBuffer,
        e: wl_buffer::Event,
        _: &(),
        _: &Connection,
        _: &QueueHandle<Self>,
    ) {
        if let wl_buffer::Event::Release = e {
            s.releases += 1;
        }
    }
}
impl Dispatch<wl_callback::WlCallback, ()> for Client {
    fn event(
        s: &mut Self,
        _: &wl_callback::WlCallback,
        e: wl_callback::Event,
        _: &(),
        _: &Connection,
        _: &QueueHandle<Self>,
    ) {
        if let wl_callback::Event::Done { .. } = e {
            s.frames += 1;
        }
    }
}
impl Dispatch<zxdg_toplevel_decoration_v1::ZxdgToplevelDecorationV1, ()> for Client {
    fn event(
        s: &mut Self,
        _: &zxdg_toplevel_decoration_v1::ZxdgToplevelDecorationV1,
        e: zxdg_toplevel_decoration_v1::Event,
        _: &(),
        _: &Connection,
        _: &QueueHandle<Self>,
    ) {
        if let zxdg_toplevel_decoration_v1::Event::Configure { mode } = e {
            s.server_decor =
                mode == wayland_client::WEnum::Value(zxdg_toplevel_decoration_v1::Mode::ServerSide);
        }
    }
}
delegate_noop!(Client: ignore wl_compositor::WlCompositor);
delegate_noop!(Client: ignore wl_shm::WlShm);
delegate_noop!(Client: ignore wl_shm_pool::WlShmPool);
delegate_noop!(Client: ignore wl_surface::WlSurface);
delegate_noop!(Client: ignore wl_output::WlOutput);
impl Dispatch<xdg_toplevel::XdgToplevel,()> for Client {
    fn event(s:&mut Self,_:&xdg_toplevel::XdgToplevel,e:xdg_toplevel::Event,_:&(),_:&Connection,_:&QueueHandle<Self>){if let xdg_toplevel::Event::Close=e{s.closes+=1;}}
}
impl Dispatch<wl_seat::WlSeat,()> for Client {
    fn event(s:&mut Self,seat:&wl_seat::WlSeat,e:wl_seat::Event,_:&(),_:&Connection,q:&QueueHandle<Self>){
        if let wl_seat::Event::Capabilities{capabilities:wayland_client::WEnum::Value(caps)}=e{
            if caps.contains(wl_seat::Capability::Keyboard) && s.keyboard.is_none(){s.keyboard=Some(seat.get_keyboard(q,()));}
            if caps.contains(wl_seat::Capability::Pointer) && s.pointer.is_none(){s.pointer=Some(seat.get_pointer(q,()));}
        }
    }
}
impl Dispatch<wl_keyboard::WlKeyboard,()> for Client {
    fn event(s:&mut Self,_:&wl_keyboard::WlKeyboard,e:wl_keyboard::Event,_:&(),_:&Connection,_:&QueueHandle<Self>){
        match e{
            wl_keyboard::Event::Keymap{fd,size,..}=>{let mut bytes=Vec::new();std::fs::File::from(fd).take(size.into()).read_to_end(&mut bytes).unwrap();s.keymap=String::from_utf8(bytes).unwrap();},
            wl_keyboard::Event::Key{key,state,..}=>s.keys.push((key,state)),
            wl_keyboard::Event::Enter{keys,..}=>{s.keyboard_enters+=1;s.held_on_enter.push(keys.chunks_exact(4).map(|b|u32::from_ne_bytes(b.try_into().unwrap())).collect());},
            wl_keyboard::Event::Leave{..}=>s.keyboard_leaves+=1,
            wl_keyboard::Event::Modifiers{mods_depressed,..}=>s.modifier_masks.push(mods_depressed),_=>{}
        }
    }
}
impl Dispatch<wl_pointer::WlPointer,()> for Client {
    fn event(s:&mut Self,_:&wl_pointer::WlPointer,e:wl_pointer::Event,_:&(),_:&Connection,_:&QueueHandle<Self>){
        match e{wl_pointer::Event::Enter{serial,..}=>s.pointer_serial=Some(serial),wl_pointer::Event::Button{button,state,serial,..}=>{s.button_serial=Some(serial);s.buttons.push((button,state));},_=>{}}
    }
}
delegate_noop!(Client: ignore zxdg_decoration_manager_v1::ZxdgDecorationManagerV1);

struct MemoryOutput {
    frame: Option<tiny_skia::Pixmap>,
    reject: bool,
}
impl Presenter for MemoryOutput {
    fn present(&mut self, f: &tiny_skia::Pixmap) -> Result<(), String> {
        if self.reject {
            return Err("host test rejection".into());
        }
        self.frame = Some(f.clone());
        Ok(())
    }
}
#[derive(Debug)]
struct Snapshot {
    managed: usize,
    visible: usize,
    content: Option<wm_core::Rect>,
    pixels: Vec<u8>,
    theme_pixels: usize,
    titlebar:Option<wm_core::Point>,close_button:Option<wm_core::Point>,
}
enum Request {
    #[cfg(feature = "desktop-shell")]
    Desktop(mpsc::Sender<Result<(),String>>),
    #[cfg(feature = "desktop-shell")]
    AddClient(UnixStream),
    Snapshot(mpsc::Sender<Snapshot>),
    Reject(bool),
    Workspace(usize),
    Keyboard([u8;8]),Mouse([u8;12]),
    Stop,
}
struct Harness {
    tx: mpsc::Sender<Request>,
    thread: Option<thread::JoinHandle<()>>,
    stopped: Arc<AtomicBool>,
}
impl Harness {
    fn new() -> (Self, Connection) {
        Self::with_input(false)
    }
    fn with_input(input:bool)->(Self,Connection){
        let (server, client) = UnixStream::pair().unwrap();
        let (tx, rx) = mpsc::channel();
        let stopped = Arc::new(AtomicBool::new(false));
        let done = stopped.clone();
        let handle = thread::spawn(move || {
            let mut display = Display::<Server>::new().unwrap();
            let mut state = Server::new(&display.handle(), Size::new(320, 240)).unwrap();
            if input{state.enable_pdhid_seat(&display.handle()).unwrap();}
            let mut decoder=wm_darwin::hid::Decoder::default();
            Server::add_client(&mut display, server).unwrap();
            let mut out = MemoryOutput {
                frame: None,
                reject: false,
            };
            let started = Instant::now();
            while started.elapsed() < Duration::from_secs(15) {
                display.dispatch_clients(&mut state).unwrap();
                let _ = state.repaint(&mut out, started.elapsed().as_millis() as u32);
                display.flush_clients().unwrap();
                match rx.try_recv() {
                    #[cfg(feature = "desktop-shell")]
                    Ok(Request::Desktop(reply))=>{reply.send(state.enable_desktop_shell(wm_theme::FontState::new())).unwrap();},
                    #[cfg(feature = "desktop-shell")]
                    Ok(Request::AddClient(stream))=>{Server::add_client(&mut display,stream).unwrap();},
                    Ok(Request::Stop) => break,
                    Ok(Request::Reject(reject)) => out.reject = reject,
                    Ok(Request::Workspace(workspace)) => state.wm.switch_workspace(workspace),
                    Ok(Request::Keyboard(bytes))=>{if let Some(event)=decoder.keyboard(&bytes).unwrap(){state.deliver_hid(event,started.elapsed().as_millis() as u32).unwrap();}},
                    Ok(Request::Mouse(bytes))=>{state.deliver_hid(decoder.mouse(&bytes).unwrap(),started.elapsed().as_millis() as u32).unwrap();},
                    Ok(Request::Snapshot(reply)) => {
                        let content = state
                            .wm
                            .backend()
                            .windows
                            .values()
                            .find(|w| w.mapped)
                            .map(|w| w.content);
                        let pixels = out.frame.as_ref().unwrap().data().to_vec();
                        let client=state.wm.focused_client().and_then(|id|state.wm.client(id));
                        let (titlebar,close_button)=client.map(|c|{
                            let content=state.wm.backend().windows[&c.window].content.pos;
                            let origin=wm_core::Point::new(content.x-c.layout.client_offset.x,content.y-c.layout.client_offset.y);
                            let y=c.layout.titlebar_height as i32/2;
                            let x=(c.layout.frame_size.w as i32/2..c.layout.frame_size.w as i32-1).chain(1..c.layout.frame_size.w as i32/2).find(|x|!c.layout.button_hitboxes.iter().map(|(_,r)|r).chain(c.layout.resize_hitboxes.iter().map(|(_,r)|r)).any(|r|*x>=r.pos.x && *x<r.pos.x+r.size.w as i32 && y>=r.pos.y && y<r.pos.y+r.size.h as i32));
                            let close=c.layout.button_hitboxes.iter().find(|(k,_)|*k==wm_theme_api::ButtonKind::Close).map(|(_,r)|wm_core::Point::new(origin.x+r.pos.x+r.size.w as i32/2,origin.y+r.pos.y+r.size.h as i32/2));
                            (x.map(|x|wm_core::Point::new(origin.x+x,origin.y+y)),close)
                        }).unwrap_or((None,None));
                        let theme_pixels = pixels
                            .chunks_exact(4)
                            .filter(|p| p[..3] != [56, 72, 80] && p[..3] != [240, 20, 10])
                            .count();
                        reply
                            .send(Snapshot {
                                managed: state.wm.client_count(),
                                visible: state
                                    .wm
                                    .backend()
                                    .windows
                                    .keys()
                                    .filter(|id| state.wm.backend().visible(**id))
                                    .count(),
                                content,
                                pixels,
                                theme_pixels,
                                titlebar,close_button,
                            })
                            .unwrap();
                    }
                    Err(_) => {}
                }
                thread::sleep(Duration::from_millis(1));
            }
            done.store(true, Ordering::Release);
        });
        (
            Self {
                tx,
                thread: Some(handle),
                stopped,
            },
            Connection::from_socket(client).unwrap(),
        )
    }
    fn snapshot(&self) -> Snapshot {
        let (tx, rx) = mpsc::channel();
        self.tx.send(Request::Snapshot(tx)).unwrap();
        rx.recv_timeout(Duration::from_secs(3)).unwrap()
    }
}
impl Drop for Harness {
    fn drop(&mut self) {
        let _ = self.tx.send(Request::Stop);
        self.thread.take().unwrap().join().unwrap();
        assert!(self.stopped.load(Ordering::Acquire));
    }
}

#[test]
fn actual_shm_client_uses_chonkstep_policy_decorations_and_owned_lifetime() {
    let (h, connection) = Harness::new();
    let mut queue = connection.new_event_queue();
    let qh = queue.handle();
    let mut client = Client::default();
    connection.display().get_registry(&qh, ());
    queue.roundtrip(&mut client).unwrap();
    assert!(!client.globals.iter().any(|g| matches!(
        g.as_str(),
        "wl_seat" | "wl_subcompositor" | "zwp_linux_dmabuf_v1" | "zwlr_layer_shell_v1"
    )));
    let surface = client.compositor.as_ref().unwrap().create_surface(&qh, ());
    let xdg = client
        .shell
        .as_ref()
        .unwrap()
        .get_xdg_surface(&surface, &qh, ());
    let top = xdg.get_toplevel(&qh, ());
    top.set_title("Real Chonkstep SHM client".into());
    top.set_app_id("host.shm.test".into());
    let decoration = client
        .decoration
        .as_ref()
        .unwrap()
        .get_toplevel_decoration(&top, &qh, ());
    decoration.set_mode(zxdg_toplevel_decoration_v1::Mode::ServerSide);
    surface.commit();
    queue.roundtrip(&mut client).unwrap();
    assert!(client.configures > 0);
    assert!(client.server_decor);
    let mut file = tempfile::tempfile().unwrap();
    let bytes = [10, 20, 240, 255].repeat(80 * 48);
    file.write_all(&bytes).unwrap();
    let pool = client
        .shm
        .as_ref()
        .unwrap()
        .create_pool(file.as_fd(), bytes.len() as i32, &qh, ());
    let buffer = pool.create_buffer(0, 80, 48, 80 * 4, wl_shm::Format::Argb8888, &qh, ());
    h.tx.send(Request::Reject(true)).unwrap();
    h.snapshot(); // ordered barrier before commit
    surface.attach(Some(&buffer), 0, 0);
    surface.damage_buffer(0, 0, 80, 48);
    surface.frame(&qh, ());
    surface.commit();
    queue.roundtrip(&mut client).unwrap();
    queue.roundtrip(&mut client).unwrap();
    assert_eq!(client.releases, 1);
    assert_eq!(
        client.frames, 0,
        "failed presentation must not complete a frame"
    );
    h.tx.send(Request::Reject(false)).unwrap();
    h.snapshot();
    queue.roundtrip(&mut client).unwrap();
    queue.roundtrip(&mut client).unwrap();
    assert_eq!(client.frames, 1);
    let first = h.snapshot();
    assert_eq!(first.managed, 1);
    assert_eq!(first.visible, 1);
    let content = first.content.unwrap();
    assert_eq!(content.size, Size::new(80, 48));
    let pixel = ((content.pos.y as usize + 4) * 320 + content.pos.x as usize + 4) * 4;
    assert_eq!(&first.pixels[pixel..pixel + 4], &[240, 20, 10, 255]);
    assert!(
        first.theme_pixels > 500,
        "actual RasterThemeEngine must paint visible frame chrome"
    );
    h.tx.send(Request::Workspace(1)).unwrap();
    assert_eq!(
        h.snapshot().visible,
        0,
        "wm-core workspace policy hides the actual client"
    );
    h.tx.send(Request::Workspace(0)).unwrap();
    assert_eq!(h.snapshot().visible, 1);
    // Buffer released: overwrite and destroy client storage. Retained rendering stays owned.
    file.seek(SeekFrom::Start(0)).unwrap();
    file.write_all(&vec![0; bytes.len()]).unwrap();
    buffer.destroy();
    pool.destroy();
    queue.roundtrip(&mut client).unwrap();
    top.set_title("Changed title after release".into());
    queue.roundtrip(&mut client).unwrap();
    let later = h.snapshot();
    assert_eq!(&later.pixels[pixel..pixel + 4], &[240, 20, 10, 255]);
    surface.attach(None, 0, 0);
    surface.commit();
    queue.roundtrip(&mut client).unwrap();
    assert_eq!(h.snapshot().visible, 0);
    let before = client.configures;
    surface.commit();
    queue.roundtrip(&mut client).unwrap();
    assert!(
        client.configures > before,
        "unmap must require fresh configure/ack"
    );
    file.seek(SeekFrom::Start(0)).unwrap();
    file.write_all(&bytes).unwrap();
    let pool = client
        .shm
        .as_ref()
        .unwrap()
        .create_pool(file.as_fd(), bytes.len() as i32, &qh, ());
    let buffer = pool.create_buffer(0, 80, 48, 320, wl_shm::Format::Xrgb8888, &qh, ());
    surface.attach(Some(&buffer), 0, 0);
    surface.commit();
    queue.roundtrip(&mut client).unwrap();
    assert_eq!(h.snapshot().visible, 1);
    decoration.destroy();
    top.destroy();
    xdg.destroy();
    surface.destroy();
    queue.roundtrip(&mut client).unwrap();
    assert_eq!(h.snapshot().managed, 0);
}

#[test]
fn unqualified_scale_is_a_protocol_failure_not_a_fake_frame() {
    let (h, connection) = Harness::new();
    let mut queue = connection.new_event_queue();
    let qh = queue.handle();
    let mut client = Client::default();
    connection.display().get_registry(&qh, ());
    queue.roundtrip(&mut client).unwrap();
    let surface = client.compositor.as_ref().unwrap().create_surface(&qh, ());
    let xdg = client
        .shell
        .as_ref()
        .unwrap()
        .get_xdg_surface(&surface, &qh, ());
    let _top = xdg.get_toplevel(&qh, ());
    surface.commit();
    queue.roundtrip(&mut client).unwrap();
    let mut file = tempfile::tempfile().unwrap();
    file.write_all(&[0; 64]).unwrap();
    let pool = client
        .shm
        .as_ref()
        .unwrap()
        .create_pool(file.as_fd(), 64, &qh, ());
    let buffer = pool.create_buffer(0, 4, 4, 16, wl_shm::Format::Argb8888, &qh, ());
    surface.set_buffer_scale(2);
    surface.attach(Some(&buffer), 0, 0);
    surface.frame(&qh, ());
    surface.commit();
    assert!(queue.roundtrip(&mut client).is_err());
    assert_eq!(client.frames, 0);
    assert_eq!(h.snapshot().visible, 0);
}

#[test]
fn pdhid_records_deliver_real_wayland_keys_pointer_cursor_and_wm_drag(){
    let(h,connection)=Harness::with_input(true);let mut queue=connection.new_event_queue();let qh=queue.handle();let mut c=Client::default();
    connection.display().get_registry(&qh,());queue.roundtrip(&mut c).unwrap();queue.roundtrip(&mut c).unwrap();queue.roundtrip(&mut c).unwrap();
    assert!(c.seat.is_some() && c.pointer.is_some() && c.keyboard.is_some());assert!(c.keymap.contains("xkb_keymap"));
    let surface=c.compositor.as_ref().unwrap().create_surface(&qh,());let xdg=c.shell.as_ref().unwrap().get_xdg_surface(&surface,&qh,());let top=xdg.get_toplevel(&qh,());
    let _decor=c.decoration.as_ref().unwrap().get_toplevel_decoration(&top,&qh,());top.set_title("Chonkstep input".into());surface.commit();queue.roundtrip(&mut c).unwrap();
    let mut file=tempfile::tempfile().unwrap();file.write_all(&[10,20,240,255].repeat(160*80)).unwrap();
    let pool=c.shm.as_ref().unwrap().create_pool(file.as_fd(),160*80*4,&qh,());let buffer=pool.create_buffer(0,160,80,640,wl_shm::Format::Xrgb8888,&qh,());
    surface.attach(Some(&buffer),0,0);surface.commit();queue.roundtrip(&mut c).unwrap();queue.roundtrip(&mut c).unwrap();assert!(c.keyboard_enters>0);
    let key=|seq:u32,usage:u8,down:u8|{let mut b=[0;8];b[..4].copy_from_slice(&seq.to_le_bytes());b[4]=usage;b[5]=down;Request::Keyboard(b)};
    for request in [key(1,0xe1,1),key(2,4,1),key(3,4,0),key(4,0xe1,0)]{h.tx.send(request).unwrap();}
    h.snapshot();queue.roundtrip(&mut c).unwrap();
    use wayland_client::WEnum::Value;
    assert_eq!(c.keys,vec![(42,Value(wl_keyboard::KeyState::Pressed)),(30,Value(wl_keyboard::KeyState::Pressed)),(30,Value(wl_keyboard::KeyState::Released)),(42,Value(wl_keyboard::KeyState::Released))]);
    assert!(c.modifier_masks.iter().any(|m|*m!=0));assert_eq!(c.modifier_masks.last(),Some(&0));
    let mouse=|seq:u32,p:wm_core::Point,buttons:u8|{let mut b=[0;12];b[..4].copy_from_slice(&seq.to_le_bytes());b[5]=buttons;b[9]=1;
        let x=((f64::from(p.x)+0.25)/319.*65535.).round().clamp(0.,65535.) as u16;
        let y=((f64::from(p.y)+0.25)/239.*65535.).round().clamp(0.,65535.) as u16;
        b[6..8].copy_from_slice(&x.to_le_bytes());b[10..12].copy_from_slice(&y.to_le_bytes());Request::Mouse(b)};
    let first=h.snapshot();let content=first.content.unwrap();let inside=wm_core::Point::new(content.pos.x+20,content.pos.y+20);
    h.tx.send(mouse(1,inside,0)).unwrap();h.tx.send(mouse(2,inside,1)).unwrap();h.tx.send(mouse(3,inside,0)).unwrap();h.snapshot();queue.roundtrip(&mut c).unwrap();
    assert_eq!(c.buttons,vec![(0x110,Value(wl_pointer::ButtonState::Pressed)),(0x110,Value(wl_pointer::ButtonState::Released))]);
    // Client cursor SHM goes through the same guarded import/release and CPU output.
    let cursor=c.compositor.as_ref().unwrap().create_surface(&qh,());
    let mut cursor_file=tempfile::tempfile().unwrap();cursor_file.write_all(&[0,255,0,255].repeat(4*4)).unwrap();
    let cursor_pool=c.shm.as_ref().unwrap().create_pool(cursor_file.as_fd(),64,&qh,());let cursor_buffer=cursor_pool.create_buffer(0,4,4,16,wl_shm::Format::Argb8888,&qh,());
    c.pointer.as_ref().unwrap().set_cursor(c.pointer_serial.unwrap(),Some(&cursor),0,0);cursor.attach(Some(&cursor_buffer),0,0);cursor.frame(&qh,());cursor.commit();
    queue.roundtrip(&mut c).unwrap();queue.roundtrip(&mut c).unwrap();let shot=h.snapshot();let at=(inside.y as usize*320+inside.x as usize)*4;assert_eq!(&shot.pixels[at..at+4],&[0,255,0,255]);
    assert!(c.releases>=2 && c.frames>=1);
    // Drag on the real RasterThemeEngine titlebar. Client receives no fake click.
    let title=first.titlebar.unwrap();let destination=wm_core::Point::new(title.x+31,title.y+17);
    h.tx.send(mouse(4,title,0)).unwrap();h.tx.send(mouse(5,title,1)).unwrap();h.tx.send(mouse(6,destination,1)).unwrap();h.tx.send(mouse(7,destination,0)).unwrap();
    let moved=h.snapshot().content.unwrap();assert_eq!(moved.pos,wm_core::Point::new(content.pos.x+31,content.pos.y+17));
    queue.roundtrip(&mut c).unwrap();assert_eq!(c.buttons.len(),2,"decoration drag must not leak button events to the client");
    // A forged/old serial cannot start client-initiated movement.
    top._move(c.seat.as_ref().unwrap(),0);queue.roundtrip(&mut c).unwrap();
    let inside=wm_core::Point::new(moved.pos.x+20,moved.pos.y+20);
    h.tx.send(mouse(8,inside,0)).unwrap();h.snapshot();assert_eq!(h.snapshot().content.unwrap().pos,moved.pos);
    h.tx.send(mouse(9,inside,1)).unwrap();h.snapshot();queue.roundtrip(&mut c).unwrap();
    top._move(c.seat.as_ref().unwrap(),c.button_serial.unwrap());queue.roundtrip(&mut c).unwrap();
    let destination=wm_core::Point::new(inside.x+12,inside.y+9);
    h.tx.send(mouse(10,destination,1)).unwrap();h.tx.send(mouse(11,destination,0)).unwrap();
    assert_eq!(h.snapshot().content.unwrap().pos,wm_core::Point::new(moved.pos.x+12,moved.pos.y+9));
    let close=h.snapshot().close_button.unwrap();h.tx.send(mouse(12,close,1)).unwrap();h.tx.send(mouse(13,close,0)).unwrap();h.snapshot();queue.roundtrip(&mut c).unwrap();assert_eq!(c.closes,1);
}

#[cfg(feature="desktop-shell")]
fn key_record(seq:u32,usage:u8,down:bool)->Request {
    let mut bytes=[0;8];bytes[..4].copy_from_slice(&seq.to_le_bytes());bytes[4]=usage;bytes[5]=u8::from(down);Request::Keyboard(bytes)
}
#[cfg(feature="desktop-shell")]
fn map_client(connection:&Connection,title:&str)->(wayland_client::EventQueue<Client>,Client,wl_surface::WlSurface,xdg_surface::XdgSurface,xdg_toplevel::XdgToplevel) {
    let mut queue=connection.new_event_queue();let q=queue.handle();let mut client=Client::default();
    connection.display().get_registry(&q,());for _ in 0..3{queue.roundtrip(&mut client).unwrap();}
    let surface=client.compositor.as_ref().unwrap().create_surface(&q,());
    let xdg=client.shell.as_ref().unwrap().get_xdg_surface(&surface,&q,());let top=xdg.get_toplevel(&q,());
    top.set_title(title.into());let decor=client.decoration.as_ref().unwrap().get_toplevel_decoration(&top,&q,());
    decor.set_mode(zxdg_toplevel_decoration_v1::Mode::ServerSide);surface.commit();queue.roundtrip(&mut client).unwrap();
    let mut file=tempfile::tempfile().unwrap();file.write_all(&[10,20,240,255].repeat(100*60)).unwrap();
    let pool=client.shm.as_ref().unwrap().create_pool(file.as_fd(),100*60*4,&q,());let buffer=pool.create_buffer(0,100,60,400,wl_shm::Format::Argb8888,&q,());
    surface.attach(Some(&buffer),0,0);surface.frame(&q,());surface.commit();
    for _ in 0..3{queue.roundtrip(&mut client).unwrap();}
    assert!(client.server_decor);assert_eq!(client.releases,1);assert_eq!(client.frames,1);
    // Release confirms that compositor pixels have independent ownership.
    buffer.destroy();pool.destroy();
    (queue,client,surface,xdg,top)
}
#[test]
#[cfg(feature="desktop-shell")]
fn shared_shell_modal_launch_chord_does_not_enter_new_client_as_text_or_release(){
    let(h,a)=Harness::with_input(true);let(tx,rx)=mpsc::channel();h.tx.send(Request::Desktop(tx)).unwrap();rx.recv_timeout(Duration::from_secs(3)).unwrap().unwrap();
    let(mut qa,mut ca,_sa,_xa,_ta)=map_client(&a,"terminal-A");
    assert!(ca.keyboard_enters>0);
    h.tx.send(key_record(1,0xe3,true)).unwrap();h.tx.send(key_record(2,0x28,true)).unwrap();h.snapshot();qa.roundtrip(&mut ca).unwrap();
    assert!(ca.keyboard_leaves>0,"real shell capture must leave client focus");
    assert!(!ca.keys.iter().any(|(key,_)|*key==28),"launch Return must not reach A");
    // Native spawner is Unsupported: the real Shell shows a refusal popup.
    // A separate real client maps while that popup owns focus; no fake spawn.
    let(server,b)=UnixStream::pair().unwrap();h.tx.send(Request::AddClient(server)).unwrap();h.snapshot();let b=Connection::from_socket(b).unwrap();
    let(mut qb,mut cb,_sb,_xb,_tb)=map_client(&b,"terminal-B");
    assert_eq!(h.snapshot().managed,2);assert_eq!(cb.keyboard_enters,0,"B maps without focus while modal");
    h.tx.send(key_record(3,0x28,false)).unwrap();h.tx.send(key_record(4,0xe3,false)).unwrap();
    // A real Escape press closes the actual refusal menu, then its release
    // retires capture before B gains focus. The app receives no synthetic keys.
    h.tx.send(key_record(5,0x29,true)).unwrap();h.tx.send(key_record(6,0x29,false)).unwrap();h.snapshot();qb.roundtrip(&mut cb).unwrap();
    assert!(cb.keyboard_enters>0);assert!(cb.keys.is_empty());assert!(cb.held_on_enter.iter().all(Vec::is_empty));
    h.tx.send(key_record(7,4,true)).unwrap();h.tx.send(key_record(8,4,false)).unwrap();h.snapshot();qb.roundtrip(&mut cb).unwrap();
    use wayland_client::WEnum::Value;
    assert_eq!(cb.keys,vec![(30,Value(wl_keyboard::KeyState::Pressed)),(30,Value(wl_keyboard::KeyState::Released))]);
    assert_eq!(cb.modifier_masks.last(),Some(&0));
}
