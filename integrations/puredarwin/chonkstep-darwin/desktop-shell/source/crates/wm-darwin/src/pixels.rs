//! Bounded owned pixels. Client memory is copied before wl_buffer.release.
use smithay::reexports::wayland_server::protocol::{wl_buffer::WlBuffer, wl_shm};
use smithay::wayland::shm::with_buffer_contents;
use wm_theme_api::DecorationBuffer;

pub const MAX_IMAGE_BYTES: usize = 64 * 1024 * 1024;

pub fn image_len(width: u32, height: u32) -> Result<usize, String> {
    let len = (width as usize)
        .checked_mul(height as usize)
        .and_then(|n| n.checked_mul(4))
        .filter(|n| *n != 0 && *n <= MAX_IMAGE_BYTES)
        .ok_or("invalid or oversized image")?;
    Ok(len)
}

pub fn copy_shm(buffer: &WlBuffer) -> Result<DecorationBuffer, String> {
    with_buffer_contents(buffer, |ptr, pool_len, info| {
        let width = u32::try_from(info.width).map_err(|_| "negative width")?;
        let height = u32::try_from(info.height).map_err(|_| "negative height")?;
        let len = image_len(width, height)?;
        let stride = usize::try_from(info.stride).map_err(|_| "negative stride")?;
        let offset = usize::try_from(info.offset).map_err(|_| "negative offset")?;
        let row = width as usize * 4;
        let end = stride
            .checked_mul(height as usize - 1)
            .and_then(|n| n.checked_add(row))
            .and_then(|n| n.checked_add(offset))
            .ok_or("SHM range overflow")?;
        if stride < row || end > pool_len {
            return Err("SHM range outside pool".into());
        }
        if !matches!(
            info.format,
            wl_shm::Format::Argb8888 | wl_shm::Format::Xrgb8888
        ) {
            return Err("only ARGB8888/XRGB8888 SHM is supported".into());
        }
        let mut pixels = vec![0; len];
        for y in 0..height as usize {
            for x in 0..width as usize {
                let source = offset + y * stride + x * 4;
                let dest = (y * width as usize + x) * 4;
                // No Rust references to mutable client memory; Smithay contains SIGBUS.
                // All offsets were checked above. Wayland SHM is native endian;
                // this backend supports little-endian Darwin ARM64 / host tests only.
                let mut bgra = [0u8; 4];
                for (i, byte) in bgra.iter_mut().enumerate() {
                    *byte = unsafe { ptr.add(source + i).read_volatile() };
                }
                let alpha = if info.format == wl_shm::Format::Xrgb8888 {
                    255
                } else {
                    bgra[3]
                };
                // Malicious non-premultiplied data cannot violate tiny-skia's invariant.
                pixels[dest..dest + 4].copy_from_slice(&[
                    bgra[2].min(alpha),
                    bgra[1].min(alpha),
                    bgra[0].min(alpha),
                    alpha,
                ]);
            }
        }
        Ok(DecorationBuffer {
            width,
            height,
            pixels,
        })
    })
    .map_err(|e| format!("SHM access failed: {e:?}"))?
}

/// Validated 32-bit direct framebuffer layout, with either 8- or 10-bit RGB.
#[derive(Clone, Copy, Debug)]
pub struct FramebufferLayout {
    pub width: u32,
    pub height: u32,
    pub stride: u32,
    pub size: u64,
    pub bpp: u32,
    pub pixel_type: u32,
    pub masks: [u32; 3],
}
impl FramebufferLayout {
    pub fn validate(self) -> Result<usize, String> {
        image_len(self.width, self.height)?;
        if self.bpp != 32 || self.pixel_type != 2 {
            return Err("PDGOP requires 32-bit RGB direct pixels".into());
        }
        if self.masks != [0x00ff0000, 0x0000ff00, 0x000000ff]
            && self.masks != [0x3ff00000, 0x000ffc00, 0x000003ff]
        {
            return Err("unsupported PDGOP RGB masks".into());
        }
        let span = u64::from(self.stride)
            .checked_mul(u64::from(self.height))
            .ok_or("framebuffer span overflow")?;
        if u64::from(self.stride) < u64::from(self.width) * 4
            || self.stride % 4 != 0
            || span > self.size
            || span > MAX_IMAGE_BYTES as u64
        {
            return Err("framebuffer layout exceeds mapped span".into());
        }
        Ok(span as usize)
    }
    pub fn pack_rgb(self, rgba: &[u8]) -> u32 {
        self.masks
            .iter()
            .zip(rgba)
            .fold(0, |word, (mask, channel)| {
                let shift = mask.trailing_zeros();
                let max = mask >> shift;
                word | (((u32::from(*channel) * max + 127) / 255) << shift)
            })
    }
    pub fn convert(self, rgba: &[u8]) -> Result<Vec<u8>, String> {
        let span = self.validate()?;
        if rgba.len() != image_len(self.width, self.height)? {
            return Err("frame size mismatch".into());
        }
        let mut native = vec![0; span];
        for y in 0..self.height as usize {
            for x in 0..self.width as usize {
                let src = (y * self.width as usize + x) * 4;
                let dst = y * self.stride as usize + x * 4;
                native[dst..dst + 4]
                    .copy_from_slice(&self.pack_rgb(&rgba[src..src + 4]).to_le_bytes());
            }
        }
        Ok(native)
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    #[test]
    fn rgb30_stride_and_bounds() {
        let l = FramebufferLayout {
            width: 2,
            height: 1,
            stride: 12,
            size: 12,
            bpp: 32,
            pixel_type: 2,
            masks: [0x3ff00000, 0xffc00, 0x3ff],
        };
        let data = l.convert(&[255, 0, 0, 255, 0, 128, 255, 255]).unwrap();
        assert_eq!(
            u32::from_le_bytes(data[..4].try_into().unwrap()),
            0x3ff00000
        );
        assert_eq!(
            u32::from_le_bytes(data[4..8].try_into().unwrap()),
            (514 << 10) | 1023
        );
        assert_eq!(&data[8..], &[0; 4]);
        assert!(FramebufferLayout { size: 11, ..l }.validate().is_err());
        assert!(FramebufferLayout { stride: 7, ..l }.validate().is_err());
        assert!(FramebufferLayout { masks: [0; 3], ..l }.validate().is_err());
        assert!(image_len(u32::MAX, u32::MAX).is_err());
    }
}
