// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Apple SPI trackpad multitouch report decoder and pointer synthesis.
 *
 * Report layout and validity rules ported from drivers/hid/hid-magicmouse.c
 * (magicmouse_raw_event_spi, magicmouse_raw_event_mtp, struct tp_finger,
 * struct tp_header, struct tp_mouse_report) of Omacom linux-aurora
 * integration/aurora 6a08a08e:
 *
 *   Apple "Magic" Wireless Mouse driver
 *   Copyright (c) 2010 Michael Poole <mdpoole@troilus.org>
 *   Copyright (c) 2010 Chase Douglas <chase.douglas@canonical.com>
 *
 * See m1tp_mt.h for what is ported and what is new.
 */
#include "m1tp_mt.h"
#include <string.h>

/* feature_mt_trackpad2_usb in magicmouse_enable_multitouch() */
const uint8_t m1tp_mt_enable_payload[2] = { 0x02, 0x01 };

const uint8_t m1tp_mt_pointer_descriptor[M1TP_MT_POINTER_DESCRIPTOR_BYTES] = {
    0x05, 0x01, 0x09, 0x02, 0xa1, 0x01, 0x09, 0x01, 0xa1, 0x00, /* mouse, pointer */
    0x85, 0x02,                                                 /* report ID 2 */
    0x05, 0x09, 0x19, 0x01, 0x29, 0x03, 0x15, 0x00, 0x25, 0x01, /* buttons 1-3 */
    0x95, 0x03, 0x75, 0x01, 0x81, 0x02,
    0x95, 0x01, 0x75, 0x05, 0x81, 0x01,                         /* 5 bits padding */
    0x05, 0x01, 0x09, 0x30, 0x09, 0x31, 0x09, 0x38,             /* X, Y, wheel */
    0x15, 0x81, 0x25, 0x7f, 0x75, 0x08, 0x95, 0x03, 0x81, 0x06, /* -127..127, relative */
    0xc0, 0xc0
};

/* struct tp_header: num_fingers and buttons follow 22 unknown bytes. */
#define TP_HDR_NUM_FINGERS 22u
#define TP_HDR_BUTTONS 23u
/* struct tp_finger, little-endian 16-bit fields */
#define TP_F_ABS_X 4u
#define TP_F_ABS_Y 6u
#define TP_F_REL_X 8u
#define TP_F_REL_Y 10u
#define TP_F_TOUCH_MAJOR 18u
#define TP_F_TOUCH_MINOR 20u
#define TP_F_PRESSURE 26u

/* le16_to_int() */
static int16_t le16s(const uint8_t *p)
{
    return (int16_t)(uint16_t)(p[0] | (uint16_t)((uint16_t)p[1] << 8));
}
static void put16(uint8_t *p, int16_t v)
{
    p[0] = (uint8_t)((uint16_t)v & 0xffu);
    p[1] = (uint8_t)((uint16_t)v >> 8);
}
static int32_t iabs(int32_t v) { return v < 0 ? -v : v; }
static int8_t clamp8(int32_t v) { return (int8_t)(v > 127 ? 127 : v < -127 ? -127 : v); }

void m1tp_mt_default_config(struct m1tp_mt_config *c)
{
    if (!c) return;
    /* Contact units are roughly 95 per mm (J314_TP_RES_X: 12400 units over
     * 130 mm). None of these numbers come from the Linux driver. */
    /* 4: about 24 counts per mm. The basic mode's own reports peak around 90
     * counts each in a fast stroke (boot pr02), which is this order of
     * magnitude; the exact relation has not been measured. */
    c->units_per_count = 4;
    c->scroll_units_per_step = 300;  /* one wheel step per ~3 mm */
    c->click_span_x = 3800;          /* 40 mm */
    c->click_span_y = 2850;          /* 30 mm */
    c->slop_units = 150;             /* 1.5 mm */
    c->jump_units = 1500;            /* 15 mm in one frame */
    c->natural_scroll = 0;
    c->use_header_motion = 0;
}

