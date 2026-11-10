// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Fixed synthetic scenario for the multitouch decoder: the same table runs in
 * the host tests and inside the kernel (pdm1tpselftest). Reports are built in
 * the layout of drivers/hid/hid-magicmouse.c (see m1tp_mt.h); no recorded
 * device data is used. Expected values assume the default configuration.
 */
#include "m1tp_mt.h"
#include <string.h>

enum { TOUCH = 0, BASIC = 1, RESET = 2 };
struct step {
    uint8_t form, tp_button, contacts;
    int8_t mouse_dx, mouse_dy;      /* BASIC: tp_button is the report's button byte */
    int16_t xy[2][2];
    uint8_t emit, buttons;
    int8_t dx, dy, wheel;
};

static const struct step scenario[] = {
    /* one finger: pointer motion from contact travel (4 units per count) */
    { TOUCH, 0, 0, 0, 0, { { 0, 0 }, { 0, 0 } },               0, 0, 0, 0, 0 },
    { TOUCH, 0, 1, 0, 0, { { 1000, 3000 }, { 0, 0 } },         0, 0, 0, 0, 0 },
    { TOUCH, 0, 1, 0, 0, { { 1040, 3000 }, { 0, 0 } },         1, 0, 10, 0, 0 },
    { TOUCH, 0, 1, 0, 0, { { 1040, 3040 }, { 0, 0 } },         1, 0, 0, -10, 0 },
    /* one-finger click and drag: left button */
    { TOUCH, 1, 1, 0, 0, { { 1040, 3040 }, { 0, 0 } },         1, 1, 0, 0, 0 },
    { TOUCH, 1, 1, 0, 0, { { 1060, 3040 }, { 0, 0 } },         1, 1, 5, 0, 0 },
    { TOUCH, 0, 1, 0, 0, { { 1060, 3040 }, { 0, 0 } },         1, 0, 0, 0, 0 },
    { TOUCH, 0, 0, 0, 0, { { 0, 0 }, { 0, 0 } },               0, 0, 0, 0, 0 },
    /* two-finger click: right button for the press and its release */
    { TOUCH, 0, 2, 0, 0, { { 1000, 3000 }, { 2000, 3000 } },   0, 0, 0, 0, 0 },
    { TOUCH, 1, 2, 0, 0, { { 1000, 3000 }, { 2000, 3000 } },   1, 2, 0, 0, 0 },
    { TOUCH, 0, 2, 0, 0, { { 1000, 3000 }, { 2000, 3000 } },   1, 0, 0, 0, 0 },
    { TOUCH, 0, 0, 0, 0, { { 0, 0 }, { 0, 0 } },               0, 0, 0, 0, 0 },
    /* two-finger drag: wheel steps (300 units each), no pointer motion */
    { TOUCH, 0, 2, 0, 0, { { 1000, 3000 }, { 2000, 3000 } },   0, 0, 0, 0, 0 },
    { TOUCH, 0, 2, 0, 0, { { 1000, 3100 }, { 2000, 3100 } },   0, 0, 0, 0, 0 },
    { TOUCH, 0, 2, 0, 0, { { 1000, 3300 }, { 2000, 3300 } },   1, 0, 0, 0, 1 },
    { TOUCH, 0, 2, 0, 0, { { 2000, 3600 }, { 1000, 3600 } },   1, 0, 0, 0, 1 },
    { TOUCH, 0, 2, 0, 0, { { 1000, 3300 }, { 2000, 3300 } },   1, 0, 0, 0, -1 },
    { TOUCH, 0, 2, 0, 0, { { 1000, 3000 }, { 2000, 3000 } },   1, 0, 0, 0, -1 },
    { TOUCH, 0, 0, 0, 0, { { 0, 0 }, { 0, 0 } },               0, 0, 0, 0, 0 },
    /* two fingers far apart (80 mm): the click stays the left button */
    { TOUCH, 1, 2, 0, 0, { { -4000, 3000 }, { 4000, 3000 } },  1, 1, 0, 0, 0 },
    { TOUCH, 0, 2, 0, 0, { { -4000, 3000 }, { 4000, 3000 } },  1, 0, 0, 0, 0 },
    { TOUCH, 0, 0, 0, 0, { { 0, 0 }, { 0, 0 } },               0, 0, 0, 0, 0 },
    /* one finger rests, the other moves: pointer motion, no wheel */
    { TOUCH, 0, 2, 0, 0, { { 0, 500 }, { 1000, 4000 } },       0, 0, 0, 0, 0 },
    { TOUCH, 0, 2, 0, 0, { { 0, 500 }, { 1200, 4000 } },       1, 0, 50, 0, 0 },
    { TOUCH, 0, 2, 0, 0, { { 0, 500 }, { 1000, 4000 } },       1, 0, -50, 0, 0 },
    { TOUCH, 0, 0, 0, 0, { { 0, 0 }, { 0, 0 } },               0, 0, 0, 0, 0 },
    /* by default the pointer header of a touch report is not the motion */
    { TOUCH, 0, 1, 3, -2, { { 500, 500 }, { 0, 0 } },          0, 0, 0, 0, 0 },
    { TOUCH, 0, 1, 3, -2, { { 700, 500 }, { 0, 0 } },          1, 0, 50, 0, 0 },
    { TOUCH, 0, 0, 0, 0, { { 0, 0 }, { 0, 0 } },               0, 0, 0, 0, 0 },
    /* a basic-mode report passes through; the reset report emits nothing */
    { BASIC, 1, 0, 5, -5, { { 0, 0 }, { 0, 0 } },              1, 1, 5, -5, 0 },
    { BASIC, 0, 0, 0, 0, { { 0, 0 }, { 0, 0 } },               1, 0, 0, 0, 0 },
    { RESET, 0, 0, 0, 0, { { 0, 0 }, { 0, 0 } },               0, 0, 0, 0, 0 },
};

