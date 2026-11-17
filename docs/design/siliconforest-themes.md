# SiliconForest themes: Clearing, Canopy, Wafer, Die Shot

Four showcase themes for the SiliconForest desk (agent THEME, 17 November 2026). This file is the
creative brief, the reference for what each theme controls, the guide to adding a theme, and the
shot list for the screen recording. What is installed on the M1, with hashes and the way back, is
in `docs/roadmap/issues/SF-Z-desktop-night.md`, section "THEME".

## 1. The brand idea

SiliconForest is two materials that grow in layers: a forest (canopy, understory, moss, mist) and a
chip (metal layers, traces, vias, pads, the wafer under them). The mark already says it: a conifer
whose three tiers are circuit traces with vias at the tips, standing on a row of chip pads. The
themes take four views of that one object, from far to near:

| View | Theme | id | Mood |
|---|---|---|---|
| the forest at dawn, from a ridge | **Clearing** | `sf-clearing` | light, calm, the one to live in |
| the forest at night, from under the trees | **Canopy** | `sf-canopy` | dark, deep green, copper light |
| the wafer on the bench, in a clean room | **Wafer** | `sf-wafer` | light, cool, exact, a trace of iridescence |
| the die under the microscope | **Die Shot** | `sf-dieshot` | dark, gold on black, the demo piece that takes liberties |

Rules for the family:
- **Mac muscle memory.** Close, minimise, zoom sit at the top left in that order in all four; the
  title is where a Mac puts it; nothing a hand has learnt moves. The liberties are in the *shape* of
  the controls (a disc, a via, a pad, a bond pad), never in their place or order.
- **The apps are the content.** Terminal, Activity Monitor and Firefox draw their own insides (and
  their own menus). The chrome is quiet enough that a light app in a dark theme, or the reverse, looks
  intended: title bars are near-neutral with a tint of the theme, one accent per theme, no gradients
  that compete with a toolbar underneath.
- **One accent each**, used for focus, selection in the desk's own menus, the dock's LED ink and
  the running-application mark: moss green, copper, thin-film indigo, bond-wire gold.
