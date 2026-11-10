/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Apple SPI trackpad multitouch report decoder and pointer synthesis for
 * PDM1SPIHID (J293).
 *
 * The report layout (struct tp_mouse_report, struct tp_header, struct
 * tp_finger), the validity rules and the "skip touch_major == 0" filter are
 * ported from drivers/hid/hid-magicmouse.c of Omacom linux-aurora
 * (integration/aurora, 6a08a08e):
 *
 *   Apple "Magic" Wireless Mouse driver
 *   Copyright (c) 2010 Michael Poole <mdpoole@troilus.org>
 *   Copyright (c) 2010 Chase Douglas <chase.douglas@canonical.com>
 *
 * The pointer synthesis (right button on a two-finger click, wheel steps on a
 * two-finger drag) has no counterpart in that driver, which hands contacts to
 * user space; it is new code under the same licence.
 *
 * Pure C, no allocation, no I/O: built into the kext and into the host tests.
 */
#ifndef M1TP_MT_H
#define M1TP_MT_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif

/* hid-magicmouse.c: SPI_REPORT_ID, SPI_RESET_REPORT_ID, MAX_CONTACTS */
#define M1TP_MT_REPORT_ID 0x02u
#define M1TP_MT_RESET_REPORT_ID 0x60u
#define M1TP_MT_MAX_CONTACTS 16u
/* sizeof(struct tp_mouse_report), sizeof(struct tp_header), sizeof(struct tp_finger) */
#define M1TP_MT_MOUSE_BYTES 8u
#define M1TP_MT_HEADER_BYTES 38u
#define M1TP_MT_FINGER_BYTES 30u
#define M1TP_MT_MAX_REPORT_BYTES (M1TP_MT_MOUSE_BYTES + M1TP_MT_HEADER_BYTES + \
    M1TP_MT_MAX_CONTACTS * M1TP_MT_FINGER_BYTES)

/* SET_REPORT(feature) payload that requests touch data: feature_mt_trackpad2_usb
 * in magicmouse_enable_multitouch(), sent by spi-hid-apple-core.c as request
 * 0x52, report 0x02, response length 2, to interface 2. */
#define M1TP_MT_ENABLE_COMMAND 0x52u
#define M1TP_MT_ENABLE_REPORT 0x02u
#define M1TP_MT_ENABLE_RESPONSE_BYTES 2u
extern const uint8_t m1tp_mt_enable_payload[2];

/* The report the child HID device publishes in multitouch mode: report ID 2,
 * three buttons, relative X, Y and wheel. */
#define M1TP_MT_POINTER_REPORT_BYTES 5u
#define M1TP_MT_POINTER_DESCRIPTOR_BYTES 54u
extern const uint8_t m1tp_mt_pointer_descriptor[M1TP_MT_POINTER_DESCRIPTOR_BYTES];

enum m1tp_mt_kind {
    M1TP_MT_KIND_INVALID = 0,  /* not a report this decoder accepts; dropped */
    M1TP_MT_KIND_BASIC = 1,    /* 8-byte pointer report of the basic mode */
    M1TP_MT_KIND_TOUCH = 2,    /* pointer header + vendor header + contacts */
    M1TP_MT_KIND_RESET = 3     /* controller reset: the mode must be requested again */
};

struct m1tp_mt_contact {
    int16_t x, y;              /* abs_x, abs_y as sent: y grows away from the user */
    int16_t rel_x, rel_y;      /* not used for synthesis; printed by the debug dump */
    int16_t touch_major, touch_minor, pressure;
};

struct m1tp_mt_frame {
    uint8_t kind;
    uint8_t mouse_buttons;     /* tp_mouse_report.buttons */
    int8_t mouse_dx, mouse_dy; /* tp_mouse_report.rel_x, rel_y */
    uint8_t tp_buttons;        /* tp_header.buttons */
    uint8_t reported;          /* tp_header.num_fingers */
    uint8_t contacts;          /* records with touch_major != 0 */
    struct m1tp_mt_contact c[M1TP_MT_MAX_CONTACTS];
};

