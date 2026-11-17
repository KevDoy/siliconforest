/* SPDX-License-Identifier: MIT */
#ifndef SFAVE_IDENTITY_H
#define SFAVE_IDENTITY_H
#include <stddef.h>
/* Reads a caller-validated normal-RAM TEXT span. Returns one unique numeric
 * version after prefix; repeated identical tags are fine, conflicting tags fail.
 * No image data is returned, only the bounded ASCII prefix+version. */
int sfave_find_identity(const void *, size_t, const char *, char *, size_t);
/* Registry text may be padded OSData, bounded non-NUL data, or an OSString.
 * Copy printable text only; never return binary/padding beyond its first NUL. */
int sfave_copy_metadata_text(const void *,size_t,char *,size_t);
#endif
