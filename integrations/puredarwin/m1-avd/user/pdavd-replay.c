// SPDX-License-Identifier: MIT
/*
 * pdavd-replay: plays a libpdavd recording (made on any machine with
 * PDAVD_DEVICE=record:<file> under FFmpeg's pdavd hwaccel) into the real decoder and
 * prints, for every picture, the MD5 of its visible NV12 data: Y rows then CbCr rows,
 * tightly packed, which is what `ffmpeg -pix_fmt nv12 -f framemd5` hashes for a software
 * decode of the same stream. Pictures come in decode order with their presentation time;
 * tools/avd_compare.py puts them in display order and compares.
 *
 *   pdavd-replay [-n <pictures>] [-l <loops>] [-q] [-d <frame.nv12>:<picture>] <recording>
 *
 * Output lines:  PDAVD_FRAME <n> pts <pts> <width>x<height> <md5> us <decode time> st <status>
 *                PDAVD_FAIL  <n> pts <pts> errno <e> st <status>
 *                PDAVD_SUMMARY ...
 */
#include <errno.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <pdavd/pdavd.h>

#include "pd_md5.h"
#include "pdavd_rec.h"

struct ctxmap {
	int      used;
	uint32_t real;
	struct pdavd_format format;
	const uint8_t *buf[PDAVD_MAX_BUFFERS];
};