void m1tp_mt_init(struct m1tp_mt_state *s, const struct m1tp_mt_config *config)
{
    struct m1tp_mt_config d;
    if (!s) return;
    memset(s, 0, sizeof(*s));
    m1tp_mt_default_config(&d);
    if (config) {
        if (config->units_per_count) d.units_per_count = config->units_per_count;
        if (config->scroll_units_per_step) d.scroll_units_per_step = config->scroll_units_per_step;
        if (config->click_span_x) d.click_span_x = config->click_span_x;
        if (config->click_span_y) d.click_span_y = config->click_span_y;
        if (config->slop_units) d.slop_units = config->slop_units;
        if (config->jump_units) d.jump_units = config->jump_units;
        d.natural_scroll = config->natural_scroll ? 1 : 0;
        d.use_header_motion = config->use_header_motion ? 1 : 0;
    }
    /* keep the arithmetic below far from overflow whatever a boot-arg says */
    if (d.units_per_count > 1000) d.units_per_count = 1000;
    if (d.scroll_units_per_step > 100000) d.scroll_units_per_step = 100000;
    s->cfg = d;
}

int m1tp_mt_parse(const uint8_t *data, size_t size, struct m1tp_mt_frame *f)
{
    const size_t prefix = M1TP_MT_MOUSE_BYTES + M1TP_MT_HEADER_BYTES;
    const uint8_t *tp;
    size_t npoints, i;
    unsigned n = 0;

    if (!f) return M1TP_MT_KIND_INVALID;
    memset(f, 0, sizeof(*f));
    if (!data || !size) return M1TP_MT_KIND_INVALID;
    /* magicmouse_raw_event_spi(): the reset report restarts the mode request */
    if (data[0] == M1TP_MT_RESET_REPORT_ID) {
        f->kind = M1TP_MT_KIND_RESET;
        return f->kind;
    }
    if (data[0] != M1TP_MT_REPORT_ID || size < M1TP_MT_MOUSE_BYTES)
        return M1TP_MT_KIND_INVALID;
    f->mouse_buttons = data[1];
    /* -128 is outside the descriptor's -127..127 and has been seen in idle
     * basic-mode reports: not motion. */
    f->mouse_dx = data[2] == 0x80 ? 0 : (int8_t)data[2];
    f->mouse_dy = data[3] == 0x80 ? 0 : (int8_t)data[3];
    /* The basic mode sends the 8-byte pointer report alone. */
    if (size == M1TP_MT_MOUSE_BYTES) {
        f->kind = M1TP_MT_KIND_BASIC;
        return f->kind;
    }
    /* magicmouse_raw_event_mtp(): "Expect 46 bytes of prefix, and N * 30
     * bytes of touch data." */
    if (size < prefix || ((size - prefix) % M1TP_MT_FINGER_BYTES) != 0)
        return M1TP_MT_KIND_INVALID;
    tp = data + M1TP_MT_MOUSE_BYTES;
    npoints = (size - prefix) / M1TP_MT_FINGER_BYTES;
    if (npoints < tp[TP_HDR_NUM_FINGERS] || npoints > M1TP_MT_MAX_CONTACTS)
        return M1TP_MT_KIND_INVALID;
    f->reported = tp[TP_HDR_NUM_FINGERS];
    f->tp_buttons = tp[TP_HDR_BUTTONS];
    for (i = 0; i < f->reported; i++) {
        const uint8_t *r = data + prefix + i * M1TP_MT_FINGER_BYTES;
        struct m1tp_mt_contact *c = &f->c[n];
        /* a record with no touch area is not a contact */
        if (le16s(r + TP_F_TOUCH_MAJOR) == 0)
            continue;
        c->x = le16s(r + TP_F_ABS_X);
        c->y = le16s(r + TP_F_ABS_Y);
        c->rel_x = le16s(r + TP_F_REL_X);
        c->rel_y = le16s(r + TP_F_REL_Y);
        c->touch_major = le16s(r + TP_F_TOUCH_MAJOR);
        c->touch_minor = le16s(r + TP_F_TOUCH_MINOR);
        c->pressure = le16s(r + TP_F_PRESSURE);
        n++;
    }
    f->contacts = (uint8_t)n;
    f->kind = M1TP_MT_KIND_TOUCH;
    return f->kind;
}

