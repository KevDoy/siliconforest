/* SPDX-License-Identifier: MIT */
/*
 * /dev/pdavd: the Apple Video Decoder of Apple Silicon as a stateless decoder.
 *
 * The model is Linux's V4L2 stateless ("request") decoder interface, without V4L2: the
 * caller parses the bitstream and passes, per picture, the codec's parameter structures
 * (the `struct v4l2_ctrl_*` of <pdavd/v4l2-controls.h>, the layout Linux uses, identified
 * by their V4L2_CID_STATELESS_* numbers) together with the coded data and the index of the
 * decoded buffer to write. Reference pictures are named by the timestamp of the decode
 * that produced them, as in V4L2.
 *
 * A context is created per stream. Decoded buffers belong to the context, live in the
 * kernel (mapped for the decoder's DART) and are mapped read-only into the caller.
 */
#ifndef _PDAVD_IOCTL_H
#define _PDAVD_IOCTL_H

#include <stdint.h>
#ifdef __APPLE__
#include <sys/ioccom.h>
#else
#include <sys/ioctl.h>   /* the host tests only use the structures */
#endif

#define PDAVD_ABI_VERSION 1

#define PDAVD_FOURCC(a, b, c, d) ((uint32_t)(a) | ((uint32_t)(b) << 8) | ((uint32_t)(c) << 16) | ((uint32_t)(d) << 24))
/* coded formats (the V4L2 fourccs of the stateless formats) */
#define PDAVD_CODEC_H264 PDAVD_FOURCC('S', '2', '6', '4')
#define PDAVD_CODEC_HEVC PDAVD_FOURCC('S', '2', '6', '5')
#define PDAVD_CODEC_VP9  PDAVD_FOURCC('V', 'P', '9', 'F')
/* decoded formats */
#define PDAVD_PIX_NV12   PDAVD_FOURCC('N', 'V', '1', '2')   /* 8 bit 4:2:0, Y then interleaved CbCr */
#define PDAVD_PIX_NV16   PDAVD_FOURCC('N', 'V', '1', '6')   /* 8 bit 4:2:2 */
#define PDAVD_PIX_P010   PDAVD_FOURCC('P', '0', '1', '0')   /* 10 bit in 16-bit little-endian words, high bits */

#define PDAVD_CAP_HEVC (1u << 0)
#define PDAVD_CAP_H264 (1u << 1)
#define PDAVD_CAP_VP9  (1u << 2)
#define PDAVD_CAP_AV1  (1u << 3)

#define PDAVD_MAX_BUFFERS 32
#define PDAVD_MAX_CTRLS   8

struct pdavd_info {
	uint32_t abi;            /* PDAVD_ABI_VERSION */
	uint32_t caps;           /* PDAVD_CAP_* */
	uint32_t booted;         /* the decoder's firmware is running */
	uint32_t contexts;       /* contexts in use */
	char     firmware[48];   /* the "CODECS:" text of the firmware image, if any */
};

struct pdavd_format {
	uint32_t pixelformat;    /* PDAVD_PIX_* */
	uint32_t width, height;  /* of the decoded buffer: coded size rounded up (64 x 16) */
	uint32_t bytesperline;   /* luma stride; the chroma plane has the same stride */
	uint32_t uv_offset;      /* offset of the CbCr plane in the buffer */
	uint32_t sizeimage;      /* bytes per decoded buffer, including the decoder's private tail */
};

struct pdavd_ctrl {
	uint32_t id;             /* V4L2_CID_STATELESS_* */
	uint32_t size;           /* bytes; a multiple of the element size for array controls */
	uint64_t ptr;            /* user address */
};

struct pdavd_create {
	uint32_t          codec;        /* in: PDAVD_CODEC_* */
	uint32_t          width, height;/* in: coded size in luma samples */
	uint32_t          buffers;      /* in: decoded buffers to allocate (<= PDAVD_MAX_BUFFERS) */
	struct pdavd_ctrl format_ctrl;  /* in: the SPS (H.264, HEVC) or the VP9 frame header */
	uint32_t          ctx;          /* out: context number */
	struct pdavd_format format;     /* out */
};

struct pdavd_map {
	uint32_t ctx;
	uint32_t index;          /* decoded buffer */
	uint64_t address;        /* out: read-only mapping in the caller, valid until the context is destroyed */
	uint64_t size;           /* out */
};

#define PDAVD_DECODE_HOLD (1u << 0)   /* more slices of this picture follow: queue only */

struct pdavd_decode {
	uint32_t          ctx;
	uint32_t          flags;        /* PDAVD_DECODE_* */
	uint32_t          dst;          /* decoded buffer to write */
	uint32_t          nctrls;
	uint64_t          timestamp;    /* names the picture for later references */
	uint64_t          data;         /* user address of the coded data (one slice for H.264) */
	uint32_t          size;
	uint32_t          status;       /* out: last mailbox word of the decoder */
	uint64_t          decode_us;    /* out: time from submission to completion */
	struct pdavd_ctrl ctrls[PDAVD_MAX_CTRLS];
};

struct pdavd_destroy {
	uint32_t ctx;
};

#define PDAVD_IOC_INFO    _IOR('V', 1, struct pdavd_info)
#define PDAVD_IOC_CREATE  _IOWR('V', 2, struct pdavd_create)
#define PDAVD_IOC_MAP     _IOWR('V', 3, struct pdavd_map)
#define PDAVD_IOC_DECODE  _IOWR('V', 4, struct pdavd_decode)
#define PDAVD_IOC_DESTROY _IOW('V', 5, struct pdavd_destroy)

#endif
