//! wm-core's real policy adapter. Geometry is global; decoration parts are local.
use smithay::reexports::wayland_protocols::xdg::shell::server::xdg_toplevel;
use smithay::wayland::shell::xdg::ToplevelSurface;
use std::collections::HashMap;
use wm_core::*;
use wm_theme_api::{DecorationBuffer, DecorationLayout, DecorationSurface, ResizeEdge};

pub struct Window {
    pub surface: ToplevelSurface,
    pub title: Option<String>,
    pub app_id: Option<String>,
    pub content: Rect,
    pub image: Option<DecorationBuffer>,
    pub mapped: bool,
    pub committed: bool,
    pub chrome: ClientChrome,
    pub hints: SizeHints,
}
struct Frame {
    window: u64,
    geometry: Rect,
    mapped: bool,
    paint: Option<DecorationSurface>,
    layout: DecorationLayout,
}
struct Shell {
    geometry: Rect,
    mapped: bool,
    above: bool,
    image: DecorationBuffer,
}

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum DesktopHit {
    Root,
    Shell(u64, Point),
    Frame(u64, Point),
    Client(u64, Point),
}

pub struct CpuBackend {
    pub windows: HashMap<u64, Window>,
    frames: HashMap<u64, Frame>,
    shells: HashMap<u64, Shell>,
    stack: Vec<u64>, // window IDs; each is drawn with its owning frame
    shell_stack: Vec<u64>,
    monitors: Vec<MonitorInfo>,
    next_id: u64,
    background: [u8; 3],
    background_image: Option<DecorationBuffer>,
    pub dirty: bool,
    pub(crate) input_enabled: bool,
    pub(crate) pointer: Point,
    pub(crate) cursor: Option<(Point, DecorationBuffer)>,
    pub(crate) replay: bool,
    pub(crate) grabbed_keys: std::collections::HashSet<KeyCombo>,
    pointer_grab: Option<DragHandle>,
    popup_grab: Option<wm_theme_api::PopupGrab>,
    pub(crate) keyboard_grabbed: bool,
    shell_motion: std::collections::VecDeque<(u64, Point)>,
    shell_clicks: std::collections::VecDeque<(u64, Point, MouseButton, bool)>,
    pub(crate) input_fault: Option<String>,
    pub(crate) keyboard_config_verified:bool,
}
impl CpuBackend {
    pub fn new(size: Size) -> Result<Self, String> {
        crate::pixels::image_len(size.w, size.h)?;
        Ok(Self {
            windows: HashMap::new(),
            frames: HashMap::new(),
            shells: HashMap::new(),
            stack: Vec::new(),
            shell_stack: Vec::new(),
            next_id: 1,
            background: [56, 72, 80],
            background_image: None,
            dirty: true,
            input_enabled: false, pointer: Point::new(0,0), cursor: None,
            replay: false, grabbed_keys: Default::default(), pointer_grab: None,
            popup_grab: None, keyboard_grabbed: false,
            shell_motion: Default::default(), shell_clicks: Default::default(), input_fault: None,
            keyboard_config_verified:false,
            monitors: vec![MonitorInfo {
                geometry: Rect::new(Point::new(0, 0), size),
                name: "PDGOP-0".into(),
                identity: None,
                primary: true,
            }],
        })
    }
    fn alloc(&mut self) -> u64 {
        let id = self.next_id;
        self.next_id += 1;
        id
    }
    pub fn popup_active(&self) -> bool { self.popup_grab.is_some() }
    pub fn popup_can_open(&self) -> bool { self.input_enabled && self.pointer_grab.is_none() && self.popup_grab.is_none() && !self.keyboard_grabbed }
    pub fn desktop_hit(&self, point: Point) -> DesktopHit {
        let hit_shell = |above: bool| self.shell_stack.iter().rev().find_map(|id| {
            let s=self.shells.get(id)?;
            let x=i64::from(point.x)-i64::from(s.geometry.pos.x);
            let y=i64::from(point.y)-i64::from(s.geometry.pos.y);
            (s.mapped && !s.image.pixels.is_empty() && s.above==above && x>=0 && y>=0
                && x<i64::from(s.geometry.size.w) && y<i64::from(s.geometry.size.h))
                .then(||DesktopHit::Shell(*id,Point::new(x as i32,y as i32)))
        });
        if let Some(hit)=hit_shell(true){return hit;}
        if let Some((surface,local))=self.hit_at(point){return match surface {
            SurfaceRef::Client(id)=>DesktopHit::Client(id,local),
            SurfaceRef::Frame(id)=>DesktopHit::Frame(id,local),
        };}
        hit_shell(false).unwrap_or(DesktopHit::Root)
    }
    pub fn queue_shell_motion(&mut self,id:u64,local:Point){
        if !self.shells.get(&id).is_some_and(|s|s.mapped){return;}
        if self.shell_motion.len()>=256 {self.input_fault=Some("shell motion queue overflow".into());return;}
        self.shell_motion.push_back((id,local));
    }
    pub fn queue_shell_click(&mut self,id:u64,local:Point,button:MouseButton,pressed:bool){
        if !self.shells.get(&id).is_some_and(|s|s.mapped){return;}
        if self.shell_clicks.len()>=256 {self.input_fault=Some("shell click queue overflow".into());return;}
        self.shell_clicks.push_back((id,local,button,pressed));
    }
    pub fn add(&mut self, surface: ToplevelSurface) -> u64 {
        let id = self.alloc();
        self.windows.insert(
            id,
            Window {
                surface,
                title: None,
                app_id: None,
                content: Rect::new(Point::new(0, 0), Size::new(1, 1)),
                image: None,
                mapped: false,
                committed: false,
                chrome: ClientChrome::Edges,
                hints: SizeHints::default(),
            },
        );
        self.stack.push(id);
        id
    }
    pub fn remove(&mut self, id: u64) {
        self.windows.remove(&id);
        self.frames.retain(|_, f| f.window != id);
        self.stack.retain(|v| *v != id);
        self.dirty = true;
    }
    pub fn window_for_surface(
        &self,
        surface: &smithay::reexports::wayland_server::protocol::wl_surface::WlSurface,
    ) -> Option<u64> {
        self.windows
            .iter()
            .find(|(_, w)| w.surface.wl_surface() == surface)
            .map(|(id, _)| *id)
    }
    fn restack_window(&mut self, id: u64) {
        self.stack.retain(|v| *v != id);
        self.stack.push(id);
        self.dirty = true;
    }
    pub fn visible(&self, id: u64) -> bool {
        self.windows
            .get(&id)
            .is_some_and(|w| w.mapped && w.image.is_some())
            && self
                .frames
                .values()
                .find(|f| f.window == id)
                .is_none_or(|f| f.mapped)
    }
    pub(crate) fn hit_at(&self,point:Point)->Option<(SurfaceRef<u64,u64>,Point)> {
        use smithay::wayland::compositor::{with_states,SurfaceAttributes};
        fn inside(p:Point,r:Rect)->bool {
            i64::from(p.x)>=i64::from(r.pos.x) && i64::from(p.y)>=i64::from(r.pos.y) &&
            i64::from(p.x)<i64::from(r.pos.x)+i64::from(r.size.w) && i64::from(p.y)<i64::from(r.pos.y)+i64::from(r.size.h)
        }
        for id in self.stack.iter().rev() {
            let w=&self.windows[id];
            let frame=self.frames.iter().find(|(_,f)|f.window==*id);
            if frame.is_some_and(|(_,f)|!f.mapped){continue;}
            if w.mapped && w.image.is_some() && inside(point,w.content){
                let local=Point::new(point.x-w.content.pos.x,point.y-w.content.pos.y);
                let accepts=with_states(w.surface.wl_surface(),|s|s.cached_state.get::<SurfaceAttributes>().current().input_region.as_ref().is_none_or(|r|r.contains((local.x,local.y))));
                if accepts{return Some((SurfaceRef::Client(*id),local));}
                continue;
            }
            if let Some((id,f))=frame {
                let local=Point::new(point.x-f.geometry.pos.x,point.y-f.geometry.pos.y);
                if inside(point,f.geometry) && !f.layout.input_exclusion.is_some_and(|r|inside(local,r)) {
                    return Some((SurfaceRef::Frame(*id),local));
                }
            }
        }
        None
    }
    pub fn render(&self) -> Result<tiny_skia::Pixmap, String> {
        let size = self.screen_size();
        let mut pix =
            tiny_skia::Pixmap::new(size.w, size.h).ok_or("CPU output allocation failed")?;
        pix.fill(tiny_skia::Color::from_rgba8(
            self.background[0],
            self.background[1],
            self.background[2],
            255,
        ));
        let draw = |pix: &mut tiny_skia::Pixmap, pos: Point, image: &DecorationBuffer| {
            if let Some(src) =
                tiny_skia::PixmapRef::from_bytes(&image.pixels, image.width, image.height)
            {
                pix.draw_pixmap(
                    pos.x,
                    pos.y,
                    src,
                    &tiny_skia::PixmapPaint::default(),
                    tiny_skia::Transform::identity(),
                    None,
                );
            }
        };
        if let Some(ref image) = self.background_image {
            draw(&mut pix, Point::new(0, 0), image);
        }
        let shells = |pix: &mut tiny_skia::Pixmap, above: bool| {
            for id in &self.shell_stack {
                if let Some(s) = self.shells.get(id).filter(|s| s.mapped && s.above == above) {
                    draw(pix, s.geometry.pos, &s.image);
                }
            }
        };
        shells(&mut pix, false);
        for id in &self.stack {
            let frame=self.frames.values().find(|f|f.window==*id);
            if !self.visible(*id) && !frame.is_some_and(|f|f.mapped) {
                continue;
            }
            let w = &self.windows[id];
            if let Some(image) = w.image.as_ref().filter(|_|w.mapped) {
                draw(&mut pix, w.content.pos, image);
            }
            if let Some(frame) = self.frames.values().find(|f| f.window == *id) {
                if let Some(surface) = &frame.paint {
                    for solid in &surface.solids {
                        let mut paint = tiny_skia::Paint::default();
                        paint.set_color_rgba8(solid.rgb[0], solid.rgb[1], solid.rgb[2], 255);
                        if let Some(rect) = tiny_skia::Rect::from_xywh(
                            (frame.geometry.pos.x + solid.rect.pos.x) as f32,
                            (frame.geometry.pos.y + solid.rect.pos.y) as f32,
                            solid.rect.size.w as f32,
                            solid.rect.size.h as f32,
                        ) {
                            pix.fill_rect(rect, &paint, tiny_skia::Transform::identity(), None);
                        }
                    }
                    for part in &surface.parts {
                        draw(
                            &mut pix,
                            Point::new(
                                frame.geometry.pos.x + part.offset.x,
                                frame.geometry.pos.y + part.offset.y,
                            ),
                            &part.buffer,
                        );
                    }
                }
            }
        }
        shells(&mut pix, true);
        if let Some((hotspot,image))=&self.cursor {
            draw(&mut pix,Point::new(self.pointer.x-hotspot.x,self.pointer.y-hotspot.y),image);
        }
        Ok(pix)
    }
}
impl Backend for CpuBackend {
    type WindowId = u64;
    type FrameId = u64;
    type ShellId = u64;
    fn scan_existing_windows(&mut self) -> Vec<u64> {
        self.windows.keys().copied().collect()
    }
    fn monitors(&self) -> Vec<MonitorInfo> {
        self.monitors.clone()
    }
    fn monitors_ref(&self) -> &[MonitorInfo] {
        &self.monitors
    }
    fn screen_size(&self) -> Size {
        self.monitors[0].geometry.size
    }
    fn poll_event(&mut self) -> Option<BackendEvent<u64, u64>> {
        None
    } // frontend dispatches synchronously
    fn window_title(&self, id: u64) -> Option<String> {
        self.windows.get(&id)?.title.clone()
    }
    fn window_class(&self, id: u64) -> Option<WmClass> {
        let s = self.windows.get(&id)?.app_id.clone()?;
        Some(WmClass {
            instance: s.clone(),
            class: s,
        })
    }
    fn window_pid(&self, _: u64) -> Option<u32> {
        None
    } // Darwin peer credentials are not qualified
    fn size_hints(&self, id: u64) -> SizeHints {
        self.windows[&id].hints
    }
    fn window_parent(&self, id: u64) -> Option<u64> {
        self.window_for_surface(&self.windows.get(&id)?.surface.parent()?)
    }
    fn supports_protocol(&self, _: u64, p: WmProtocol) -> bool {
        p == WmProtocol::DeleteWindow
    }
    fn window_geometry(&self, id: u64) -> Rect {
        self.windows[&id].content
    }
    fn client_chrome(&self, id: u64) -> ClientChrome {
        self.windows[&id].chrome
    }
    fn capture_window_image(&self, id: u64, _: Size) -> Option<DecorationBuffer> {
        self.windows.get(&id)?.image.clone()
    }
    fn create_decoration(&mut self, id: u64, l: &DecorationLayout) -> u64 {
        let frame = self.alloc();
        if let Some(w) = self.windows.get_mut(&id) {
            w.content.pos = l.client_offset;
            w.mapped = true;
        }
        self.frames.insert(
            frame,
            Frame {
                window: id,
                geometry: Rect::new(Point::new(0, 0), l.frame_size),
                mapped: false,
                paint: None,
                layout: l.clone(),
            },
        );
        self.dirty = true;
        frame
    }
    fn destroy_decoration(&mut self, id: u64) {
        self.frames.remove(&id);
        self.dirty = true;
    }
    fn paint_decoration(&mut self, id: u64, p: &DecorationSurface) {
        if let Some(f) = self.frames.get_mut(&id) {
            f.paint = Some(p.clone());
        }
        self.dirty = true;
    }
    fn set_frame_cursor(&mut self, _: u64, _: Option<ResizeEdge>) {} // no advertised pointer capability
    fn set_frame_geometry(&mut self, id: u64, g: Rect) {
        if let Some(f) = self.frames.get_mut(&id) {
            if let Some(w) = self.windows.get_mut(&f.window) {
                w.content.pos.x += g.pos.x - f.geometry.pos.x;
                w.content.pos.y += g.pos.y - f.geometry.pos.y;
            }
            f.geometry = g;
        }
        self.dirty = true;
    }
    fn set_decoration_layout(&mut self, id: u64, l: &DecorationLayout) {
        if let Some(f)=self.frames.get_mut(&id){f.layout=l.clone();}
        if l.titlebar_height == 0 && l.client_offset == Point::new(0, 0) {
            if let Some(f) = self.frames.get_mut(&id) {
                f.paint = None;
            }
        }
        self.dirty = true;
    }
    fn resize_client(&mut self, id: u64, size: Size) {
        if let Some(w) = self.windows.get_mut(&id) {
            w.content.size = size;
            w.surface
                .with_pending_state(|s| s.size = Some((size.w as i32, size.h as i32).into()));
            w.surface.send_pending_configure();
        }
        self.dirty = true;
    }
    fn accept_client_size(&mut self, id: u64, size: Size) {
        if let Some(w) = self.windows.get_mut(&id) {
            w.content.size = size;
        }
        self.dirty = true;
    }
    fn configure_unmanaged(&mut self, id: u64, g: Rect) {
        if let Some(w) = self.windows.get_mut(&id) {
            w.content = g;
        }
        self.resize_client(id, g.size);
    }
    fn map_frame(&mut self, id: u64) {
        if let Some(f) = self.frames.get_mut(&id) {
            f.mapped = true;
        }
        self.dirty = true;
    }
    fn unmap_frame(&mut self, id: u64) {
        if let Some(f) = self.frames.get_mut(&id) {
            f.mapped = false;
        }
        self.dirty = true;
    }
    fn set_client_mapped(&mut self, id: u64, mapped: bool) {
        if let Some(w) = self.windows.get_mut(&id) {
            w.mapped = mapped;
        }
        self.dirty = true;
    }
    fn map_frameless(&mut self, id: u64) {
        self.set_client_mapped(id, true);
    }
    fn unmap_frameless(&mut self, id: u64) {
        self.set_client_mapped(id, false);
    }
    fn position_client(&mut self, id: u64, p: Point) {
        let f = self
            .frames
            .values()
            .find(|f| f.window == id)
            .map(|f| f.geometry.pos)
            .unwrap_or(Point::new(0, 0));
        if let Some(w) = self.windows.get_mut(&id) {
            w.content.pos = Point::new(f.x + p.x, f.y + p.y);
        }
        self.dirty = true;
    }
    fn raise(&mut self, id: u64) {
        if let Some(f) = self.frames.get(&id) {
            self.restack_window(f.window);
        }
    }
    fn raise_frameless(&mut self, id: u64) {
        self.restack_window(id);
    }
    fn restack(&mut self, ids: &[u64]) {
        for id in ids {
            self.raise(*id);
        }
    }
    fn set_input_focus(&mut self, id: u64) {
        for (key, w) in &self.windows {
            w.surface.with_pending_state(|s| {
                if *key == id {
                    s.states.set(xdg_toplevel::State::Activated);
                } else {
                    s.states.unset(xdg_toplevel::State::Activated);
                }
            });
            w.surface.send_pending_configure();
        }
        self.dirty = true;
    }
    fn send_close(&mut self, id: u64) {
        if let Some(w) = self.windows.get(&id) {
            w.surface.send_close();
        }
    }
    fn grab_pointer_for_drag(&mut self) -> DragHandle {
        assert!(self.input_enabled,"cannot grab without an input source");
        assert!(self.pointer_grab.is_none(),"nested pointer grab");
        assert!(self.popup_grab.is_none(),"drag while popup owns pointer");
        let handle=DragHandle(self.alloc());self.pointer_grab=Some(handle);handle
    }
    fn ungrab_pointer(&mut self, handle: DragHandle) {
        assert_eq!(self.pointer_grab,Some(handle));self.pointer_grab=None;
    }
    fn grab_keyboard(&mut self){assert!(!self.keyboard_grabbed,"nested keyboard capture");self.keyboard_grabbed=true;}
    fn ungrab_keyboard(&mut self){assert!(self.keyboard_grabbed,"keyboard capture not owned");self.keyboard_grabbed=false;}
    fn take_shell_motion(&mut self)->Option<(u64,Point)>{self.shell_motion.pop_front()}
    fn take_shell_click(&mut self)->Option<(u64,Point,MouseButton,bool)>{self.shell_clicks.pop_front()}
    fn grab_key(&mut self, combo: KeyCombo) {
        self.grabbed_keys.insert(combo);
    }
    fn set_keyboard_config(&mut self,config:KeyboardConfig){
        // This frontend creates this exact keymap and repeat policy in
        // enable_pdhid_seat_caps. No runtime reconfiguration is advertised.
        let valid=self.input_enabled && config.rules.as_deref()==Some("evdev")
            && config.model.as_deref()==Some("pc105") && config.layout.as_deref()==Some("us")
            && config.variant.as_deref()==Some("") && config.options.is_none()
            && config.repeat_rate==Some(25) && config.repeat_delay==Some(500)
            && config.numlock_by_default.is_none();
        if valid{self.keyboard_config_verified=true;}
        else{self.input_fault=Some("unsupported keyboard configuration".into());}
    }
    fn ungrab_key(&mut self, combo: KeyCombo) {
        self.grabbed_keys.remove(&combo);
    }
    fn pointer_position(&self)->Option<Point>{self.input_enabled.then_some(self.pointer)}
    // All pointer events already arrive at the compositor; passive X11 grabs
    // are unnecessary. replay_pointer decides whether the triggering click
    // is delivered to the Wayland client after wm-core policy runs.
    fn grab_button_passive(&mut self, _: u64, _: MouseButton) {}
    fn ungrab_button_passive(&mut self, _: u64, _: MouseButton) {}
    fn replay_pointer(&mut self) {self.replay=true;}
    fn create_shell_surface(&mut self, g: Rect, bg: (u8, u8, u8), above: bool) -> Option<u64> {
        let len = crate::pixels::image_len(g.size.w, g.size.h).ok()?;
        let mut pixels = vec![0; len];
        for p in pixels.chunks_exact_mut(4) {
            p.copy_from_slice(&[bg.0, bg.1, bg.2, 255]);
        }
        let id = self.alloc();
        self.shells.insert(
            id,
            Shell {
                geometry: g,
                mapped: false,
                above,
                image: DecorationBuffer {
                    width: g.size.w,
                    height: g.size.h,
                    pixels,
                },
            },
        );
        self.shell_stack.push(id);
        Some(id)
    }
    fn map_shell_surface(&mut self, id: u64) {
        if let Some(s) = self.shells.get_mut(&id) {
            s.mapped = true;
        }
        self.dirty = true;
    }
    fn unmap_shell_surface(&mut self, id: u64) {
        if let Some(s) = self.shells.get_mut(&id) {
            s.mapped = false;
        }
        self.dirty = true;
    }
    fn destroy_shell_surface(&mut self, id: u64) {
        self.shells.remove(&id);
        self.shell_stack.retain(|v| *v != id);
        self.shell_motion.retain(|(v,_)|*v!=id);
        self.shell_clicks.retain(|(v,_,_,_)|*v!=id);
        self.dirty = true;
    }
    fn raise_shell_surface(&mut self, id: u64) {
        self.shell_stack.retain(|v| *v != id);
        self.shell_stack.push(id);
        self.dirty = true;
    }
    fn configure_shell_surface(&mut self, id: u64, g: Rect) {
        if let Some(s) = self.shells.get_mut(&id) {
            s.geometry = g;
        }
        self.dirty = true;
    }
    fn paint_shell_surface(&mut self, id: u64, p: &DecorationBuffer) {
        if let Some(s) = self.shells.get_mut(&id) {
            s.image = p.clone();
        }
        self.dirty = true;
    }
    fn release_shell_buffer(&mut self, id: u64) {
        self.unmap_shell_surface(id);
        if let Some(s) = self.shells.get_mut(&id) {
            s.image.pixels.clear();
        }
    }
    fn paint_root_color(&mut self, rgb: (u8, u8, u8)) {
        self.background = [rgb.0, rgb.1, rgb.2];
        self.background_image = None;
        self.dirty = true;
    }
    fn paint_root_image(&mut self, p: &DecorationBuffer) {
        self.background_image = Some(p.clone());
        self.dirty = true;
    }
}