- **It costs nothing.** Every effect is either drawn once and cached (title bands, dock tiles,
  wallpaper) or a few hundred thousand shaded pixels a frame (the shadow's ring). Nothing blurs the
  screen; nothing animates while the desk is still.

## 2. The four concepts

Sizes are in points at 100 %; the owner's desk runs at 150 % (state `scale` = 180), where 1 pt is
1.5 px. Contrast ratios are WCAG 2 relative-luminance ratios, computed from the values below
(`integrations/puredarwin/desktop/themes/contrast.py` prints the table).

### Clearing (`sf-clearing`, light)
*The forest at dawn: mist between ridgelines, the first light on moss.* The native theme done
properly: what the default would be if it had been designed for a Mac user.

| Token | Value | Against | Ratio |
|---|---|---|---|
| title bar (active) | `#e9ede5` | | |
| title bar (inactive) | `#f1f3ee` | | |
| panel / menus | `#fbfcf9` | | |
| text | `#1f2a24` | title bar | 12.5 |
| muted text (inactive title) | `#56625a` | inactive title | 5.7 |
| hairline | `#b9c3b8` | | |
| accent: moss | `#2c7a54` | panel | 5.1 (white on it: 5.2) |
| selection | `#cfe6d8` | text on it | 11.3 |
| close / minimise / zoom | `#cf4a3a` / `#a87004` / `#2f8a52` | title bar | 3.8 / 3.6 / 3.6 |
| controls, window inactive | `#7c877d` (ring) | inactive title | 3.4 |

- Type: Inter Medium 13 pt, centred over the window (as macOS), inactive in muted.
- Controls: 12 pt discs, 8 pt apart, 8 pt from the left edge, optically centred on the title's
  x-height; a 0.5 pt darker rim of the same hue. Hovering any of the three shows all three glyphs
  (x, minus, plus) in the hue at 55 % luminance; pressed darkens the disc 12 %. An inactive window's
  controls are hollow rings in neutral, and colour again when the pointer is over them.
- Frame: title bar 28 pt, radius 10 pt on all four corners, a 1 px hairline, a 1 px lighter top edge
  inside the title bar. Shadow: 0 / 10 pt offset, 40 pt blur, `#16241b` at 40 % for the focused
  window, 24 % unfocused (the first draft's 30 % did not lift a white window off the mist).
- Motion: a window fades in over 180 ms while rising 8 pt, ease-out cubic. Theme changes cross-fade
  over 220 ms.
- Dock and clock: paper tiles (`#f1f3ee`) with moss LED ink on dark-green glass; digital clock.
- Wallpaper `sf-clearing`: six ridgelines of conifers receding into mist, drawn from the mark's
  own tier shape, pale sage to warm white at the horizon, the sun as a soft via low on the right.
  The lower third (where the nearest, darkest ridge is) stays under the windows' feet; the upper two
  thirds are almost flat mist.

### Canopy (`sf-canopy`, dark)
*The forest at night, from under the trees: black conifers either side of a gap of sky, copper
light on the horizon.*

| Token | Value | Against | Ratio |
|---|---|---|---|
| title bar (active) | `#17221d` | | |
| title bar (inactive) | `#121a16` | | |
| panel / menus | `#111a16` | | |
| text | `#e6ede4` | title bar | 13.7 |
| muted text | `#9aa89d` | inactive title | 7.1 |
| hairline | `#2e3d35` | | |
| accent: copper | `#dd8f55` | panel | 6.9 (dark on it: 7.2) |
| selection | `#2b3d33` | text on it | 9.7 |
| close / minimise / zoom | `#ec7c60` / `#e2b65c` / `#82c98f` | title bar | 5.9 / 8.6 / 8.3 |
| controls, window inactive | `#5d6e64` | inactive title | 3.3 |

- Type: Inter SemiBold 13 pt, centred.
- Controls: **vias**. 12 pt rings, 1.75 pt wall, in the three hues, the hole showing the title bar
  (a plated through-hole). Hover fills the ring and shows the glyph in the title bar's colour;
  pressed is the filled via 15 % lighter. Inactive: neutral rings.
- Frame: title bar 30 pt, radius 12 pt, hairline `#2e3d35`, and a 1 px copper line along the top of
  the focused window only (the horizon's light, caught on an edge). Shadow: 0 / 14 pt, 48 pt blur,
  `#010403` at 70 % focused, 45 % unfocused: on a dark desk the shadow has to be nearly black to read.
- Motion: 200 ms fade, 10 pt rise, ease-out quint (slower, heavier). Theme cross-fade 260 ms.
- Dock and clock: dark bark tiles (`#18231e`), copper LED ink; digital clock.
- Wallpaper `sf-canopy`: tall conifers standing at both sides of the picture, the tallest at the
  edges (the busy part is at the rim, where the dock and the screen's edge are), a deep blue-green
  night sky between them with stars, six of them joined by faint traces into the mark's
  constellation, low ridges against a copper glow on the horizon. (A first version looked straight
  up at crowns leaning in from all four edges; the crowns read as fronds, not conifers, and it was
  dropped.)

### Wafer (`sf-wafer`, light)
*The clean room: a polished wafer under white light, the colours of a thin film where the light
catches it.*

| Token | Value | Against | Ratio |
|---|---|---|---|
| title bar (active) | `#f1f3f7` | | |
| title bar (inactive) | `#f6f7f9` | | |
| panel / menus | `#ffffff` | | |
| text | `#1c2330` | title bar | 14.2 |
| muted text | `#566074` | inactive title | 5.9 |
| hairline | `#c3cad6` | | |
| accent: thin-film indigo | `#4a5bd0` | panel | 5.7 (white on it: 5.7) |
| selection | `#dde2fa` | text on it | 12.3 |
| close / minimise / zoom | `#d2475a` / `#a87400` / `#1f8a5b` | title bar | 4.0 / 3.7 / 3.9 |
| controls at rest / inactive | `#737c8c` | inactive title | 3.9 |

- Type: IBM Plex Sans 13 pt (bundled, OFL), centred; the engineering-drawing voice.
- Controls: **pads**. 11 pt squares with a 3 pt radius, 8 pt apart. At rest they are graphite
  (`#737c8c`): a clean room has no coloured lights. When the pointer enters the group all three take
  their hues and glyphs. Pressed darkens 12 %. Inactive: hollow.
- Frame: title bar 28 pt, radius 7 pt (crisper than Clearing), hairline `#c3cad6`. The focused
  window carries a 2 pt **interference line** along its top edge: teal `#19a3a8` to indigo `#4a5bd0`
  to violet `#9a5bd6`, left to right; the only gradient in the family. Shadow: 0 / 7 pt, 28 pt
  blur, `#141d38` at 34 % focused, 20 % unfocused: tighter and cooler, light from straight above.
- Motion: 140 ms fade, 4 pt rise, ease-out cubic: quick and exact. Theme cross-fade 200 ms.
- Dock and clock: white enamel tiles, indigo LED ink on blue-black glass; the analog clock suits it
  (the owner's stored clock style is not changed).
- Wallpaper `sf-wafer`: the edge of a wafer entering from the lower right: a large arc, the die
  grid inside it in very low contrast, a restrained thin-film sheen along the arc (teal to violet),
  the flat and a notch; the upper left two thirds are a near-white field with the faintest falloff.

### Die Shot (`sf-dieshot`, dark)
*The die under the microscope: metal layers, bond pads, gold wire on black.* The liberty theme.

| Token | Value | Against | Ratio |
|---|---|---|---|
| title bar (active) | `#12161c` | | |
| title bar (inactive) | `#0e1116` | | |
| panel / menus | `#0e1116` | | |
| text | `#e9eef2` | title bar | 15.5 |
| muted text | `#93a0ad` | inactive title | 7.1 |
| hairline | `#2c3540` | | |
| accent: bond-wire gold | `#e7c15a` | panel | 11.0 (dark on it: 10.9) |
| selection | `#26303a` | text on it | 11.5 |
| close / minimise / zoom | `#ff7a6b` / `#e7c15a` / `#58d6a0` | title bar | 7.1 / 10.5 / 10.0 |
| controls, window inactive | `#5b6876` | inactive title | 3.3 |

- Type: Source Code Pro Medium 12 pt, **left**, directly after the controls, like a net label on a
  schematic. This is the one theme where the title is not centred.
- Controls: **bond pads**. 12 pt squares, 1.5 pt radius, outlined 1.5 pt in the three hues with a
  solid 4 pt core (pad and probe mark). Hover fills the pad; the glyph is cut in the title bar's
  colour. Pressed: filled, 15 % lighter. Same place, same order as a Mac.
- Frame: title bar 30 pt, radius 3 pt (a die is sawn, not moulded), a 1 px gold border on the
  focused window, `#2c3540` otherwise, and a **trace**: a 1 px gold line from the end of the title to
  8 pt before the right edge, ending in a via (5 pt ring). It is the one decoration in the family,
  and it is why the title is at the left. Shadow: 0 / 10 pt, 32 pt blur, black at 80 % focused, 50 %
  unfocused.
- Motion: 120 ms fade, no rise, ease-out expo: a window is *there*, like a probe touching down.
  Theme cross-fade 180 ms.
- Dock and clock: black tiles with a gold hairline, gold LED ink; digital clock.
- Wallpaper `sf-dieshot`: an invented floorplan (no real chip): cache arrays, cores, an I/O ring of
  bond pads round the edge, routing channels, in blue-black metal with thin-film tints; the mark's
  conifer is laid out in the top metal at the centre like a mask-shop signature, at very low
  contrast. Detail is dense at the rim and in the blocks' borders; the blocks' interiors, where
  windows sit, are fine regular texture that reads as a flat tone from a metre away.

### How they differ from each other and from what was there

| | Clearing | Canopy | Wafer | Die Shot | washi / obsidian / relay (existing) |
|---|---|---|---|---|---|
| appearance | light | dark | light | dark | light / dark / dark |
| controls | discs | vias (rings) | pads (rounded squares), grey at rest | bond pads (outlined squares) | line glyphs at the right |
| controls' place | left | left | left | left | right |
| title | centred, Inter Medium | centred, Inter SemiBold | centred, Plex Sans | left, Source Code Pro | left |
| radius | 10 | 12 | 7 | 3 | 0 / 3 / 9 (top corners only) |
| focus signal | shadow depth, coloured controls | copper top line | interference line | gold border + trace | accent square / title colour |
| shadow | soft, green-black 40 % | deep, black 70 % | tight, cool 34 % | dense black 80 % | none drawn on this desk |
| motion | 180 ms, rise 8 | 200 ms, rise 10 | 140 ms, rise 4 | 120 ms, no rise | none |
| accent | moss `#2c7a54` | copper `#dd8f55` | indigo `#4a5bd0` | gold `#e7c15a` | red / amber / periwinkle |

## 3. Wallpapers

All four are original and drawn by code in this repository
(`integrations/puredarwin/desktop/themes/wallpapers/make_wallpapers.py`, numpy + Pillow): no
photograph, no third-party image, no font. Each is rendered at 2560x1600 (the panel) and can be
rendered at 5120x3200 (`--scale 2`). Gradients are computed in floating point and dithered with
triangular noise of one 8-bit step before quantising, so no band is visible on the panel; the files
are JPEG at quality 98, 4:4:4 (measured: at 93 the codec flattens the dither into runs of up to 111
equal pixels on Wafer's bench, at 98 the longest run is 23; a PNG of the same picture is 2.8 MB
against 0.7 MB). The busy part of every picture is kept away from the middle of the
screen, and each was checked with windows on top.

## 4. What a theme controls on this desk

A theme is a `wm_theme::Theme` value built in code (`crates/wm-theme/src/`; the compositor
`chonkstep-darwin-desktop` and the dock `chonkstep-dock` both compile it in, so a new theme needs
both rebuilt). The SiliconForest themes are in `forest.rs` and are drawn by the modern recipe
(`styles/modern.rs`) with the window tokens of `modern::WindowChrome`:

| What | Where it comes from | Who draws it |
|---|---|---|
| title bar height, corner radius, border, padding, control box and gap | `Chrome.frame` (`FrameMetrics`) | recipe, cached per title/focus/state |
| title bar and menu colours, text, hairline, accent, selection | `Chrome` tokens (`surface`, `raised`, `panel`, `text`, `muted`, `line`, `accent`, ...) | recipe |
| controls: shape, side, order, hues, idle colour, grey-at-rest | `WindowChrome.controls`, `.left`, `.neutral_at_rest`, `.close`/`.minimise`/`.zoom`/`.idle`; order: `Theme.titlebar.buttons` | `styles/forest.rs` |
| title: face, weight (Normal, Medium, SemiBold, Bold), size, centred or left | `Theme.titlebar.font`, `WindowChrome.centered` | `styles/forest.rs`, `paint::draw_title_text` (capitals' height centred: every title on the same line) |
| top edge: highlight, accent rim, interference line | `WindowChrome.top_line` | `styles/forest.rs` |
| the trace and its via | `WindowChrome.trace` | `styles/forest.rs` |
| a hairline of one device pixel at every scale | `WindowChrome.hairline` | `Chrome::scaled` |
| shadow: offset, blur, colour, alpha with and without focus | `Chrome.shadow`, `WindowChrome.shadow_unfocused` | compositor (`wm-darwin/src/effects.rs`), only for `WindowChrome.effects` |
| rounded lower corners with the border's ring | `Chrome.frame.radius`, `WindowChrome.effects` | compositor (`effects.rs`) |
| a window's appearing: duration, rise, curve | `WindowChrome.open_ms`, `.open_rise`, `.open_curve` | compositor (`motion.rs`), policy `motion` |
| theme change: cross-fade duration | `WindowChrome.switch_ms` | compositor (`motion.rs`, `glrender.rs` `fade_begin`) |
| the desk's menus, notices, switcher, launcher, minimised tiles | `Theme.menu`, `Theme.tile`, the `Chrome` tokens | shell (as for every theme) |
| dock tiles, LED ink and glass, launcher mark, clock faces | `Theme.tile`, `Theme.terminal.cursor` (the accent), `Chrome.frame.radius` | dock (as for every theme) |
| wallpaper | `Theme.wallpaper` = the theme's id; the picture is `<id>.jpg` in the host's backgrounds (`chonk-shell` `Wallpaper::Forest`), else the theme's `background` colour | shell |
| appearance | each is drawn for one (`default_theme::standalone_appearance`); the other appearance takes the sibling's palette | |

Not themed, and why:
- **The pointer.** One SiliconForest set for every theme (DESK7): a pointer that changes with the
  theme is a pointer the hand has to find again.
- **Applications' own drawing.** Terminal, Activity Monitor and Firefox draw their content, toolbars
  and menus themselves (AppKit); the accent does not reach their selection colour. That needs AppKit
  to read the desk's accent: asked of nobody yet, noted in SF-Z "THEME".
- **A window's leaving.** Closing and minimising are not animated: the picture of a window that has
  gone belongs to its application and is no longer there to fade.
- **The existing themes.** washi, obsidian, relay and the others are drawn exactly as before:
  none has `effects`, a control shape or motion (their tokens are the defaults), so the owner's desk
  does not change until he picks a SiliconForest theme.

## 5. Adding a theme

1. In `crates/wm-theme/src/forest.rs` add a palette (twelve tokens, four control colours, the
   shadow's colour and two alphas) and a row in `CHOICES`, and give it its metrics, type, controls
   and motion in the `match id` tables of `theme()`. A light theme and a dark one that share shapes
   can be each other's companion, as Clearing/Canopy are.
2. Register it in `default_theme.rs`: `CHOICES` (both lists, and their lengths), `all_themes()`,
   and the light list of `native_renditions_are_what_each_theme_originally_shipped_as` if it is light.
3. `cargo test -p wm-theme`: `forest::tests::text_and_controls_keep_their_contrast` holds the
   palette to 4.5:1 for text and 3:1 for controls; the registry tests hold the lists together.
4. Look at it: `cargo run -p wm-theme --example forest_preview -- <dir> 1.5` (the chrome in every
   state), `DOCK_PREVIEW_DIR=<dir> DOCK_PREVIEW_THEMES=<id> cargo test -p chonk-dock column_preview
   -- --ignored` (the dock's tiles), and `tests/theme_gallery.rs` (the whole desk off screen with
   pictures for window contents).
5. A wallpaper: a function in `integrations/puredarwin/desktop/themes/wallpapers/make_wallpapers.py`
   and a name in its table; the file `<id>.jpg` goes to `/usr/share/backgrounds/siliconforest`
   (`desk_m1.sh wallpapers`). Without it the theme shows its `background` colour.
6. Build the compositor (both builds) and the dock, install, relaunch
   (`integrations/puredarwin/terminal-app/compositor/README.md`).

A new *kind* of control or top edge is a variant of `modern::Controls` / `modern::TopLine` and an
arm in `styles/forest.rs`; nothing else knows theme ids.