void m1tp_mt_encode(const struct m1tp_mt_out *o, uint8_t report[M1TP_MT_POINTER_REPORT_BYTES])
{
    report[0] = M1TP_MT_REPORT_ID;
    report[1] = (uint8_t)(o->buttons & 7u);
    report[2] = (uint8_t)o->dx;
    report[3] = (uint8_t)o->dy;
    report[4] = (uint8_t)o->wheel;
}

static void forget_touches(struct m1tp_mt_state *s)
{
    s->have_prev = 0; s->prev_count = 0;
    s->gesture = M1TP_MT_G_UNDECIDED;
    s->travel_x[0] = s->travel_x[1] = s->travel_y[0] = s->travel_y[1] = 0;
    s->rem_x = s->rem_y = s->rem_scroll = 0;
}

static int finish(struct m1tp_mt_state *s, uint8_t buttons, int32_t dx, int32_t dy,
                  int32_t wheel, struct m1tp_mt_out *out)
{
    out->buttons = buttons; out->dx = clamp8(dx); out->dy = clamp8(dy); out->wheel = clamp8(wheel);
    if (buttons == s->emitted_buttons && !out->dx && !out->dy && !out->wheel)
        return 0;
    s->emitted_buttons = buttons;
    return 1;
}

/* Pointer counts from contact travel, carrying the remainder. */
static void scaled_motion(struct m1tp_mt_state *s, int32_t ux, int32_t uy, int32_t *dx, int32_t *dy)
{
    int32_t k = (int32_t)s->cfg.units_per_count;
    s->rem_x += ux;
    s->rem_y -= uy;                 /* the pad's y grows away from the user */
    *dx = s->rem_x / k; s->rem_x -= *dx * k;
    *dy = s->rem_y / k; s->rem_y -= *dy * k;
}

