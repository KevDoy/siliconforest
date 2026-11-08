//! Pure Wayland SHM frontend. Protocol success corresponds to real owned pixels.
use std::{collections::HashMap, sync::Arc};
use smithay::{delegate_compositor,delegate_shm,delegate_xdg_shell,delegate_xdg_decoration,delegate_output,delegate_seat};
use smithay::reexports::wayland_server::{Client,Display,DisplayHandle,Resource,
    backend::{ClientData,ClientId,DisconnectReason},
    protocol::{wl_surface::{self,WlSurface},wl_buffer,wl_callback,wl_seat,wl_output}};
use smithay::reexports::wayland_protocols::xdg::decoration::zv1::server::zxdg_toplevel_decoration_v1::Mode as DecorationMode;
use smithay::input::{Seat,SeatState,SeatHandler,pointer::CursorImageStatus};
use smithay::output::{Output,PhysicalProperties,Subpixel,Mode,Scale};
use smithay::utils::{Serial,Transform};
use smithay::wayland::{
    buffer::BufferHandler,
    compositor::{CompositorClientState,CompositorHandler,CompositorState,SurfaceAttributes,BufferAssignment,with_states},
    shell::xdg::{XdgShellHandler,XdgShellState,ToplevelSurface,PopupSurface,PositionerState,XdgToplevelSurfaceData,
        SurfaceCachedState,decoration::{XdgDecorationState,XdgDecorationHandler}},
    shm::{ShmHandler,ShmState}, output::OutputHandler,
};
use wm_core::{BackendEvent,ClientChrome,WindowManager,Size};
use crate::{scene::CpuBackend,presenter::Presenter};

#[derive(Default)]
pub struct Peer {
    compositor: CompositorClientState,
}
impl ClientData for Peer {
    fn initialized(&self, _: ClientId) {}
    fn disconnected(&self, _: ClientId, _: DisconnectReason) {}
}

