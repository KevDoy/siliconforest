//! Real seat delivery shared by native PDHID and host wire tests.
use std::collections::HashSet;
use smithay::{input::{Seat,keyboard::{KeyboardHandle,FilterResult,ModifiersState,XkbConfig},
    pointer::{PointerHandle,MotionEvent,ButtonEvent,AxisFrame,CursorImageStatus,CursorImageSurfaceData}},
    backend::input::{KeyState,ButtonState,Axis,AxisSource},utils::SERIAL_COUNTER,
    reexports::wayland_server::{DisplayHandle,protocol::{wl_surface::WlSurface,wl_output,wl_seat::WlSeat}},
    wayland::compositor::{with_states,SurfaceAttributes,BufferAssignment}};
use wm_core::{Backend,BackendEvent,SurfaceRef,Point,MouseButton,Modifiers,KeyCombo};
use wm_theme_api::DecorationBuffer;
use crate::{server::Server,hid::{Event,Motion,SeatCaps,accept_for_seat}};

pub(crate) struct InputState {
    pub seat:Seat<Server>,keyboard:KeyboardHandle<Server>,pointer:Option<PointerHandle<Server>>,
    position:(f64,f64),buttons:u8,forwarded:u8,modifiers:Modifiers,last_time:u32,
    suppressed_keys:HashSet<u32>,pub cursor_surface:Option<WlSurface>,
    #[cfg(feature = "desktop-shell")]
    deferred_focus:bool,
}
fn modifiers(m:&ModifiersState)->Modifiers {
    let mut out=Modifiers::empty();out.set(Modifiers::SHIFT,m.shift);out.set(Modifiers::CONTROL,m.ctrl);
    out.set(Modifiers::ALT,m.alt);out.set(Modifiers::SUPER,m.logo);out
}
fn arrow()->DecorationBuffer {
    let mut pixels=vec![0;12*18*4];
    for y in 0..16 {for x in 0..=(y*2/3).min(10){let i=(y*12+x)*4;
        let shade=if x==0 || x==y*2/3 || y==15 {0}else{255};pixels[i..i+4].copy_from_slice(&[shade,shade,shade,255]);}}
    DecorationBuffer{width:12,height:18,pixels}
}
impl Server {
    #[cfg(feature = "desktop-shell")]
    pub(crate) fn desktop_seat_ready(&self)->bool{self.input.as_ref().is_some_and(|i|i.pointer.is_some())}
    pub(crate) fn authorize_client_drag(&mut self,surface:&smithay::wayland::shell::xdg::ToplevelSurface,seat:&WlSeat,serial:smithay::utils::Serial)->Option<u64>{
        let input=self.input.as_ref()?;
        let pointer=input.pointer.as_ref()?;
        if !input.seat.owns(seat) || !pointer.has_grab(serial) || input.buttons&1==0{return None;}
        let start=pointer.grab_start_data()?;
        if start.button!=0x110 || start.focus?.0!=*surface.wl_surface(){return None;}
        let id=self.wm.backend().window_for_surface(surface.wl_surface())?;
        let pointer=pointer.clone();let position=input.position;let time=input.last_time;
        // Transfer the authenticated client implicit grab to wm-core. Leaving
        // the client resets its pointer interaction; no synthetic key/button is sent.
        pointer.unset_grab(self,serial,time);
        pointer.motion(self,None,&MotionEvent{location:position.into(),serial,time});
        self.input.as_mut().unwrap().forwarded=0;
        Some(id)
    }
    /// The production caller invokes this only after NativeHid::open succeeds.
    /// Tests exercise the protocol independently; this method makes no device claim.
    pub fn enable_pdhid_seat(&mut self,dh:&DisplayHandle)->Result<(),String>{
        self.enable_pdhid_seat_caps(dh,SeatCaps{keyboard:true,pointer:true})
    }
    /// `caps.pointer` is false for `--input pdhid-keyboard`: no pointer handle,
    /// so the seat does not advertise a pointer. This is not native input.
    pub fn enable_pdhid_seat_caps(&mut self,dh:&DisplayHandle,caps:SeatCaps)->Result<(),String>{
        if self.input.is_some(){return Err("PDHID seat already enabled".into());}
        if !caps.keyboard{return Err("PDHID seat requires keyboard capability".into());}
        let mut seat=self.seats.new_wl_seat(dh,"puredarwin-hid");
        let keyboard=seat.add_keyboard(XkbConfig{rules:"evdev",model:"pc105",layout:"us",variant:"",options:None},500,25)
            .map_err(|e|format!("XKB keymap initialization failed: {e:?}"))?;
        let pointer=if caps.pointer{Some(seat.add_pointer())}else{None};
        self.input=Some(InputState{seat,keyboard,pointer,position:(0.,0.),buttons:0,forwarded:0,
            modifiers:Modifiers::empty(),last_time:0,suppressed_keys:HashSet::new(),cursor_surface:None,
            #[cfg(feature = "desktop-shell")]
            deferred_focus:false});
        let b=self.wm.backend_mut();b.input_enabled=true;
        if caps.pointer{b.cursor=Some((Point::new(0,0),arrow()));}
        b.dirty=true;
        self.sync_input_focus();Ok(())
    }
    pub(crate) fn sync_input_focus(&mut self){
        let Some(input)=&mut self.input else{return;};let keyboard=input.keyboard.clone();
        #[cfg(feature = "desktop-shell")]
        let blocked=if self.desktop.is_some(){
            if self.wm.backend().keyboard_grabbed {input.deferred_focus=true;}
            if !self.wm.backend().keyboard_grabbed && keyboard.pressed_keys().is_empty(){input.deferred_focus=false;}
            input.deferred_focus || self.wm.backend().keyboard_grabbed
        } else {false};
        #[cfg(not(feature = "desktop-shell"))]
        let blocked=false;
        let focus=if blocked{None}else{self.wm.focused_client().and_then(|id|self.wm.client(id)).map(|c|c.window)
            .filter(|id|self.wm.backend().visible(*id)).map(|id|self.wm.backend().windows[&id].surface.wl_surface().clone())};
        if keyboard.current_focus()!=focus{keyboard.set_focus(self,focus,SERIAL_COUNTER.next_serial());}
    }
    pub fn deliver_hid(&mut self,event:Event,time:u32)->Result<(),String>{
        if self.input.is_none(){return Err("no PDHID seat; event not delivered".into());}
        let caps=SeatCaps{keyboard:true,pointer:self.input.as_ref().unwrap().pointer.is_some()};
        accept_for_seat(caps,&event)?;
        self.input.as_mut().unwrap().last_time=time;
        self.sync_input_focus();
        match event {
            Event::Key{evdev,down}=>{
                let keyboard=self.input.as_ref().unwrap().keyboard.clone();
                keyboard.input::<(),_>(self,(evdev+8).into(),if down{KeyState::Pressed}else{KeyState::Released},SERIAL_COUNTER.next_serial(),time,
                    |s,m,key|{
                        let mods=modifiers(m);s.input.as_mut().unwrap().modifiers=mods;
                        if !down && s.input.as_mut().unwrap().suppressed_keys.remove(&evdev){return FilterResult::Intercept(());}
                        let sym=key.raw_latin_sym_or_raw_current_sym().unwrap_or_else(||key.modified_sym());
                        let combo=KeyCombo{keysym:sym.raw(),modifiers:mods};
                        #[cfg(feature = "desktop-shell")]
                        if s.desktop.is_some(){
                            match s.desktop_key(combo,down){
                                Ok(true)=>{
                                    if down {let input=s.input.as_mut().unwrap();input.suppressed_keys.insert(evdev);input.deferred_focus=true;}
                                    else {s.input.as_ref().unwrap().keyboard.retire_forwarded_key((evdev+8).into());}
                                    return FilterResult::Intercept(());
                                }
                                Err(e)=>{s.wm.backend_mut().input_fault=Some(e);return FilterResult::Intercept(());}
                                Ok(false)=>{}
                            }
                            if s.wm.backend().keyboard_grabbed {
                                if down {s.wm.dispatch(BackendEvent::KeyPress(combo));s.input.as_mut().unwrap().suppressed_keys.insert(evdev);}
                                else {s.wm.dispatch(BackendEvent::KeyRelease(combo));s.input.as_ref().unwrap().keyboard.retire_forwarded_key((evdev+8).into());}
                                return FilterResult::Intercept(());
                            }
                        }
                        if down && s.wm.backend().grabbed_keys.contains(&combo){
                            s.input.as_mut().unwrap().suppressed_keys.insert(evdev);
                            s.wm.dispatch(BackendEvent::KeyPress(combo));FilterResult::Intercept(())
                        }else{FilterResult::Forward}
                    });
            }
            Event::Pointer{motion,buttons,wheel}=>{
                let pointer=self.input.as_ref().unwrap().pointer.clone().ok_or("pointer event rejected; seat has no pointer")?;
                let size=self.wm.backend().screen_size();
                let input=self.input.as_mut().unwrap();
                let (x,y)=match motion {Motion::Absolute(x,y)=>(f64::from(x)*f64::from(size.w-1)/65535.,f64::from(y)*f64::from(size.h-1)/65535.),
                    Motion::Relative(x,y)=>(input.position.0+f64::from(x),input.position.1+f64::from(y))};
                input.position=(x.clamp(0.,f64::from(size.w-1)),y.clamp(0.,f64::from(size.h-1)));
                let root=Point::new(input.position.0.floor() as i32,input.position.1.floor() as i32);
                let previous=input.buttons;let mods=input.modifiers;
                let b=self.wm.backend_mut();b.pointer=root;b.dirty=true;
                #[cfg(feature = "desktop-shell")]
                if self.input.as_ref().unwrap().forwarded==0 && !self.wm.interactive_drag_active()
                    && self.desktop_pointer(root,buttons,previous)? {
                    pointer.motion(self,None,&MotionEvent{location:(f64::from(root.x),f64::from(root.y)).into(),serial:SERIAL_COUNTER.next_serial(),time});
                    pointer.frame(self);
                    self.input.as_mut().unwrap().buttons=buttons;
                    self.service_desktop()?;
                    self.sync_input_focus();
                    return Ok(());
                }
                let hit=self.wm.backend().hit_at(root);
                self.wm.dispatch(BackendEvent::PointerMotion{root,surface_local:hit});
                // Move/resize grabs belong to wm-core. Client implicit grabs belong to Smithay.
                let focus=if self.wm.interactive_drag_active(){None}else{
                    self.wm.backend().hit_at(root).and_then(|(hit,_)|match hit{
                        SurfaceRef::Client(id)=>{let w=&self.wm.backend().windows[&id];Some((w.surface.wl_surface().clone(),(f64::from(w.content.pos.x),f64::from(w.content.pos.y)).into()))},_=>None})};
                pointer.motion(self,focus,&MotionEvent{location:(x.clamp(0.,f64::from(size.w-1)),y.clamp(0.,f64::from(size.h-1))).into(),serial:SERIAL_COUNTER.next_serial(),time});
                for bit in 0..3 {
                    let mask=1<<bit;if (previous^buttons)&mask==0{continue;}
                    let pressed=buttons&mask!=0;
                    let forwarded=self.input.as_ref().unwrap().forwarded;
                    let had_drag=self.wm.interactive_drag_active();
                    let button=match bit{0=>MouseButton::Left,1=>MouseButton::Right,_=>MouseButton::Middle};
                    self.wm.backend_mut().replay=false;
                    if forwarded==0 || !pressed {
                        if let Some((surface,local))=self.wm.backend().hit_at(root){self.wm.dispatch(BackendEvent::PointerButton{surface,local,button,pressed,time_ms:time,mods});}
                        else if !pressed{self.wm.dispatch(BackendEvent::DragEnded);}
                    }
                    let send=if pressed {forwarded!=0 || (!had_drag && !self.wm.interactive_drag_active() && self.wm.backend().replay)}else{forwarded&mask!=0};
                    if send {
                        pointer.button(self,&ButtonEvent{serial:SERIAL_COUNTER.next_serial(),time,button:0x110+bit,state:if pressed{ButtonState::Pressed}else{ButtonState::Released}});
                        let forwarded=&mut self.input.as_mut().unwrap().forwarded;
                        if pressed{*forwarded|=mask;}else{*forwarded&=!mask;}
                    }
                }
                self.input.as_mut().unwrap().buttons=buttons;
                if wheel!=0 && !self.wm.interactive_drag_active(){pointer.axis(self,AxisFrame::new(time).source(AxisSource::Wheel).value(Axis::Vertical,-f64::from(wheel)*10.).v120(Axis::Vertical,-i32::from(wheel)*120));}
                pointer.frame(self);
            }
        }
        #[cfg(feature = "desktop-shell")]
        self.service_desktop()?;
        self.sync_input_focus();Ok(())
    }
    pub(crate) fn cursor_image_changed(&mut self,image:CursorImageStatus){
        let Some(input)=&mut self.input else{return;};
        input.cursor_surface=match &image {CursorImageStatus::Surface(surface)=>Some(surface.clone()),_=>None};
        if let Some(surface)=input.cursor_surface.clone(){self.import_cursor(&surface);}
        else{self.wm.backend_mut().cursor=match image{CursorImageStatus::Hidden=>None,_=>Some((Point::new(0,0),arrow()))};self.wm.backend_mut().dirty=true;}
    }
    pub(crate) fn import_cursor(&mut self,surface:&WlSurface){
        let (buffer,scale,transform,hotspot,callbacks)=with_states(surface,|s|{
            let hotspot=s.data_map.get::<CursorImageSurfaceData>().map(|d|d.lock().unwrap().hotspot).unwrap_or_default();
            let mut a=s.cached_state.get::<SurfaceAttributes>();let a=a.current();a.damage.clear();
            (a.buffer.take(),a.buffer_scale,a.buffer_transform,hotspot,std::mem::take(&mut a.frame_callbacks))
        });
        if scale!=1 || transform!=wl_output::Transform::Normal{Self::refuse(surface,"unsupported cursor transform/scale");return;}
        match buffer{
            Some(BufferAssignment::NewBuffer(buffer))=>{
                let image=crate::pixels::copy_shm(&buffer);buffer.release();
                match image{Ok(image) if image.width<=256 && image.height<=256=>self.wm.backend_mut().cursor=Some((Point::new(hotspot.x,hotspot.y),image)),
                    Ok(_)=>Self::refuse(surface,"cursor exceeds 256x256"),Err(e)=>Self::refuse(surface,&e)}
            }
            Some(BufferAssignment::Removed)=>self.wm.backend_mut().cursor=None,
            None=>{if let Some((old,_))=&mut self.wm.backend_mut().cursor{*old=Point::new(hotspot.x,hotspot.y);}}
        }
        self.wm.backend_mut().dirty=true;
        self.cursor_callbacks.extend(callbacks);
    }
}
