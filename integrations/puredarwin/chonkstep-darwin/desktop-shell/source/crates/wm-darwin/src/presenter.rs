//! PDGOP connection is kept alive until all mapped writes have stopped.
use wm_core::Size;
pub trait Presenter {
    fn present(&mut self, frame: &tiny_skia::Pixmap) -> Result<(), String>;
}

// Layout matches PureDarwin src/Libraries/PDGOP/include/PDGOP.h (LP64).
#[repr(C)]
#[derive(Default)]
pub struct RawFramebuffer {
    pub master_port: u32,
    pub service: u32,
    pub connect: u32,
    pub address: u64,
    pub size: u64,
    pub width: u32,
    pub height: u32,
    pub stride: u32,
    pub bpp: u32,
    pub pixel_type: u32,
    pub component_masks: [u32; 3],
}
#[cfg(all(target_os = "macos", target_arch = "aarch64"))]
mod native {
    use super::*;
    use crate::pixels::FramebufferLayout;
    use std::marker::PhantomData;
    use std::rc::Rc;
    #[link(name = "PDGOP", kind = "static")]
    unsafe extern "C" {
        fn PDGOPOpen(fb: *mut RawFramebuffer) -> i32;
        fn PDGOPClose(fb: *mut RawFramebuffer);
    }
    // PureDarwin PDIOKitLib.h:92 / minimal IOKitLib.c:237. This is intentionally
    // NOT Apple's two-argument boolean-returning IOObjectConformsTo ABI.
    #[link(name = "IOKit", kind = "static")]
    unsafe extern "C" {
        fn IOObjectConformsTo(object: u32, class: *const libc::c_char, conforms: *mut i32) -> i32;
    }
    pub struct Pdgop {
        raw: RawFramebuffer,
        layout: FramebufferLayout,
        _single_thread: PhantomData<Rc<()>>,
    }
    impl Pdgop {
        pub fn open() -> Result<Self, String> {
            let mut raw = RawFramebuffer::default();
            let kr = unsafe { PDGOPOpen(&mut raw) };
            let layout = FramebufferLayout {
                width: raw.width,
                height: raw.height,
                stride: raw.stride,
                size: raw.size,
                bpp: raw.bpp,
                pixel_type: raw.pixel_type,
                masks: raw.component_masks,
            };
            let checked = (|| {
                if kr != 0 {
                    return Err(format!("PDGOPOpen failed: {kr:#x}"));
                }
                if raw.address == 0
                    || raw.address % 4 != 0
                    || raw.connect == 0
                    || raw.service == 0
                    || raw.address.checked_add(raw.size).is_none()
                {
                    return Err("PDGOP returned invalid mapping/connection".into());
                }
                layout.validate()?;
                let mut conforms = 0i32; // mach/arm/boolean.h: boolean_t is int
                let kr = unsafe {
                    IOObjectConformsTo(raw.service, c"IOGOPFramebuffer".as_ptr(), &mut conforms)
                };
                if kr != 0 {
                    return Err(format!("IOObjectConformsTo failed: {kr:#x}"));
                }
                if conforms == 0 {
                    return Err("native presenter supports only IOGOPFramebuffer; VirtIO present protocol is distinct".into());
                }
                Ok(())
            })();
            if let Err(error) = checked {
                unsafe { PDGOPClose(&mut raw) };
                return Err(error);
            }
            Ok(Self {
                raw,
                layout,
                _single_thread: PhantomData,
            })
        }
        pub fn size(&self) -> Size {
            Size::new(self.layout.width, self.layout.height)
        }
    }
    impl Presenter for Pdgop {
        fn present(&mut self, frame: &tiny_skia::Pixmap) -> Result<(), String> {
            if frame.width() != self.layout.width || frame.height() != self.layout.height {
                return Err("PDGOP frame dimensions changed".into());
            }
            let native = self.layout.convert(frame.data())?;
            // WC mapped IOGOP scanout needs stores, not VirtIO selector 11 (gamma on IOGOP).
            // Only visible pixels are touched; preserve row padding. Mapping and user client
            // remain owned by this !Send/!Sync object until its Drop.
            for y in 0..self.layout.height as usize {
                for x in 0..self.layout.width as usize {
                    let offset = y * self.layout.stride as usize + x * 4;
                    let word = u32::from_le_bytes(native[offset..offset + 4].try_into().unwrap());
                    unsafe {
                        ((self.raw.address as usize + offset) as *mut u32).write_volatile(word)
                    };
                }
            }
            unsafe { std::arch::asm!("dsb oshst", options(nostack, preserves_flags)) };
            Ok(()) // store completion only: no vblank, panel, RPC or desktop claim
        }
    }
    impl Drop for Pdgop {
        fn drop(&mut self) {
            unsafe { PDGOPClose(&mut self.raw) }
        }
    } // library unmaps before IOServiceClose
}
#[cfg(all(target_os = "macos", target_arch = "aarch64"))]
pub use native::Pdgop;
#[cfg(not(all(target_os = "macos", target_arch = "aarch64")))]
pub struct Pdgop;
#[cfg(not(all(target_os = "macos", target_arch = "aarch64")))]
impl Pdgop {
    pub fn open() -> Result<Self, String> {
        Err("native PDGOP requires aarch64-apple-darwin; no emulated native success".into())
    }
    pub fn size(&self) -> Size {
        unreachable!("Pdgop::open cannot succeed on this platform")
    }
}
#[cfg(not(all(target_os = "macos", target_arch = "aarch64")))]
impl Presenter for Pdgop {
    fn present(&mut self, _: &tiny_skia::Pixmap) -> Result<(), String> {
        Err("native PDGOP unavailable".into())
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    #[test]
    fn pdgop_lp64_layout() {
        assert_eq!(std::mem::size_of::<RawFramebuffer>(), 64);
        assert_eq!(std::mem::align_of::<RawFramebuffer>(), 8);
        assert_eq!(std::mem::offset_of!(RawFramebuffer, address), 16);
        assert_eq!(std::mem::offset_of!(RawFramebuffer, size), 24);
        assert_eq!(std::mem::offset_of!(RawFramebuffer, width), 32);
        assert_eq!(std::mem::offset_of!(RawFramebuffer, component_masks), 52);
    }
}
