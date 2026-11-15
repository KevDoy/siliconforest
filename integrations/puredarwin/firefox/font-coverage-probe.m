/* SPDX-License-Identifier: MIT */
#import <Foundation/Foundation.h>
#import <CoreGraphics/CoreGraphics.h>
#import <CoreText/CoreText.h>
#include <ft2build.h>
#include FT_FREETYPE_H
#include <stdio.h>
static void name(CFStringRef text,char output[256]){
 if(!text||!CFStringGetCString(text,output,256,kCFStringEncodingUTF8))snprintf(output,256,"?");
}
int main(void){@autoreleasepool{
 const char *texts[]={"A","Ε","日","द","ا","🙂","🌲","🚀"};
 unsigned codes[]={0x41,0x395,0x65e5,0x926,0x627,0x1f642,0x1f332,0x1f680};int mismatches=0,missing=0;
 CTFontRef base=CTFontCreateWithName(CFSTR("sans-serif"),28,NULL);if(!base)return 2;
 for(int i=0;i<8;i++){
  CFStringRef text=CFStringCreateWithCString(NULL,texts[i],kCFStringEncodingUTF8);CFIndex count=CFStringGetLength(text);
  CTFontRef fallback=CTFontCreateForString(base,text,CFRangeMake(0,count));if(!fallback)return 2;
  CGFontRef graphics=CTFontCopyGraphicsFont(fallback,NULL);if(!graphics)return 2;
  UniChar chars[2]={0};CGGlyph glyphs[2]={0},roundtripGlyphs[2]={0};CFStringGetCharacters(text,CFRangeMake(0,count),chars);
  int found=CTFontGetGlyphsForCharacters(fallback,chars,glyphs,count);
  CTFontRef roundtrip=CTFontCreateWithGraphicsFont(graphics,28,NULL,NULL);
  CTFontGetGlyphsForCharacters(roundtrip,chars,roundtripGlyphs,count);
  FT_Face face=[(id)graphics respondsToSelector:@selector(face)]?(FT_Face)[(id)graphics performSelector:@selector(face)]:NULL;
  unsigned expected=face?FT_Get_Char_Index(face,codes[i]):0;
  CFStringRef ctName=CTFontCopyPostScriptName(fallback),cgName=CGFontCopyPostScriptName(graphics);char a[256],b[256];name(ctName,a);name(cgName,b);
  int same=face&&glyphs[0]==expected&&roundtripGlyphs[0]==expected;
  printf("FXFONTCOVER U+%04X CT=%s CG=%s found=%d glyph=%u roundtrip=%u graphicsFT=%u same=%d base_equal=%d\n",codes[i],a,b,found,glyphs[0],roundtripGlyphs[0],expected,same,CFEqual(base,fallback));
  mismatches+=!same;missing+=!found;
  if(ctName)CFRelease(ctName);if(cgName)CFRelease(cgName);CFRelease(roundtrip);CGFontRelease(graphics);CFRelease(fallback);CFRelease(text);
 }
 CFRelease(base);printf("FXFONTCOVER mismatches=%d missing=%d\n",mismatches,missing);return mismatches?1:0;
}}