struct m1tp_mt_config {
    uint32_t units_per_count;       /* contact units per pointer count */
    uint32_t scroll_units_per_step; /* contact units of two-finger travel per wheel step */
    uint32_t click_span_x, click_span_y; /* two fingers further apart click as one */
    uint32_t slop_units;            /* travel before a two-finger gesture is classified */
    uint32_t jump_units;            /* a larger per-frame move is a new contact, not motion */
    uint8_t natural_scroll;         /* 1: content follows the fingers */
    uint8_t use_header_motion;      /* 1: once the pointer header of a touch report carries
                                     * motion, take the pointer motion from it */
};

struct m1tp_mt_out {
    uint8_t buttons;                /* bit 0 left, bit 1 right */
    int8_t dx, dy, wheel;           /* dy grows towards the user, wheel + is away */
};

enum m1tp_mt_gesture { M1TP_MT_G_UNDECIDED = 0, M1TP_MT_G_SCROLL = 1,
    M1TP_MT_G_POINT = 2, M1TP_MT_G_NONE = 3 };

/* One-shot facts for the driver's stage log lines. */
#define M1TP_MT_EV_HEADER_MOTION 1u /* a touch report's pointer header carried motion */
#define M1TP_MT_EV_RIGHT_CLICK 2u
#define M1TP_MT_EV_SCROLL 4u

struct m1tp_mt_state {
    struct m1tp_mt_config cfg;
    uint8_t have_prev, prev_count;
    int32_t prev_x[2], prev_y[2];   /* the two tracked contacts when prev_count == 2 */
    int32_t prev_sum_x, prev_sum_y;
    uint8_t button_down, button_mask, emitted_buttons;
    uint8_t header_motion;          /* latched (use_header_motion): the firmware's own motion */
    uint8_t gesture;
    int32_t travel_x[2], travel_y[2];
    int32_t rem_x, rem_y, rem_scroll;
    uint32_t events;                /* M1TP_MT_EV_* seen so far */
};

void m1tp_mt_default_config(struct m1tp_mt_config *);
/* A zero field of *config takes its default. */
void m1tp_mt_init(struct m1tp_mt_state *, const struct m1tp_mt_config *config);
/* Classify and decode one CRC-checked interface 2 input report. Always fills
 * *frame; returns frame->kind. */
int m1tp_mt_parse(const uint8_t *payload, size_t bytes, struct m1tp_mt_frame *frame);
/* Advance by one decoded frame. Returns 1 when *out is a pointer report to
 * deliver, 0 when there is nothing to report. */
int m1tp_mt_step(struct m1tp_mt_state *, const struct m1tp_mt_frame *, struct m1tp_mt_out *out);
/* out -> the 5-byte report described by m1tp_mt_pointer_descriptor. */
void m1tp_mt_encode(const struct m1tp_mt_out *, uint8_t report[M1TP_MT_POINTER_REPORT_BYTES]);

/* Fixture builder shared by the host tests and the in-kernel self-test: writes
 * a touch-mode report. Every contact gets touch_major = touch_minor = 400 and
 * pressure 60 unless xy is given with major[] (may be NULL). Returns the report
 * length, or 0 if it does not fit. */
size_t m1tp_mt_build(uint8_t *report, size_t capacity, uint8_t mouse_buttons,
    int8_t mouse_dx, int8_t mouse_dy, uint8_t tp_buttons, unsigned contacts,
    const int16_t (*xy)[2], const int16_t *major);

/* Runs the fixed synthetic scenario (m1tp_mt_selftest.c) through parse + step
 * on a scratch state and compares every emitted pointer report. Returns 0 when
 * all match, else (step index + 1). *steps and *reports receive the counts.
 * A non-NULL sink also receives each emitted report, in order. */
typedef void (*m1tp_mt_sink)(void *context, const struct m1tp_mt_out *);
unsigned m1tp_mt_selftest(struct m1tp_mt_state *scratch, uint8_t *buffer, size_t capacity,
    unsigned *steps, unsigned *reports, m1tp_mt_sink sink, void *context);

#ifdef __cplusplus
}
#endif
#endif
