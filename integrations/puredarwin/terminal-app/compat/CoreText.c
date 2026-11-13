// SPDX-License-Identifier: MIT
// CoreText additions (wrapper over the Cocoa stream's CoreText, re-exported).
#include <CoreFoundation/CoreFoundation.h>
#include <stdbool.h>
#include <stdlib.h>

typedef struct CGContext *CGContextRef;
typedef const struct __CTFont *CTFontRef;
typedef unsigned short CGGlyph;
typedef struct { double x, y; } SFPoint;
typedef struct { double width, height; } SFSize;
extern void CTFontDrawGlyphs(CTFontRef font, const CGGlyph *glyphs, const SFPoint *positions, size_t count, CGContextRef context);
extern SFPoint CGContextGetTextPosition(CGContextRef c);
extern void CGContextSetTextPosition(CGContextRef c, double x, double y);

// private: glyphs laid out by their advances from the context's text position, which moves on
void CTFontDrawGlyphsWithAdvances(CTFontRef font, const CGGlyph *glyphs, const SFSize *advances, size_t count, CGContextRef context)
{
    if (font == NULL || glyphs == NULL || advances == NULL || context == NULL || count == 0) return;
    SFPoint stack[256], *positions = count <= 256 ? stack : malloc(count * sizeof(SFPoint));
    if (positions == NULL) return;
    SFPoint at = CGContextGetTextPosition(context);
    double x = 0, y = 0;
    for (size_t i = 0; i < count; i++) {
        positions[i].x = at.x + x; positions[i].y = at.y + y;
        x += advances[i].width; y += advances[i].height;
    }
    CTFontDrawGlyphs(font, glyphs, positions, count, context);
    CGContextSetTextPosition(context, at.x + x, at.y + y);
    if (positions != stack) free(positions);
}

// private: every font is drawn anti-aliased by Onyx2D
bool CTFontShouldAntiAlias(CTFontRef font) { return true; }
