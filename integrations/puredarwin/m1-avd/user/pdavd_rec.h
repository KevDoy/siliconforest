/* SPDX-License-Identifier: MIT */
/* Reader of libpdavd recordings (see <pdavd/pdavd.h>), shared by pdavd-replay and the host test. */
#ifndef _PDAVD_REC_H
#define _PDAVD_REC_H

#include <pdavd/pdavd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct pdavd_rec {
	uint32_t type;
	uint32_t length;
	uint8_t *body;          /* owned by the reader, valid until the next read */
	size_t   cap;
	/* decoded views of the body */
	struct pdavd_create  create;
	const uint8_t       *format_ctrl;
	struct pdavd_decode  decode;
	struct pdavd_meta    meta;
	const uint8_t       *ctrl[PDAVD_MAX_CTRLS];
	const uint8_t       *data;
	struct pdavd_destroy destroy;
};

/* 1: a record was read; 0: end of file; -1: malformed */
static inline int
pdavd_rec_read(FILE *f, struct pdavd_rec *r)
{
	uint32_t head[3];
	size_t at;

	if (fread(head, sizeof(head), 1, f) != 1)
		return feof(f) ? 0 : -1;
	if (head[0] != PDAVD_REC_MAGIC || head[2] > (256u << 20))
		return -1;
	r->type = head[1];
	r->length = head[2];
	if (r->cap < r->length) {
		free(r->body);
		r->body = malloc(r->length);
		r->cap = r->body ? r->length : 0;
		if (r->body == NULL)
			return -1;
	}
	if (r->length && fread(r->body, r->length, 1, f) != 1)
		return -1;
	switch (r->type) {
	case PDAVD_REC_CREATE:
		if (r->length < sizeof(r->create))
			return -1;
		memcpy(&r->create, r->body, sizeof(r->create));
		if (r->length != sizeof(r->create) + r->create.format_ctrl.size)
			return -1;
		r->format_ctrl = r->body + sizeof(r->create);
		return 1;
	case PDAVD_REC_DECODE:
		if (r->length < sizeof(r->decode) + sizeof(r->meta))
			return -1;
		memcpy(&r->decode, r->body, sizeof(r->decode));
		memcpy(&r->meta, r->body + sizeof(r->decode), sizeof(r->meta));
		at = sizeof(r->decode) + sizeof(r->meta);
		if (r->decode.nctrls > PDAVD_MAX_CTRLS)
			return -1;
		for (uint32_t i = 0; i < r->decode.nctrls; i++) {
			if (r->decode.ctrls[i].size > r->length - at)
				return -1;
			r->ctrl[i] = r->body + at;
			at += r->decode.ctrls[i].size;
		}
		if (r->decode.size != r->length - at)
			return -1;
		r->data = r->body + at;
		return 1;
	case PDAVD_REC_DESTROY:
		if (r->length != sizeof(r->destroy))
			return -1;
		memcpy(&r->destroy, r->body, sizeof(r->destroy));
		return 1;
	default:
		return -1;
	}
}

#endif
