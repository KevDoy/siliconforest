/* SPDX-License-Identifier: MIT */
/*
 * libpdavd: access to the Apple Video Decoder through /dev/pdavd (PDAppleAVD.kext).
 *
 * The decoder is stateless (see <pdavd/pdavd_ioctl.h>): the caller parses the bitstream.
 * Programs normally do not use this library directly but FFmpeg's `pdavd` hwaccel, which
 * is built on it and does the parsing, reference handling and reordering.
 *
 * A device name of the form "record:<file>" does not open the decoder: every call is
 * written to <file> instead (decoded buffers are plain zeroed memory), so that a decode
 * session can be captured on any machine and replayed against the driver's host model or
 * against the real decoder (tools pdavd-host and pdavd-replay). The environment variable
 * PDAVD_DEVICE overrides the default name "/dev/pdavd".
 */
#ifndef _PDAVD_H
#define _PDAVD_H

#include <stddef.h>
#include <stdint.h>

#include "pdavd_ioctl.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct pdavd_dev pdavd_dev;

/* What the caller knows about a picture and the decoder does not; kept with recordings. */
struct pdavd_meta {
	int64_t  pts;             /* presentation time, in the caller's units */
	uint32_t width, height;   /* visible size (top-left aligned crop of the decoded buffer) */
	uint32_t crop_left, crop_top;
	uint32_t flags;           /* PDAVD_META_* */
	uint32_t reserved;
};
#define PDAVD_META_KEY (1u << 0)

/* NULL: $PDAVD_DEVICE, else /dev/pdavd. NULL with errno set on failure. */
pdavd_dev *pdavd_open(const char *name);
void pdavd_close(pdavd_dev *dev);
int  pdavd_info(pdavd_dev *dev, struct pdavd_info *info);
/* The calls below return 0, or -1 with errno set. */
int  pdavd_create(pdavd_dev *dev, struct pdavd_create *create);
/* Read-only view of decoded buffer `index`: Y plane at 0, CbCr at format.uv_offset. */
int  pdavd_map(pdavd_dev *dev, uint32_t ctx, uint32_t index, const uint8_t **address, size_t *size);
/* `meta` may be NULL; it should be given with the last (non-HOLD) call of a picture. */
int  pdavd_decode(pdavd_dev *dev, struct pdavd_decode *decode, const struct pdavd_meta *meta);
/* Optional atomic import. Caller mappings live until DESTROY. Recording mode
 * refuses ENOTSUP; older kernels refuse ENOTTY. Query the optional cap first. */
int  pdavd_import_buffers(pdavd_dev *dev, uint32_t ctx,
                         const struct pdavd_import_buffer *buffers, uint32_t count);
int  pdavd_destroy(pdavd_dev *dev, uint32_t ctx);

/* ---- recordings ----------------------------------------------------------------------- */
#define PDAVD_REC_MAGIC   0x4a414450u   /* "PDAJ" */
enum { PDAVD_REC_CREATE = 1, PDAVD_REC_DECODE = 2, PDAVD_REC_DESTROY = 3 };
/*
 * A recording is a sequence of records: { u32 magic, u32 type, u32 length } then `length`
 * bytes:
 *   CREATE:  struct pdavd_create, then the bytes of its format control
 *   DECODE:  struct pdavd_decode, struct pdavd_meta, then for each control its bytes, then
 *            the coded data
 *   DESTROY: struct pdavd_destroy
 * (the user addresses inside the structures are meaningless in a recording).
 */

#ifdef __cplusplus
}
#endif
#endif