int
main(int argc, char **argv)
{
	const char *path = NULL, *dump = NULL;
	long limit = -1, loops = 1, dump_at = -1;
	int quiet = 0, rc = 0;
	uint64_t pictures = 0, failed = 0, us_total = 0, us_max = 0, bytes = 0;
	struct pdavd_info info;
	pdavd_dev *dev;

	for (int i = 1; i < argc; i++) {
		if (strcmp(argv[i], "-n") == 0 && i + 1 < argc)
			limit = atol(argv[++i]);
		else if (strcmp(argv[i], "-l") == 0 && i + 1 < argc)
			loops = atol(argv[++i]);
		else if (strcmp(argv[i], "-q") == 0)
			quiet = 1;
		else if (strcmp(argv[i], "-d") == 0 && i + 1 < argc) {
			char *colon = strrchr(argv[++i], ':');
			dump = argv[i];
			if (colon != NULL) {
				*colon = 0;
				dump_at = atol(colon + 1);
			}
		} else
			path = argv[i];
	}
	if (path == NULL) {
		fprintf(stderr, "usage: pdavd-replay [-n pictures] [-l loops] [-q] [-d file:picture] <recording>\n");
		return 2;
	}
	dev = pdavd_open(NULL);
	if (dev == NULL) {
		printf("PDAVD_OPEN_FAILED errno %d (%s)\n", errno, strerror(errno));
		return 1;
	}
	if (pdavd_info(dev, &info) != 0) {
		printf("PDAVD_INFO_FAILED errno %d\n", errno);
		return 1;
	}
	printf("PDAVD_INFO abi %u caps 0x%x firmware \"%s\"\n", info.abi, info.caps, info.firmware);

	for (long loop = 0; loop < loops && rc == 0; loop++) {
		FILE *f = fopen(path, "rb");
		struct pdavd_rec r;
		struct ctxmap map[16];
		long n = 0;
		int got;

		if (f == NULL) {
			printf("PDAVD_NO_RECORDING %s\n", path);
			return 2;
		}
		memset(&r, 0, sizeof(r));
		memset(map, 0, sizeof(map));
		while ((got = pdavd_rec_read(f, &r)) == 1) {
			if (r.type == PDAVD_REC_CREATE) {
				struct pdavd_create c = r.create;
				uint32_t rec = c.ctx;

				if (rec >= 16)
					break;
				c.format_ctrl.ptr = (uintptr_t)r.format_ctrl;
				if (pdavd_create(dev, &c) != 0) {
					printf("PDAVD_CREATE_FAILED errno %d (%s) codec %.4s %ux%u buffers %u\n", errno, strerror(errno),
					    (const char *)&c.codec, c.width, c.height, c.buffers);
					rc = 1;
					break;
				}
				map[rec].used = 1;
				map[rec].real = c.ctx;
				map[rec].format = c.format;
				if (loop == 0)
					printf("PDAVD_CONTEXT %u codec %.4s %ux%u -> %.4s %ux%u stride %u uv %u size %u buffers %u\n", c.ctx,
					    (const char *)&c.codec, c.width, c.height, (const char *)&c.format.pixelformat, c.format.width,
					    c.format.height, c.format.bytesperline, c.format.uv_offset, c.format.sizeimage, c.buffers);
			} else if (r.type == PDAVD_REC_DECODE) {
				struct pdavd_decode d = r.decode;
				struct ctxmap *m = d.ctx < 16 && map[d.ctx].used ? &map[d.ctx] : NULL;
				int hold = d.flags & PDAVD_DECODE_HOLD;

				if (m == NULL || d.dst >= PDAVD_MAX_BUFFERS)
					break;
				d.ctx = m->real;
				d.data = (uintptr_t)r.data;
				for (uint32_t i = 0; i < d.nctrls; i++)
					d.ctrls[i].ptr = (uintptr_t)r.ctrl[i];
				bytes += d.size;
				if (pdavd_decode(dev, &d, NULL) != 0) {
					printf("PDAVD_FAIL %ld pts %" PRId64 " errno %d st 0x%x\n", n, r.meta.pts, errno, d.status);
					failed++;
					if (!hold)
						n++;
					if (failed > 20) {
						rc = 1;
						break;
					}
					continue;
				}
				if (hold)
					continue;
				pictures++;
				us_total += d.decode_us;
				if (d.decode_us > us_max)
					us_max = d.decode_us;
				if (!quiet || dump_at == n) {
					uint32_t w = r.meta.width, h = r.meta.height;
					const uint8_t *p;
					struct pd_md5 md5;
					char hex[33];
					size_t size;

					if (m->buf[d.dst] == NULL && pdavd_map(dev, m->real, d.dst, &m->buf[d.dst], &size) != 0) {
						printf("PDAVD_MAP_FAILED %u errno %d\n", d.dst, errno);
						rc = 1;
						break;
					}
					p = m->buf[d.dst];
					if (w == 0 || w > m->format.width || h == 0 || h > m->format.height) {
						w = m->format.width;
						h = m->format.height;
					}
					pd_md5_init(&md5);
					for (uint32_t y = 0; y < h; y++)
						pd_md5_update(&md5, p + (size_t)(y + r.meta.crop_top) * m->format.bytesperline + r.meta.crop_left, w);
					for (uint32_t y = 0; y < (h + 1) / 2; y++)
						pd_md5_update(&md5, p + m->format.uv_offset +
						    (size_t)(y + r.meta.crop_top / 2) * m->format.bytesperline + (r.meta.crop_left & ~1u),
						    (w + 1) & ~1u);
					pd_md5_final(&md5, hex);
					if (!quiet)
						printf("PDAVD_FRAME %ld pts %" PRId64 " %ux%u %s us %" PRIu64 " st 0x%x\n", n, r.meta.pts, w, h, hex,
						    (uint64_t)d.decode_us, d.status);
					if (dump != NULL && dump_at == n && loop == 0) {
						FILE *o = fopen(dump, "wb");
						if (o != NULL) {
							for (uint32_t y = 0; y < h; y++)
								fwrite(p + (size_t)y * m->format.bytesperline, 1, w, o);
							for (uint32_t y = 0; y < (h + 1) / 2; y++)
								fwrite(p + m->format.uv_offset + (size_t)y * m->format.bytesperline, 1, (w + 1) & ~1u, o);
							fclose(o);
							printf("PDAVD_DUMPED picture %ld %ux%u nv12 to %s\n", n, w, h, dump);
						}
					}
				}
				n++;
				if (limit >= 0 && n >= limit)
					break;
			} else if (r.destroy.ctx < 16 && map[r.destroy.ctx].used) {
				pdavd_destroy(dev, map[r.destroy.ctx].real);
				map[r.destroy.ctx].used = 0;
			}
		}
		if (got < 0) {
			printf("PDAVD_BAD_RECORDING\n");
			rc = 2;
		}
		for (unsigned c = 0; c < 16; c++)
			if (map[c].used)
				pdavd_destroy(dev, map[c].real);
		free(r.body);
		fclose(f);
	}
	printf("PDAVD_SUMMARY pictures %" PRIu64 " failed %" PRIu64 " coded_bytes %" PRIu64 " decode_us_mean %" PRIu64
	    " decode_us_max %" PRIu64 " loops %ld\n", pictures, failed, bytes, pictures ? us_total / pictures : 0, us_max, loops);
	pdavd_close(dev);
	return rc ? rc : (failed ? 1 : 0);
}
