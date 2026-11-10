// SPDX-License-Identifier: GPL-2.0-or-later
/* Host tests of the multitouch decoder. Fixtures are constructed from the
 * report layout in the pinned hid-magicmouse.c; none is a device recording. */
#include "m1tp_mt.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static uint8_t buf[M1TP_MT_MAX_REPORT_BYTES + 64];
static struct m1tp_mt_state st;
static struct m1tp_mt_frame fr;
static struct m1tp_mt_out out;
static unsigned sunk;
static struct m1tp_mt_out last;

static void sink(void *context, const struct m1tp_mt_out *o)
{ assert(context == &sunk); sunk++; last = *o; }

static int feed(uint8_t button, unsigned n, const int16_t (*xy)[2], const int16_t *major)
{
    size_t size = m1tp_mt_build(buf, sizeof(buf), 0, 0, 0, button, n, xy, major);
    assert(size == 46 + 30 * n);
    assert(m1tp_mt_parse(buf, size, &fr) == M1TP_MT_KIND_TOUCH);
    return m1tp_mt_step(&st, &fr, &out);
}

int main(void)
{
    unsigned steps = 0, reports = 0;
    size_t size;

    /* layout constants against the Linux structures */
    assert(M1TP_MT_MOUSE_BYTES + M1TP_MT_HEADER_BYTES == 46);
    assert(M1TP_MT_FINGER_BYTES == 30 && M1TP_MT_MAX_REPORT_BYTES == 526);
    assert(m1tp_mt_enable_payload[0] == 0x02 && m1tp_mt_enable_payload[1] == 0x01);
    assert(m1tp_mt_pointer_descriptor[0] == 0x05 && m1tp_mt_pointer_descriptor[53] == 0xc0);

    /* the shared scenario, with and without a sink */
    assert(m1tp_mt_selftest(&st, buf, sizeof(buf), &steps, &reports, 0, 0) == 0);
    assert(steps == 32 && reports == 18);
    assert(st.events == (M1TP_MT_EV_HEADER_MOTION | M1TP_MT_EV_RIGHT_CLICK | M1TP_MT_EV_SCROLL));
    assert(m1tp_mt_selftest(&st, buf, sizeof(buf), &steps, &reports, sink, &sunk) == 0);
    assert(sunk == reports && last.buttons == 0);
    assert(m1tp_mt_selftest(&st, buf, 100, &steps, &reports, 0, 0) == 1 && steps == 0);
    printf("scenario steps=%u reports=%u\n", steps ? steps : 32, sunk);

    /* parse: sizes and counts the Linux driver refuses */
    const int16_t two[2][2] = { { 100, 200 }, { -300, 400 } };
    size = m1tp_mt_build(buf, sizeof(buf), 0, 0, 0, 0, 2, two, 0);
    assert(size == 106);
    assert(m1tp_mt_parse(buf, size, &fr) == M1TP_MT_KIND_TOUCH && fr.contacts == 2);
    assert(fr.c[1].x == -300 && fr.c[1].y == 400 && fr.c[0].touch_major == 400);
    assert(m1tp_mt_parse(buf, size - 1, &fr) == M1TP_MT_KIND_INVALID);
    assert(m1tp_mt_parse(buf, 45, &fr) == M1TP_MT_KIND_INVALID);
    assert(m1tp_mt_parse(buf, 9, &fr) == M1TP_MT_KIND_INVALID);
    assert(m1tp_mt_parse(buf, 7, &fr) == M1TP_MT_KIND_INVALID);
    assert(m1tp_mt_parse(buf, 0, &fr) == M1TP_MT_KIND_INVALID);
    assert(m1tp_mt_parse(0, 8, &fr) == M1TP_MT_KIND_INVALID);
    assert(m1tp_mt_parse(buf, 8, &fr) == M1TP_MT_KIND_BASIC);
    buf[8 + 22] = 3;  /* three fingers announced, two records present */
    assert(m1tp_mt_parse(buf, size, &fr) == M1TP_MT_KIND_INVALID);
    buf[8 + 22] = 1;  /* fewer announced than present: only those are read */
    assert(m1tp_mt_parse(buf, size, &fr) == M1TP_MT_KIND_TOUCH && fr.contacts == 1 && fr.reported == 1);
    buf[0] = 0x3f;
    assert(m1tp_mt_parse(buf, size, &fr) == M1TP_MT_KIND_INVALID);
    buf[0] = 0x60;
    assert(m1tp_mt_parse(buf, 2, &fr) == M1TP_MT_KIND_RESET);
    assert(m1tp_mt_build(buf, sizeof(buf), 0, 0, 0, 0, 17, two, 0) == 0);
    /* seventeen records cannot be built; a 17-record length is refused by parse */
    memset(buf, 0, sizeof(buf)); buf[0] = 2;
    assert(m1tp_mt_parse(buf, 46 + 30 * 17, &fr) == M1TP_MT_KIND_INVALID);
    assert(m1tp_mt_parse(buf, 46 + 30 * 16, &fr) == M1TP_MT_KIND_TOUCH && fr.contacts == 0);

    /* a record without touch area is not a contact: a two-record click is a left click */
    const int16_t major_one[2] = { 400, 0 };
    m1tp_mt_init(&st, 0);
    assert(feed(0, 2, two, major_one) == 0 && fr.contacts == 1 && fr.reported == 2);
    assert(feed(1, 2, two, major_one) == 1 && out.buttons == 1);
    assert(feed(0, 2, two, major_one) == 1 && out.buttons == 0);

    /* three fingers: no motion, no wheel, the click is the left button */
    const int16_t three_a[3][2] = { { 0, 0 }, { 1000, 0 }, { 2000, 0 } };
    const int16_t three_b[3][2] = { { 0, 900 }, { 1000, 900 }, { 2000, 900 } };
    m1tp_mt_init(&st, 0);
    assert(feed(0, 3, three_a, 0) == 0);
    assert(feed(0, 3, three_b, 0) == 0);
    assert(feed(1, 3, three_b, 0) == 1 && out.buttons == 1 && !out.dx && !out.dy && !out.wheel);
    assert(feed(0, 3, three_b, 0) == 1 && out.buttons == 0);

    /* the button chosen at the press holds until the release, whatever lifts */
    const int16_t pair[2][2] = { { 1000, 3000 }, { 2000, 3000 } };
    const int16_t one[1][2] = { { 1000, 3000 } };
    m1tp_mt_init(&st, 0);
    assert(feed(0, 2, pair, 0) == 0);
    assert(feed(1, 2, pair, 0) == 1 && out.buttons == 2);
    assert(feed(1, 1, one, 0) == 0);
    assert(feed(0, 1, one, 0) == 1 && out.buttons == 0);
    /* ... and a second finger landing during a left press does not turn it right */
    assert(feed(1, 1, one, 0) == 1 && out.buttons == 1);
    assert(feed(1, 2, pair, 0) == 0);
    assert(feed(0, 2, pair, 0) == 1 && out.buttons == 0);

    /* a contact that jumps (finger swapped between frames) is not motion */
    const int16_t far_one[1][2] = { { 5000, 3000 } };
    m1tp_mt_init(&st, 0);
    assert(feed(0, 1, one, 0) == 0);
    assert(feed(0, 1, far_one, 0) == 0);

    /* large motion is clamped to the report's range */
    const int16_t near_one[1][2] = { { 2400, 3000 } };
    m1tp_mt_init(&st, 0);
    assert(feed(0, 1, one, 0) == 0);
    assert(feed(0, 1, near_one, 0) == 1 && out.dx == 127 && out.dy == 0);

    /* sideways two-finger swipe: nothing (the record has no horizontal wheel) */
    const int16_t side[2][2] = { { 1600, 3000 }, { 2600, 3000 } };
    m1tp_mt_init(&st, 0);
    assert(feed(0, 2, pair, 0) == 0);
    assert(feed(0, 2, side, 0) == 0 && st.gesture == M1TP_MT_G_NONE);

    /* resting two fingers (jitter below the slop) neither scrolls nor moves */
    const int16_t jitter[2][2] = { { 1010, 3020 }, { 1990, 2985 } };
    m1tp_mt_init(&st, 0);
    assert(feed(0, 2, pair, 0) == 0);
    assert(feed(0, 2, jitter, 0) == 0 && feed(0, 2, pair, 0) == 0 && st.gesture == M1TP_MT_G_UNDECIDED);

    /* configuration: zero keeps the default; natural scrolling inverts the wheel */
    struct m1tp_mt_config cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.units_per_count = 12; cfg.natural_scroll = 1;
    m1tp_mt_init(&st, &cfg);
    assert(st.cfg.units_per_count == 12 && st.cfg.scroll_units_per_step == 300);
    const int16_t one_b[1][2] = { { 1120, 3000 } };
    assert(feed(0, 1, one, 0) == 0);
    assert(feed(0, 1, one_b, 0) == 1 && out.dx == 10);
    const int16_t pair_up[2][2] = { { 1000, 3600 }, { 2000, 3600 } };
    m1tp_mt_init(&st, &cfg);
    assert(feed(0, 2, pair, 0) == 0);
    assert(feed(0, 2, pair_up, 0) == 1 && out.wheel == -2 && !out.dx && !out.dy);

    /* use_header_motion: once the header carries motion it replaces contact travel */
    memset(&cfg, 0, sizeof(cfg)); cfg.use_header_motion = 1;
    m1tp_mt_init(&st, &cfg);
    assert(feed(0, 1, one, 0) == 0);
    assert(feed(0, 1, one_b, 0) == 1 && out.dx == 30);      /* 120 units / 4, before any header motion */
    size = m1tp_mt_build(buf, sizeof(buf), 0, 3, -2, 0, 1, one, 0);
    assert(m1tp_mt_parse(buf, size, &fr) == M1TP_MT_KIND_TOUCH);
    assert(m1tp_mt_step(&st, &fr, &out) == 1 && out.dx == 3 && out.dy == -2);
    assert(feed(0, 1, one_b, 0) == 0);                      /* header says no motion */
    assert(st.events & M1TP_MT_EV_HEADER_MOTION);

    /* the out-of-range idle value of the basic report is not motion */
    memset(buf, 0, 8); buf[0] = 2; buf[2] = 0x80; buf[3] = 0x80;
    m1tp_mt_init(&st, 0);
    assert(m1tp_mt_parse(buf, 8, &fr) == M1TP_MT_KIND_BASIC && !fr.mouse_dx && !fr.mouse_dy);
    assert(m1tp_mt_step(&st, &fr, &out) == 0);
    buf[2] = 0xcc; buf[3] = 0x1a;  /* bytes of a real basic-mode report (boot pr02) */
    assert(m1tp_mt_parse(buf, 8, &fr) == M1TP_MT_KIND_BASIC);
    assert(m1tp_mt_step(&st, &fr, &out) == 1 && out.dx == -52 && out.dy == 26 && out.buttons == 0);

    /* encoding */
    uint8_t rep[M1TP_MT_POINTER_REPORT_BYTES];
    struct m1tp_mt_out o = { 2, -3, 4, -1 };
    m1tp_mt_encode(&o, rep);
    assert(rep[0] == 2 && rep[1] == 2 && rep[2] == 0xfd && rep[3] == 4 && rep[4] == 0xff);

    printf("m1tp_mt host tests: PASS\n");
    return 0;
}