unsigned m1tp_mt_selftest(struct m1tp_mt_state *state, uint8_t *buffer, size_t capacity,
    unsigned *steps, unsigned *reports, m1tp_mt_sink sink, void *context)
{
    const unsigned count = (unsigned)(sizeof(scenario) / sizeof(scenario[0]));
    /* static: keeps the 250-byte frame off the kernel stack; callers are serialized */
    static struct m1tp_mt_frame frame;
    unsigned i, emitted = 0;

    if (steps) *steps = 0;
    if (reports) *reports = 0;
    if (!state || !buffer || capacity < M1TP_MT_MOUSE_BYTES + M1TP_MT_HEADER_BYTES + 2 * M1TP_MT_FINGER_BYTES)
        return 1;
    m1tp_mt_init(state, 0);
    for (i = 0; i < count; i++) {
        const struct step *t = &scenario[i];
        struct m1tp_mt_out out;
        size_t size;
        int kind, emit;

        if (t->form == BASIC) {
            memset(buffer, 0, M1TP_MT_MOUSE_BYTES);
            buffer[0] = M1TP_MT_REPORT_ID; buffer[1] = t->tp_button;
            buffer[2] = (uint8_t)t->mouse_dx; buffer[3] = (uint8_t)t->mouse_dy;
            size = M1TP_MT_MOUSE_BYTES;
        } else if (t->form == RESET) {
            buffer[0] = M1TP_MT_RESET_REPORT_ID; buffer[1] = 0;
            size = 2;
        } else {
            size = m1tp_mt_build(buffer, capacity, 0, t->mouse_dx, t->mouse_dy,
                                 t->tp_button, t->contacts, t->xy, 0);
        }
        if (!size) return i + 1;
        kind = m1tp_mt_parse(buffer, size, &frame);
        if (kind != (t->form == BASIC ? M1TP_MT_KIND_BASIC :
                     t->form == RESET ? M1TP_MT_KIND_RESET : M1TP_MT_KIND_TOUCH))
            return i + 1;
        emit = m1tp_mt_step(state, &frame, &out);
        if (emit != t->emit) return i + 1;
        if (emit) {
            if (out.buttons != t->buttons || out.dx != t->dx || out.dy != t->dy ||
                out.wheel != t->wheel)
                return i + 1;
            emitted++;
            if (sink) sink(context, &out);
        }
        if (steps) *steps = i + 1;
        if (reports) *reports = emitted;
    }
    /* the scenario must leave every button released */
    return state->emitted_buttons ? count + 1 : 0;
}