int m1tp_mt_step(struct m1tp_mt_state *s, const struct m1tp_mt_frame *f, struct m1tp_mt_out *out)
{
    int32_t sum_x = 0, sum_y = 0, move_x = 0, move_y = 0, dx = 0, dy = 0, wheel = 0;
    int32_t d_x[2] = { 0, 0 }, d_y[2] = { 0, 0 };
    int32_t jump;
    unsigned n, i;
    int stable, point = 0;

    if (!s || !f || !out) return 0;
    memset(out, 0, sizeof(*out));
    if (f->kind == M1TP_MT_KIND_BASIC) {
        /* The controller is (still) in its basic mode: its report is the result. */
        forget_touches(s);
        s->button_down = 0; s->button_mask = 0;
        return finish(s, (uint8_t)(f->mouse_buttons & 7u), f->mouse_dx, f->mouse_dy, 0, out);
    }
    if (f->kind != M1TP_MT_KIND_TOUCH) return 0;

    n = f->contacts;
    jump = (int32_t)s->cfg.jump_units;
    for (i = 0; i < n; i++) { sum_x += f->c[i].x; sum_y += f->c[i].y; }
    stable = s->have_prev && s->prev_count == n && n > 0;
    if (!stable) {
        s->gesture = M1TP_MT_G_UNDECIDED;
        s->travel_x[0] = s->travel_x[1] = s->travel_y[0] = s->travel_y[1] = 0;
        s->rem_scroll = 0;
    } else {
        move_x = sum_x - s->prev_sum_x; move_y = sum_y - s->prev_sum_y;
        if (iabs(move_x) > jump * (int32_t)n || iabs(move_y) > jump * (int32_t)n)
            move_x = move_y = 0;
    }

    /* The physical button: tp_header.buttons bit 0, as the Linux driver reads
     * it. The number of contacts at the press decides which button it is for
     * the whole press. */
    if ((f->tp_buttons & 1u) && !s->button_down) {
        s->button_down = 1; s->button_mask = 1;
        if (n == 2 &&
            (uint32_t)iabs((int32_t)f->c[0].x - f->c[1].x) <= s->cfg.click_span_x &&
            (uint32_t)iabs((int32_t)f->c[0].y - f->c[1].y) <= s->cfg.click_span_y) {
            s->button_mask = 2;
            s->events |= M1TP_MT_EV_RIGHT_CLICK;
        }
    } else if (!(f->tp_buttons & 1u) && s->button_down) {
        s->button_down = 0; s->button_mask = 0;
        /* travel during the press does not start a gesture afterwards */
        s->gesture = M1TP_MT_G_UNDECIDED;
        s->travel_x[0] = s->travel_x[1] = s->travel_y[0] = s->travel_y[1] = 0;
        s->rem_scroll = 0;
    }

    /* The Linux driver never reads the pointer header of a touch report, so
     * what the controller puts there is unknown until seen: note it, and use
     * it only when asked to. */
    if (f->mouse_dx || f->mouse_dy) {
        s->events |= M1TP_MT_EV_HEADER_MOTION;
        if (s->cfg.use_header_motion) s->header_motion = 1;
    }

    if (n == 2) {
        int32_t x0 = f->c[0].x, y0 = f->c[0].y, x1 = f->c[1].x, y1 = f->c[1].y;
        if (stable) {
            /* The controller may reorder its records: pair each contact with
             * the nearer previous one. */
            int32_t keep = iabs(x0 - s->prev_x[0]) + iabs(y0 - s->prev_y[0]) +
                           iabs(x1 - s->prev_x[1]) + iabs(y1 - s->prev_y[1]);
            int32_t swap = iabs(x0 - s->prev_x[1]) + iabs(y0 - s->prev_y[1]) +
                           iabs(x1 - s->prev_x[0]) + iabs(y1 - s->prev_y[0]);
            if (swap < keep) {
                int32_t t;
                t = s->prev_x[0]; s->prev_x[0] = s->prev_x[1]; s->prev_x[1] = t;
                t = s->prev_y[0]; s->prev_y[0] = s->prev_y[1]; s->prev_y[1] = t;
                t = s->travel_x[0]; s->travel_x[0] = s->travel_x[1]; s->travel_x[1] = t;
                t = s->travel_y[0]; s->travel_y[0] = s->travel_y[1]; s->travel_y[1] = t;
            }
            d_x[0] = x0 - s->prev_x[0]; d_y[0] = y0 - s->prev_y[0];
            d_x[1] = x1 - s->prev_x[1]; d_y[1] = y1 - s->prev_y[1];
            for (i = 0; i < 2; i++) {
                if (iabs(d_x[i]) > jump || iabs(d_y[i]) > jump) { d_x[i] = 0; d_y[i] = 0; }
                s->travel_x[i] += d_x[i]; s->travel_y[i] += d_y[i];
            }
        }
        s->prev_x[0] = x0; s->prev_y[0] = y0; s->prev_x[1] = x1; s->prev_y[1] = y1;
    }

    if (s->button_down) {
        /* A held button drags: the pointer follows whatever moves. */
        point = n >= 1;
    } else if (n == 1) {
        point = 1;
    } else if (n == 2 && stable) {
        if (s->gesture == M1TP_MT_G_UNDECIDED) {
            int32_t m0 = iabs(s->travel_x[0]) > iabs(s->travel_y[0]) ? iabs(s->travel_x[0]) : iabs(s->travel_y[0]);
            int32_t m1 = iabs(s->travel_x[1]) > iabs(s->travel_y[1]) ? iabs(s->travel_x[1]) : iabs(s->travel_y[1]);
            int32_t big = m0 > m1 ? m0 : m1, small = m0 > m1 ? m1 : m0;
            if (big >= (int32_t)s->cfg.slop_units) {
                int32_t a = s->travel_y[0], b = s->travel_y[1];
                int32_t ya = iabs(a), yb = iabs(b);
                int lead = m0 > m1 ? 0 : 1;
                if (((a > 0 && b > 0) || (a < 0 && b < 0)) &&
                    (ya < yb ? ya : yb) * 3 >= (ya > yb ? ya : yb) &&
                    iabs(s->travel_y[lead]) >= iabs(s->travel_x[lead])) {
                    /* both fingers travel the same way, mostly vertically */
                    s->gesture = M1TP_MT_G_SCROLL;
                    s->rem_scroll = a + b - (d_y[0] + d_y[1]);
                } else if (small * 3 < big) {
                    /* one finger rests, the other points */
                    s->gesture = M1TP_MT_G_POINT;
                } else {
                    /* sideways swipe, pinch: nothing the record format can carry */
                    s->gesture = M1TP_MT_G_NONE;
                }
            }
        }
        if (s->gesture == M1TP_MT_G_SCROLL) {
            /* rem_scroll is the sum over both fingers: twice the travel */
            int32_t step = (int32_t)s->cfg.scroll_units_per_step * 2;
            s->rem_scroll += d_y[0] + d_y[1];
            wheel = s->rem_scroll / step;
            s->rem_scroll -= wheel * step;
            if (wheel) s->events |= M1TP_MT_EV_SCROLL;
            if (s->cfg.natural_scroll) wheel = -wheel;
        } else if (s->gesture == M1TP_MT_G_POINT) {
            point = 1;
        }
    }

    if (point) {
        if (s->header_motion) { dx = f->mouse_dx; dy = f->mouse_dy; }
        else scaled_motion(s, move_x, move_y, &dx, &dy);
    }

    s->have_prev = 1; s->prev_count = (uint8_t)n;
    s->prev_sum_x = sum_x; s->prev_sum_y = sum_y;
    return finish(s, s->button_mask, dx, dy, wheel, out);
}

