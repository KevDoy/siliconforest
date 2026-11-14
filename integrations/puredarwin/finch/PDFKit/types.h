/* SPDX-License-Identifier: MIT */
#import <AppKit/AppKit.h>
#import <CoreGraphics/CoreGraphics.h>
#import <PDFKit/Numeric.h>
#import <PDFKit/KNumeric.h>
typedef NSString *PDFDocumentAttribute,*PDFDocumentWriteOption,*PDFAnnotationSubtype,*PDFAnnotationKey,*PDFBorderKey;
typedef NSRect PDFRect;
typedef NSPoint PDFPoint;
typedef NSSize PDFSize;
typedef NSInteger PDFDisplayBox,PDFBorderStyle,PDFDocumentPermissions,PDFPrintScalingMode;
typedef NSUInteger PDFAccessPermissions,PDFSelectionGranularity;
@class PDFView;
@protocol PDFDocumentDelegate <NSObject>
@end
