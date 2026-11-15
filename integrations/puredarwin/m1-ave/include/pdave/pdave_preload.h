/* SPDX-License-Identifier: MIT */
#ifndef PDAVE_PRELOAD_H
#define PDAVE_PRELOAD_H
#include <stddef.h>
#include <stdint.h>
/* Facts only. No image/table bytes or sample per-boot cookies belong here. */
struct pdave_segment {
    uint64_t physical, iova, remap;
    uint32_t size, flags;
};
struct pdave_preload {
    uint32_t soc, preloaded, segment_count, cold_reserved;
    struct pdave_segment segment[2];
    const char *iboot, *firmware, *rtkit;
};
/* Exact reviewed ABI identity and nonoverlapping, page-aligned reserved ranges.
 * cold_reserved is an adapter assertion about a REAL early boot reservation;
 * this pure check does not reserve pages or establish that assertion. */
int pdave_validate_preload(const struct pdave_preload *);
/* Validate the entire blob before the first register callback. Offsets passed
 * are relative to AVE's DPE bank. No bank mapping or power occurs here; programming includes DPE enable. */
struct pdave_register_io {
    void *context;
    int (*read)(void *, uint32_t, uint32_t *);
    int (*write)(void *, uint32_t, uint32_t);
};
int pdave_validate_dpe(const void *bytes, size_t size);
int pdave_program_dpe(const void *bytes, size_t size, const struct pdave_register_io *io);
#endif