size_t m1tp_mt_build(uint8_t *report, size_t capacity, uint8_t mouse_buttons,
    int8_t mouse_dx, int8_t mouse_dy, uint8_t tp_buttons, unsigned contacts,
    const int16_t (*xy)[2], const int16_t *major)
{
    const size_t prefix = M1TP_MT_MOUSE_BYTES + M1TP_MT_HEADER_BYTES;
    size_t size = prefix + (size_t)contacts * M1TP_MT_FINGER_BYTES;
    unsigned i;

    if (!report || contacts > M1TP_MT_MAX_CONTACTS || size > capacity || (contacts && !xy))
        return 0;
    memset(report, 0, size);
    report[0] = M1TP_MT_REPORT_ID;
    report[1] = mouse_buttons;
    report[2] = (uint8_t)mouse_dx;
    report[3] = (uint8_t)mouse_dy;
    report[M1TP_MT_MOUSE_BYTES + TP_HDR_NUM_FINGERS] = (uint8_t)contacts;
    report[M1TP_MT_MOUSE_BYTES + TP_HDR_BUTTONS] = tp_buttons;
    for (i = 0; i < contacts; i++) {
        uint8_t *r = report + prefix + (size_t)i * M1TP_MT_FINGER_BYTES;
        int16_t m = major ? major[i] : 400;
        put16(r + TP_F_ABS_X, xy[i][0]);
        put16(r + TP_F_ABS_Y, xy[i][1]);
        put16(r + TP_F_TOUCH_MAJOR, m);
        put16(r + TP_F_TOUCH_MINOR, m);
        put16(r + TP_F_PRESSURE, 60);
    }
    return size;
}