pub struct Server {
    #[cfg(feature = "desktop-shell")]
    pub(crate) desktop: Option<crate::desktop::DesktopSession>,
    compositor: CompositorState,
    shell: XdgShellState,
    shm: ShmState,
    _decoration: XdgDecorationState,
    output: Output,
    pub(crate) seats: SeatState<Self>,
    pub(crate) input: Option<crate::input::InputState>,
    pub(crate) cursor_callbacks: Vec<wl_callback::WlCallback>,
    pub wm: WindowManager<CpuBackend>,
    callbacks: HashMap<u64, Vec<wl_callback::WlCallback>>,
}
impl Server {
    pub fn new(dh: &DisplayHandle, size: Size) -> Result<Self, String> {
        let backend = CpuBackend::new(size)?;
        let compositor = CompositorState::new::<Self>(dh);
        // Subsurface composition is not in this first increment. Do not advertise it.
        dh.remove_global::<Self>(compositor.subcompositor_global());
        let output = Output::new(
            "PDGOP-0".into(),
            PhysicalProperties {
                size: (0, 0).into(),
                subpixel: Subpixel::Unknown,
                make: "PureDarwin".into(),
                model: "PDGOP mapped framebuffer".into(),
            },
        );
        output.create_global::<Self>(dh);
        // No measured vblank period; refresh=0 is unknown, not a promised 60Hz mode.
        let mode = Mode {
            size: (size.w as i32, size.h as i32).into(),
            refresh: 0,
        };
        output.change_current_state(
            Some(mode),
            Some(Transform::Normal),
            Some(Scale::Integer(1)),
            Some((0, 0).into()),
        );
        output.set_preferred(mode);
        Ok(Self {
            #[cfg(feature = "desktop-shell")]
            desktop: None,
            compositor,
            shell: XdgShellState::new_with_capabilities::<Self>(dh, []),
            shm: ShmState::new::<Self>(dh, vec![]),
            _decoration: XdgDecorationState::new::<Self>(dh),
            output,
            wm: WindowManager::new(
                backend,
                Box::new(wm_theme::RasterThemeEngine::new(
                    wm_theme::default_theme::nextstep_classic(),
                )),
            ),
            seats: SeatState::new(),
            input: None, cursor_callbacks: Vec::new(),
            callbacks: HashMap::new(),
        })
    }
    pub fn add_client(
        display: &mut Display<Self>,
        stream: std::os::unix::net::UnixStream,
    ) -> std::io::Result<Client> {
        display
            .handle()
            .insert_client(stream, Arc::new(Peer::default()))
    }
    /// Frame callbacks are sent only after the presenter accepts the complete CPU frame.
    /// Native PDGOP acceptance is a memory-store fence, not vblank or panel proof.
    pub fn repaint(&mut self, presenter: &mut impl Presenter, time_ms: u32) -> Result<(), String> {
        #[cfg(feature = "desktop-shell")]
        self.service_desktop()?;
        self.sync_input_focus();
        if !self.wm.backend().dirty {
            return Ok(());
        }
        let frame = self.wm.backend().render()?;
        presenter.present(&frame)?;
        self.wm.backend_mut().dirty = false;
        if self.wm.backend().cursor.is_some(){for callback in self.cursor_callbacks.drain(..){callback.done(time_ms);}}
        let ready: Vec<u64> = self
            .callbacks
            .keys()
            .copied()
            .filter(|id| self.wm.backend().visible(*id))
            .collect();
        for id in ready {
            if let Some(callbacks) = self.callbacks.remove(&id) {
                for callback in callbacks {
                    callback.done(time_ms);
                }
            }
        }
        Ok(())
    }
    fn remove(&mut self, surface: &WlSurface) {
        if let Some(id) = self.wm.backend().window_for_surface(surface) {
            self.wm.dispatch(BackendEvent::Destroyed(id));
            self.wm.backend_mut().remove(id);
            self.callbacks.remove(&id);
            self.output.leave(surface);
        }
    }
    pub(crate) fn refuse(surface: &WlSurface, why: &str) {
        surface.post_error(
            wl_surface::Error::InvalidSize,
            format!("Chonkstep Darwin: {why}"),
        );
    }
    fn metadata(&mut self, surface: &ToplevelSurface) {
        if let Some(id) = self.wm.backend().window_for_surface(surface.wl_surface()) {
            let (title, app_id) = with_states(surface.wl_surface(), |s| {
                let d = s
                    .data_map
                    .get::<XdgToplevelSurfaceData>()
                    .unwrap()
                    .lock()
                    .unwrap();
                (d.title.clone(), d.app_id.clone())
            });
            let w = self.wm.backend_mut().windows.get_mut(&id).unwrap();
            w.title = title;
            w.app_id = app_id;
            self.wm.dispatch(BackendEvent::TitleChanged(id));
        }
    }
    fn decoration(&mut self, surface: ToplevelSurface, mode: DecorationMode) {
        if let Some(id) = self.wm.backend().window_for_surface(surface.wl_surface()) {
            self.wm.backend_mut().windows.get_mut(&id).unwrap().chrome =
                if mode == DecorationMode::ServerSide {
                    ClientChrome::Full
                } else {
                    ClientChrome::Edges
                };
            surface.with_pending_state(|s| s.decoration_mode = Some(mode));
            if surface.is_initial_configure_sent() {
                surface.send_pending_configure();
            }
            self.wm.dispatch(BackendEvent::ChromeChanged(id));
        }
    }
}
impl BufferHandler for Server {
    fn buffer_destroyed(&mut self, _: &wl_buffer::WlBuffer) {}
} // no retained client mapping
impl ShmHandler for Server {
    fn shm_state(&self) -> &ShmState {
        &self.shm
    }
}
impl OutputHandler for Server {}
// Seat capabilities are created only by the explicit PDHID acquisition path.
impl SeatHandler for Server {
    type KeyboardFocus = WlSurface;
    type PointerFocus = WlSurface;
    type TouchFocus = WlSurface;
    fn seat_state(&mut self) -> &mut SeatState<Self> {
        &mut self.seats
    }
    fn focus_changed(&mut self, _: &Seat<Self>, _: Option<&WlSurface>) {}
    fn cursor_image(&mut self, _: &Seat<Self>, image: CursorImageStatus) {self.cursor_image_changed(image);}
}
impl CompositorHandler for Server {
    fn compositor_state(&mut self) -> &mut CompositorState {
        &mut self.compositor
    }
    fn client_compositor_state<'a>(&self, client: &'a Client) -> &'a CompositorClientState {
        &client.get_data::<Peer>().unwrap().compositor
    }
    fn destroyed(&mut self, surface: &WlSurface) {
        if self.input.as_ref().and_then(|i|i.cursor_surface.as_ref())==Some(surface){
            self.input.as_mut().unwrap().cursor_surface=None;self.wm.backend_mut().cursor=None;
            self.cursor_callbacks.clear();self.wm.backend_mut().dirty=true;
        }
        self.remove(surface);
    }
    fn commit(&mut self, surface: &WlSurface) {
        if self.input.as_ref().and_then(|i|i.cursor_surface.as_ref())==Some(surface){self.import_cursor(surface);return;}
        let Some(id) = self.wm.backend().window_for_surface(surface) else {
            return;
        };
        let (buffer, scale, transform, delta, callbacks, xdg) = with_states(surface, |s| {
            let xdg = *s.cached_state.get::<SurfaceCachedState>().current();
            let mut attributes = s.cached_state.get::<SurfaceAttributes>();
            let a = attributes.current();
            a.damage.clear();
            (
                a.buffer.take(),
                a.buffer_scale,
                a.buffer_transform,
                a.buffer_delta.take(),
                std::mem::take(&mut a.frame_callbacks),
                xdg,
            )
        });
        let hints = wm_core::SizeHints {
            min_size: ((xdg.min_size.w, xdg.min_size.h) != (0, 0))
                .then(|| Size::new(xdg.min_size.w.max(0) as u32, xdg.min_size.h.max(0) as u32)),
            max_size: ((xdg.max_size.w, xdg.max_size.h) != (0, 0)).then(|| {
                Size::new(
                    if xdg.max_size.w > 0 {
                        xdg.max_size.w as u32
                    } else {
                        u32::MAX
                    },
                    if xdg.max_size.h > 0 {
                        xdg.max_size.h as u32
                    } else {
                        u32::MAX
                    },
                )
            }),
            resize_increment: None,
        };
        if self.wm.backend().windows[&id].hints != hints {
            self.wm.backend_mut().windows.get_mut(&id).unwrap().hints = hints;
            self.wm.dispatch(BackendEvent::SizeHintsChanged(id));
        }
        if scale != 1
            || transform != wl_output::Transform::Normal
            || delta.is_some_and(|d| d.x != 0 || d.y != 0)
        {
            Self::refuse(
                surface,
                "buffer scale/transform/offset not implemented in this increment",
            );
            return;
        }
        match buffer {
            Some(BufferAssignment::NewBuffer(buffer)) => {
                if !self.wm.backend().windows[&id].surface.ensure_configured() {
                    return;
                }
                let image = crate::pixels::copy_shm(&buffer);
                buffer.release(); // all CPU reads finished; retain only the copy
                let image = match image {
                    Ok(image) => image,
                    Err(error) => {
                        Self::refuse(surface, &error);
                        return;
                    }
                };
                if xdg.geometry.is_some_and(|g| {
                    g.loc.x != 0
                        || g.loc.y != 0
                        || g.size.w != image.width as i32
                        || g.size.h != image.height as i32
                }) {
                    Self::refuse(surface, "cropped xdg window geometry not implemented");
                    return;
                }
                let size = Size::new(image.width, image.height);
                let w = self.wm.backend_mut().windows.get_mut(&id).unwrap();
                let first = !w.committed;
                w.image = Some(image);
                w.committed = true;
                if first {
                    w.content.size = size;
                    self.wm.dispatch(BackendEvent::MapRequest(id));
                    self.output.enter(surface);
                } else {
                    self.wm
                        .dispatch(BackendEvent::ClientSizeCommitted { window: id, size });
                }
            }
            Some(BufferAssignment::Removed) => {
                let was_mapped = self.wm.backend().windows[&id].committed;
                self.wm.dispatch(BackendEvent::Unmapped(id));
                let w = self.wm.backend_mut().windows.get_mut(&id).unwrap();
                w.committed = false;
                w.mapped = false;
                w.image = None;
                self.callbacks.remove(&id);
                self.output.leave(surface);
                if was_mapped {
                    // We do not use Smithay renderer utilities, so reset this role
                    // ourselves. Old acknowledgements cannot authorize a remap.
                    let decoration = w.surface.current_state().decoration_mode;
                    with_states(surface, |s| {
                        let mut role = s
                            .data_map
                            .get::<XdgToplevelSurfaceData>()
                            .unwrap()
                            .lock()
                            .unwrap();
                        *role = Default::default();
                        role.current.decoration_mode = decoration;
                        role.current.capabilities = Vec::new().into();
                        let mut cached = s.cached_state.get::<SurfaceCachedState>();
                        *cached.current() = Default::default();
                        *cached.pending() = Default::default();
                    });
                    w.title = None;
                    w.app_id = None;
                } else {
                    w.surface.send_pending_configure();
                }
            }
            None => {
                if !self.wm.backend().windows[&id].committed {
                    self.wm.backend().windows[&id]
                        .surface
                        .send_pending_configure();
                }
            }
        }
        self.callbacks.entry(id).or_default().extend(callbacks);
        self.wm.backend_mut().dirty = true;
    }
}
impl XdgShellHandler for Server {
    fn xdg_shell_state(&mut self) -> &mut XdgShellState {
        &mut self.shell
    }
    fn new_toplevel(&mut self, surface: ToplevelSurface) {
        self.wm.backend_mut().add(surface);
    }
    fn toplevel_destroyed(&mut self, surface: ToplevelSurface) {
        self.remove(surface.wl_surface());
    }
    fn title_changed(&mut self, surface: ToplevelSurface) {
        self.metadata(&surface);
    }
    fn app_id_changed(&mut self, surface: ToplevelSurface) {
        self.metadata(&surface);
    }
    fn parent_changed(&mut self, surface: ToplevelSurface) {
        if let Some(id) = self.wm.backend().window_for_surface(surface.wl_surface()) {
            self.wm.dispatch(BackendEvent::ParentChanged(id));
        }
    }
    fn move_request(&mut self,surface:ToplevelSurface,seat:wl_seat::WlSeat,serial:Serial){
        if let Some(id)=self.authorize_client_drag(&surface,&seat,serial){self.wm.dispatch(BackendEvent::MoveRequest(id));}
    }
    fn resize_request(&mut self,surface:ToplevelSurface,seat:wl_seat::WlSeat,serial:Serial,edges:smithay::reexports::wayland_protocols::xdg::shell::server::xdg_toplevel::ResizeEdge){
        use smithay::reexports::wayland_protocols::xdg::shell::server::xdg_toplevel::ResizeEdge as E;
        use wm_theme_api::ResizeEdge as R;
        let edge=match edges{E::Top=>R::North,E::Bottom=>R::South,E::Left=>R::West,E::Right=>R::East,
            E::TopLeft=>R::NorthWest,E::TopRight=>R::NorthEast,E::BottomLeft=>R::SouthWest,E::BottomRight=>R::SouthEast,_=>return};
        if let Some(window)=self.authorize_client_drag(&surface,&seat,serial){self.wm.dispatch(BackendEvent::ResizeRequest{window,edge});}
    }
    fn new_popup(&mut self, surface: PopupSurface, _: PositionerState) {
        eprintln!("unsupported: Darwin popup");
        surface.send_popup_done();
    }
    fn grab(&mut self, surface: PopupSurface, _: wl_seat::WlSeat, _: Serial) {
        surface.send_popup_done();
    }
    fn reposition_request(&mut self, surface: PopupSurface, _: PositionerState, _: u32) {
        surface.send_popup_done();
    }
    // No maximize/fullscreen capability is advertised. Refuse requests by responding
    // with the unchanged configure, never claiming that a mode was entered.
    fn maximize_request(&mut self, s: ToplevelSurface) {
        s.send_configure();
    }
    fn unmaximize_request(&mut self, s: ToplevelSurface) {
        s.send_configure();
    }
    fn fullscreen_request(&mut self, s: ToplevelSurface, _: Option<wl_output::WlOutput>) {
        s.send_configure();
    }
    fn unfullscreen_request(&mut self, s: ToplevelSurface) {
        s.send_configure();
    }
    fn minimize_request(&mut self, _: ToplevelSurface) {
        eprintln!("unsupported: Darwin minimize without restore UI");
    }
}
impl XdgDecorationHandler for Server {
    fn new_decoration(&mut self, s: ToplevelSurface) {
        self.decoration(s, DecorationMode::ServerSide);
    }
    fn request_mode(&mut self, s: ToplevelSurface, mode: DecorationMode) {
        self.decoration(s, mode);
    }
    fn unset_mode(&mut self, s: ToplevelSurface) {
        self.decoration(s, DecorationMode::ServerSide);
    }
}
delegate_compositor!(Server);
delegate_shm!(Server);
delegate_xdg_shell!(Server);
delegate_xdg_decoration!(Server);
delegate_output!(Server);
delegate_seat!(Server);