impl wm_theme_api::PopupHost for CpuBackend {
    type PopupId=u64;
    fn create_popup(&mut self,geometry:Rect,background:(u8,u8,u8))->Option<u64>{
        if self.pointer_grab.is_some(){return None;}
        let id=self.create_shell_surface(geometry,background,true)?;
        self.map_shell_surface(id);self.raise_shell_surface(id);Some(id)
    }
    fn destroy_popup(&mut self,id:u64){self.destroy_shell_surface(id);}
    fn paint_popup(&mut self,id:u64,buffer:&DecorationBuffer){
        let valid=self.shells.get(&id).is_some_and(|s|s.geometry.size==Size::new(buffer.width,buffer.height))
            && crate::pixels::image_len(buffer.width,buffer.height).is_ok_and(|n|n==buffer.pixels.len());
        if !valid{self.input_fault=Some("invalid popup paint".into());return;}
        self.paint_shell_surface(id,buffer);
    }
    fn grab_pointer(&mut self)->wm_theme_api::PopupGrab{
        assert!(self.input_enabled && self.pointer_grab.is_none() && self.popup_grab.is_none(),"popup ownership conflict");
        let token=wm_theme_api::PopupGrab(self.alloc());self.popup_grab=Some(token);token
    }
    fn ungrab_pointer(&mut self,token:wm_theme_api::PopupGrab){assert_eq!(self.popup_grab,Some(token));self.popup_grab=None;}
    fn grab_keyboard(&mut self){Backend::grab_keyboard(self);}
    fn ungrab_keyboard(&mut self){Backend::ungrab_keyboard(self);}
}
