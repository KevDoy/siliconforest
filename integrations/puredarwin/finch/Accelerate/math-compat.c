/* SPDX-License-Identifier: MIT */
/* Keep these missing libm entry points local to the numeric candidate. PD's
 * public sincospi helper is available on the root; no libSystem file changes. */
#include <math.h>
#define LOCAL __attribute__((visibility("hidden")))
LOCAL double __sinpi(double x) { double s,c; __sincospi(x,&s,&c); return s; }
LOCAL double __cospi(double x) { double s,c; __sincospi(x,&s,&c); return c; }
LOCAL double __tanpi(double x) { double s,c; __sincospi(x,&s,&c); return s/c; }
LOCAL float __sinpif(float x) { float s,c; __sincospif(x,&s,&c); return s; }
LOCAL float __cospif(float x) { float s,c; __sincospif(x,&s,&c); return c; }
LOCAL float __tanpif(float x) { float s,c; __sincospif(x,&s,&c); return s/c; }
