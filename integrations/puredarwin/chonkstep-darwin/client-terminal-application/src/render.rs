//! CPU rendering of the terminal view into an XRGB8888 window buffer: cell
//! backgrounds, glyphs tinted with the cell's foreground, underline and the
//! cursor. Only rows whose content changed are redrawn; the caller writes and
//! damages just those.
use crate::term::{Cell, Term, DEFAULT_BG};
use std::collections::HashMap;
use tiny_skia::{FillRule, Paint, PathBuilder, Pixmap, Transform};
pub const CW: usize = 11;
pub const CH: usize = 22;
pub const PAD: usize = 8;
struct Outline(PathBuilder);
impl ttf_parser::OutlineBuilder for Outline {
    fn move_to(&mut self, x: f32, y: f32) {
        self.0.move_to(x, y)
    }
    fn line_to(&mut self, x: f32, y: f32) {
        self.0.line_to(x, y)
    }
    fn quad_to(&mut self, x1: f32, y1: f32, x: f32, y: f32) {
        self.0.quad_to(x1, y1, x, y)
    }
    fn curve_to(&mut self, x1: f32, y1: f32, x2: f32, y2: f32, x: f32, y: f32) {
        self.0.cubic_to(x1, y1, x2, y2, x, y)
    }
    fn close(&mut self) {
        self.0.close()
    }
}
/// What one row showed when it was last drawn: its cells and the cursor column on it.
type RowKey = (Vec<Cell>, Option<usize>);
pub struct Renderer {
    face: ttf_parser::Face<'static>,
    /// Coverage masks (CW x CH, one byte per pixel); `None`: the font has no such glyph.
    glyphs: HashMap<char, Option<Vec<u8>>>,
    /// Rows as last drawn into the caller's buffer, and that buffer's size.
    shown: Vec<RowKey>,
    shown_size: (usize, usize),
}
impl Renderer {
    pub fn new() -> Result<Self, &'static str> {
        let face = ttf_parser::Face::parse(include_bytes!("../fonts/IBMPlexMono-Regular.ttf"), 0)
            .map_err(|_| "font parse failed")?;
        let mut renderer = Self { face, glyphs: HashMap::new(), shown: Vec::new(), shown_size: (0, 0) };
        for c in 33u8..=126 {
            if renderer.glyph(c as char).is_none() {
                return Err("font lacks printable ASCII");
            }
        }
        Ok(renderer)
    }
    /// The coverage mask of `c`, rasterised on first use.
    fn glyph(&mut self, c: char) -> Option<&[u8]> {
        if !self.glyphs.contains_key(&c) {
            let mask = self.face.glyph_index(c).and_then(|id| {
                let scale = 18.0 / f32::from(self.face.units_per_em());
                let mut outline = Outline(PathBuilder::new());
                self.face.outline_glyph(id, &mut outline);
                let mut p = Pixmap::new(CW as u32, CH as u32)?;
                if let Some(path) = outline.0.finish() {
                    let mut paint = Paint::default();
                    paint.set_color_rgba8(255, 255, 255, 255);
                    p.fill_path(&path, &paint, FillRule::Winding, Transform::from_row(scale, 0., 0., -scale, 0., 17.), None);
                }
                Some(p.data().chunks_exact(4).map(|px| px[3]).collect())
            });
            // Bound the cache: a program printing every code point cannot grow it without limit.
            if self.glyphs.len() >= 4096 {
                self.glyphs.retain(|c, _| c.is_ascii());
            }
            self.glyphs.insert(c, mask);
        }
        self.glyphs.get(&c).and_then(|m| m.as_deref())
    }
    /// Forget what is in the buffer (it is new or was resized): draw everything.
    pub fn invalidate(&mut self) {
        self.shown.clear();
    }
    /// Draw the view into `out` (`width` x `height` pixels, XRGB8888 little
    /// endian: B, G, R, x). Returns the pixel rows `[y0, y1)` that changed,
    /// or `None` when nothing did.
    pub fn draw(&mut self, t: &Term, out: &mut [u8], width: usize, height: usize) -> Option<(usize, usize)> {
        assert_eq!(out.len(), width * height * 4);
        let put = |out: &mut [u8], x: usize, y: usize, rgb: [u8; 3]| {
            if x < width && y < height {
                out[(y * width + x) * 4..][..4].copy_from_slice(&[rgb[2], rgb[1], rgb[0], 255]);
            }
        };
        let mut changed: Option<(usize, usize)> = None;
        if self.shown_size != (width, height) || self.shown.len() != t.rows {
            for px in out.chunks_exact_mut(4) {
                px.copy_from_slice(&[DEFAULT_BG[2], DEFAULT_BG[1], DEFAULT_BG[0], 255]);
            }
            self.shown = vec![(Vec::new(), None); t.rows];
            self.shown_size = (width, height);
            changed = Some((0, height));
        }
        let cursor = t.cursor();
        for row in 0..t.rows {
            let cells: Vec<Cell> = (0..t.cols).map(|col| t.cell(col, row)).collect();
            let key: RowKey = (cells, cursor.filter(|c| c.1 == row).map(|c| c.0));
            if self.shown[row] == key {
                continue;
            }
            let y0 = PAD + row * CH;
            for (col, cell) in key.0.iter().enumerate() {
                let x0 = PAD + col * CW;
                // The cursor is a block: the cell's colours exchanged.
                let (fg, bg) = if key.1 == Some(col) { (cell.bg, cell.fg) } else { (cell.fg, cell.bg) };
                for yy in 0..CH {
                    for xx in 0..CW {
                        put(out, x0 + xx, y0 + yy, bg);
                    }
                }
                if cell.c != ' ' {
                    // A glyph the font lacks is shown as a hollow box, not as another character.
                    let mask: Vec<u8> = match self.glyph(cell.c) {
                        Some(mask) => mask.to_vec(),
                        None => (0..CW * CH)
                            .map(|i| {
                                let (x, y) = (i % CW, i / CW);
                                let edge = (x == 2 || x == CW - 3) && (5..=17).contains(&y) || (y == 5 || y == 17) && (2..=CW - 3).contains(&x);
                                if edge { 255 } else { 0 }
                            })
                            .collect(),
                    };
                    for yy in 0..CH {
                        for xx in 0..CW {
                            // Bold: the same outline again one pixel to the right.
                            let mut alpha = u32::from(mask[yy * CW + xx]);
                            if cell.bold && xx > 0 {
                                alpha = alpha.max(u32::from(mask[yy * CW + xx - 1]));
                            }
                            if alpha != 0 {
                                let mix = |f: u8, b: u8| ((u32::from(f) * alpha + u32::from(b) * (255 - alpha)) / 255) as u8;
                                put(out, x0 + xx, y0 + yy, [mix(fg[0], bg[0]), mix(fg[1], bg[1]), mix(fg[2], bg[2])]);
                            }
                        }
                    }
                }
                if cell.underline {
                    for xx in 0..CW {
                        put(out, x0 + xx, y0 + CH - 3, fg);
                    }
                }
            }
            self.shown[row] = key;
            let (top, bottom) = (y0.min(height), (y0 + CH).min(height));
            changed = Some(match changed {
                Some((a, b)) => (a.min(top), b.max(bottom)),
                None => (top, bottom),
            });
        }
        changed
    }
}
#[cfg(test)]
mod tests {
    use super::*;
    fn pixel(out: &[u8], width: usize, x: usize, y: usize) -> [u8; 3] {
        let p = &out[(y * width + x) * 4..][..4];
        [p[2], p[1], p[0]]
    }
    #[test]
    fn real_font_colours_cursor_and_row_damage() {
        let mut r = Renderer::new().unwrap();
        assert_ne!(r.glyph('a').unwrap().to_vec(), r.glyph('A').unwrap().to_vec());
        for c in 33u8..=126 {
            assert!(r.glyph(c as char).unwrap().iter().any(|b| *b != 0), "glyph {c}");
        }
        let mut t = Term::new(20, 6).unwrap();
        let (w, h) = (20 * CW + 2 * PAD, 6 * CH + 2 * PAD);
        let mut out = vec![0u8; w * h * 4];
        // First draw: everything; the cursor block sits at the top left.
        assert_eq!(r.draw(&t, &mut out, w, h), Some((0, h)));
        assert_eq!(pixel(&out, w, PAD + 3, PAD + 3), crate::term::DEFAULT_FG);
        assert_eq!(pixel(&out, w, PAD + CW + 3, PAD + 3), DEFAULT_BG);
        assert_eq!(r.draw(&t, &mut out, w, h), None, "nothing changed");
        // Red text on a green background on the third row only.
        t.feed(b"\x1b[3;1H\x1b[31;42mMM\x1b[0m");
        assert_eq!(r.draw(&t, &mut out, w, h), Some((PAD, PAD + 3 * CH)), "rows 0 (cursor left) to 2");
        let row = PAD + 2 * CH;
        let cell: Vec<[u8; 3]> = (0..CH).flat_map(|yy| (0..CW).map(move |xx| (xx, yy))).map(|(xx, yy)| pixel(&out, w, PAD + xx, row + yy)).collect();
        assert!(cell.contains(&crate::term::palette(2)), "background");
        assert!(cell.contains(&crate::term::palette(1)), "glyph ink");
        assert_eq!(pixel(&out, w, PAD + 5 * CW + 3, row + 3), DEFAULT_BG, "beyond the text");
        // A character the font does not have is a box, and never panics.
        t.feed("\u{4e2d}".as_bytes());
        assert!(r.draw(&t, &mut out, w, h).is_some());
        // A new buffer size redraws everything.
        let mut bigger = vec![0u8; (w + 40) * (h + 40) * 4];
        assert_eq!(r.draw(&t, &mut bigger, w + 40, h + 40), Some((0, h + 40)));
    }
}
