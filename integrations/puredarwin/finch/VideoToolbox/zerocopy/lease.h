/* SPDX-License-Identifier: MIT */
#pragma once
#include <CoreFoundation/CoreFoundation.h>
#include <IOSurface/IOSurface.h>
#include <libavutil/frame.h>
/* +1 private lifetime object (not a mutable attachment). It bridges producer usecount through callback
 * lifetime and holds the actual AVFrame until all coordinated consumers/fences
 * have dropped their surface usecounts. Count alone does not create a fence. */
CFTypeRef SFVTCreateFrameLease(const AVFrame *frame,IOSurfaceRef surface);
void SFVTFrameLeaseCounts(uint64_t *issued,uint64_t *released,uint64_t *deferred);
