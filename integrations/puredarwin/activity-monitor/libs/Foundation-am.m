// SPDX-License-Identifier: MIT
// What Apple's Activity Monitor needs from Foundation beyond what Terminal.app needed
// (docs/roadmap/issues/SF-AA-activity-monitor.md); built into the Foundation wrapper with
// ../../terminal-app/compat/Foundation.m.
//   NSIndexPath (and AppKit's item/section form of it)
//   NSDateComponentsFormatter (positional, abbreviated, short, full and spell-out-less styles of a
//     time interval in hours, minutes and seconds; what Activity Monitor formats: CPU time, uptime)
//   methods the libFoundation here lacks: -[NSString enumerateLinesUsingBlock:],
//     -[NSString localizedStandardCompare:], +[NSLocale autoupdatingCurrentLocale],
//     -[NSFileManager contentsOfDirectoryAtURL:includingPropertiesForKeys:options:error:],
//     -[NSFileHandle setReadabilityHandler:] (a dispatch read source), and NSNumberFormatter's
//     minimumIntegerDigits / roundingMode / format, which are kept (this formatter's output
//     always has at least one integer digit and rounds half to even)
#import <Foundation/Foundation.h>
#include <dispatch/dispatch.h>
#include <objc/runtime.h>

#define EXPORT __attribute__((visibility("default")))

/* ---- NSIndexPath ---- */

EXPORT
@interface NSIndexPath : NSObject <NSCopying, NSCoding> {
    NSUInteger *_indexes;
    NSUInteger _length;
}
@end

@implementation NSIndexPath

+ (id)indexPathWithIndex:(NSUInteger)index { return [[[self alloc] initWithIndexes:&index length:1] autorelease]; }
+ (id)indexPathWithIndexes:(const NSUInteger *)indexes length:(NSUInteger)length { return [[[self alloc] initWithIndexes:indexes length:length] autorelease]; }
+ (id)indexPathForItem:(NSInteger)item inSection:(NSInteger)section {
    NSUInteger ix[2] = { (NSUInteger)section, (NSUInteger)item };
    return [self indexPathWithIndexes:ix length:2];
}
+ (id)indexPathForRow:(NSInteger)row inSection:(NSInteger)section { return [self indexPathForItem:row inSection:section]; }

- (id)init { return [self initWithIndexes:NULL length:0]; }
- (id)initWithIndex:(NSUInteger)index { return [self initWithIndexes:&index length:1]; }
- (id)initWithIndexes:(const NSUInteger *)indexes length:(NSUInteger)length {
    if ((self = [super init]) != nil) {
        _length = length;
        _indexes = length ? malloc(length * sizeof(NSUInteger)) : NULL;
        if (length)
            memcpy(_indexes, indexes, length * sizeof(NSUInteger));
    }
    return self;
}
- (void)dealloc { free(_indexes); [super dealloc]; }
- (id)copyWithZone:(NSZone *)zone { return [self retain]; }
- (NSUInteger)length { return _length; }
- (NSUInteger)indexAtPosition:(NSUInteger)position { return position < _length ? _indexes[position] : NSNotFound; }
- (void)getIndexes:(NSUInteger *)indexes { if (_length) memcpy(indexes, _indexes, _length * sizeof(NSUInteger)); }
- (void)getIndexes:(NSUInteger *)indexes range:(NSRange)range {
    for (NSUInteger i = 0; i < range.length; i++)
        indexes[i] = [self indexAtPosition:range.location + i];
}
- (NSIndexPath *)indexPathByAddingIndex:(NSUInteger)index {
    NSUInteger *ix = malloc((_length + 1) * sizeof(NSUInteger));
    if (_length) memcpy(ix, _indexes, _length * sizeof(NSUInteger));
    ix[_length] = index;
    NSIndexPath *p = [NSIndexPath indexPathWithIndexes:ix length:_length + 1];
    free(ix);
    return p;
}
- (NSIndexPath *)indexPathByRemovingLastIndex { return [NSIndexPath indexPathWithIndexes:_indexes length:_length ? _length - 1 : 0]; }
- (NSInteger)item { return _length > 1 ? (NSInteger)_indexes[1] : NSNotFound; }
- (NSInteger)row { return [self item]; }
- (NSInteger)section { return _length > 0 ? (NSInteger)_indexes[0] : NSNotFound; }
- (NSComparisonResult)compare:(NSIndexPath *)other {
    NSUInteger n = MIN(_length, [other length]);
    for (NSUInteger i = 0; i < n; i++) {
        NSUInteger a = _indexes[i], b = [other indexAtPosition:i];
        if (a != b)
            return a < b ? NSOrderedAscending : NSOrderedDescending;
    }
    return _length == [other length] ? NSOrderedSame : (_length < [other length] ? NSOrderedAscending : NSOrderedDescending);
}
- (BOOL)isEqual:(id)other { return other == self || ([other isKindOfClass:[NSIndexPath class]] && [self compare:other] == NSOrderedSame); }
- (NSUInteger)hash {
    NSUInteger h = _length;
    for (NSUInteger i = 0; i < _length; i++)
        h = h * 31 + _indexes[i];
    return h;
}
- (void)encodeWithCoder:(NSCoder *)coder {
    [coder encodeInteger:(NSInteger)_length forKey:@"NSIndexPathLength"];
    if (_length == 1)
        [coder encodeInteger:(NSInteger)_indexes[0] forKey:@"NSIndexPathValue"];
    else if (_length > 1)
        [coder encodeObject:[NSData dataWithBytes:_indexes length:_length * sizeof(NSUInteger)] forKey:@"NSIndexPathData"];
}
- (id)initWithCoder:(NSCoder *)coder {
    NSUInteger n = (NSUInteger)[coder decodeIntegerForKey:@"NSIndexPathLength"];
    if (n == 1) {
        NSUInteger v = (NSUInteger)[coder decodeIntegerForKey:@"NSIndexPathValue"];
        return [self initWithIndexes:&v length:1];
    }
    NSData *d = [coder decodeObjectForKey:@"NSIndexPathData"];
    return [self initWithIndexes:[d bytes] length:MIN(n, [d length] / sizeof(NSUInteger))];
}
- (NSString *)description {
    NSMutableString *s = [NSMutableString stringWithFormat:@"<NSIndexPath %p> {length = %lu, path = ", self, (unsigned long)_length];
    for (NSUInteger i = 0; i < _length; i++)
        [s appendFormat:i ? @" - %lu" : @"%lu", (unsigned long)_indexes[i]];
    [s appendString:@"}"];
    return s;
}
@end

/* ---- NSDateComponentsFormatter ---- */

enum { SFUnitsPositional = 0, SFUnitsAbbreviated = 1, SFUnitsShort = 2, SFUnitsFull = 3, SFUnitsSpellOut = 4, SFUnitsBrief = 5 };
enum { SFZeroDefault = 0, SFZeroDropLeading = 1, SFZeroDropMiddle = 2, SFZeroDropTrailing = 4, SFZeroDropAll = 14, SFZeroPad = 0x10000 };
enum { SFCalDay = 16, SFCalHour = 32, SFCalMinute = 64, SFCalSecond = 128 };

EXPORT
@interface NSDateComponentsFormatter : NSFormatter {
    NSInteger _unitsStyle;
    NSUInteger _allowedUnits;
    NSUInteger _zeroFormattingBehavior;
    NSInteger _maximumUnitCount;
    NSInteger _formattingContext;
    BOOL _allowsFractionalUnits, _collapsesLargestUnit, _includesApproximationPhrase, _includesTimeRemainingPhrase;
    NSCalendar *_calendar;
}
@end

@implementation NSDateComponentsFormatter
- (id)init {
    if ((self = [super init]) != nil)
        _allowedUnits = SFCalHour | SFCalMinute | SFCalSecond;
    return self;
}
- (void)dealloc { [_calendar release]; [super dealloc]; }
- (NSInteger)unitsStyle { return _unitsStyle; }
- (void)setUnitsStyle:(NSInteger)v { _unitsStyle = v; }
- (NSUInteger)allowedUnits { return _allowedUnits; }
- (void)setAllowedUnits:(NSUInteger)v { _allowedUnits = v; }
- (NSUInteger)zeroFormattingBehavior { return _zeroFormattingBehavior; }
- (void)setZeroFormattingBehavior:(NSUInteger)v { _zeroFormattingBehavior = v; }
- (NSInteger)maximumUnitCount { return _maximumUnitCount; }
- (void)setMaximumUnitCount:(NSInteger)v { _maximumUnitCount = v; }
- (NSInteger)formattingContext { return _formattingContext; }
- (void)setFormattingContext:(NSInteger)v { _formattingContext = v; }
- (BOOL)allowsFractionalUnits { return _allowsFractionalUnits; }
- (void)setAllowsFractionalUnits:(BOOL)v { _allowsFractionalUnits = v; }
- (BOOL)collapsesLargestUnit { return _collapsesLargestUnit; }
- (void)setCollapsesLargestUnit:(BOOL)v { _collapsesLargestUnit = v; }
- (BOOL)includesApproximationPhrase { return _includesApproximationPhrase; }
- (void)setIncludesApproximationPhrase:(BOOL)v { _includesApproximationPhrase = v; }
- (BOOL)includesTimeRemainingPhrase { return _includesTimeRemainingPhrase; }
- (void)setIncludesTimeRemainingPhrase:(BOOL)v { _includesTimeRemainingPhrase = v; }
- (NSCalendar *)calendar { return _calendar; }
- (void)setCalendar:(NSCalendar *)c { [c retain]; [_calendar release]; _calendar = c; }

- (NSString *)stringFromTimeInterval:(NSTimeInterval)t {
    if (t != t)
        return nil;
    BOOL negative = t < 0;
    double rest = negative ? -t : t;
    NSUInteger units[4] = { SFCalDay, SFCalHour, SFCalMinute, SFCalSecond };
    double sizes[4] = { 86400, 3600, 60, 1 };
    const char *abbr[4] = { "d", "h", "m", "s" }, *shrt[4] = { "day", "hr", "min", "sec" }, *full[4] = { "day", "hour", "minute", "second" };
    long long value[4] = { 0, 0, 0, 0 };
    BOOL used[4] = { NO, NO, NO, NO };
    int first = -1, last = -1;
    for (int i = 0; i < 4; i++)
        if (_allowedUnits & units[i]) {
            used[i] = YES;
            if (first < 0) first = i;
            last = i;
        }
    if (first < 0)
        return nil;
    for (int i = 0; i < 4; i++) {
        if (!used[i])
            continue;
        if (i == last) {
            value[i] = (long long)llround(rest / sizes[i]);
        } else {
            value[i] = (long long)(rest / sizes[i]);
            rest -= (double)value[i] * sizes[i];
        }
    }
    NSMutableString *s = [NSMutableString string];
    if (negative)
        [s appendString:@"-"];
    if (_unitsStyle == SFUnitsPositional) {
        int start = first;
        if (!(_zeroFormattingBehavior & SFZeroPad))
            while (start < last - 1 && value[start] == 0 && start < 2)   /* "1:05", not "0:01:05" */
                start++;
        BOOL firstPrinted = YES;
        for (int i = start; i <= last; i++) {
            if (!used[i])
                continue;
            if (firstPrinted)
                [s appendFormat:@"%lld", value[i]];
            else
                [s appendFormat:@":%02lld", value[i]];
            firstPrinted = NO;
        }
        return s;
    }
    int printed = 0;
    for (int i = first; i <= last; i++) {
        if (!used[i])
            continue;
        BOOL zero = value[i] == 0;
        BOOL drop = (_zeroFormattingBehavior == SFZeroDefault || (_zeroFormattingBehavior & SFZeroDropAll)) && zero && !(i == last && printed == 0);
        if (drop)
            continue;
        if (_maximumUnitCount > 0 && printed >= _maximumUnitCount)
            break;
        if (printed)
            [s appendString:_unitsStyle == SFUnitsFull || _unitsStyle == SFUnitsShort ? @", " : @" "];
        switch (_unitsStyle) {
        case SFUnitsAbbreviated:
        case SFUnitsBrief:
            [s appendFormat:@"%lld%s", value[i], abbr[i]];
            break;
        case SFUnitsShort:
            [s appendFormat:@"%lld %s%s", value[i], shrt[i], value[i] == 1 || i == 0 ? "" : "s"];
            break;
        default:
            [s appendFormat:@"%lld %s%s", value[i], full[i], value[i] == 1 ? "" : "s"];
        }
        printed++;
    }
    if (_includesTimeRemainingPhrase)
        [s appendString:@" remaining"];
    if (_includesApproximationPhrase)
        [s insertString:@"About " atIndex:0];
    return s;
}

- (NSString *)stringFromDateComponents:(NSDateComponents *)c {
    double t = 0;
    if ([c day] != NSUndefinedDateComponent) t += 86400.0 * [c day];
    if ([c hour] != NSUndefinedDateComponent) t += 3600.0 * [c hour];
    if ([c minute] != NSUndefinedDateComponent) t += 60.0 * [c minute];
    if ([c second] != NSUndefinedDateComponent) t += [c second];
    return [self stringFromTimeInterval:t];
}

- (NSString *)stringFromDate:(NSDate *)a toDate:(NSDate *)b { return [self stringFromTimeInterval:[b timeIntervalSinceDate:a]]; }

- (NSString *)stringForObjectValue:(id)obj {
    if ([obj isKindOfClass:[NSNumber class]])
        return [self stringFromTimeInterval:[obj doubleValue]];
    if ([obj isKindOfClass:[NSDateComponents class]])
        return [self stringFromDateComponents:obj];
    return nil;
}

- (BOOL)getObjectValue:(id *)obj forString:(NSString *)string errorDescription:(NSString **)error {
    if (error) *error = @"NSDateComponentsFormatter does not parse";
    return NO;
}
@end

/* ---- methods libFoundation lacks ---- */

@implementation NSString (SFActivityMonitor)
- (void)enumerateLinesUsingBlock:(void (^)(NSString *line, BOOL *stop))block {
    NSUInteger length = [self length], start = 0;
    BOOL stop = NO;
    while (start < length && !stop) {
        NSUInteger lineStart, lineEnd, contentsEnd;
        [self getLineStart:&lineStart end:&lineEnd contentsEnd:&contentsEnd forRange:NSMakeRange(start, 0)];
        block([self substringWithRange:NSMakeRange(lineStart, contentsEnd - lineStart)], &stop);
        if (lineEnd <= start)
            break;
        start = lineEnd;
    }
}
- (NSComparisonResult)localizedStandardCompare:(NSString *)other {
    return [self compare:other options:NSCaseInsensitiveSearch | NSNumericSearch range:NSMakeRange(0, [self length])];
}
@end

@implementation NSLocale (SFActivityMonitor)
+ (id)autoupdatingCurrentLocale { return [self currentLocale]; }
@end

@implementation NSFileManager (SFActivityMonitor)
- (NSArray *)contentsOfDirectoryAtURL:(NSURL *)url includingPropertiesForKeys:(NSArray *)keys options:(NSUInteger)options error:(NSError **)error {
    NSArray *names = [self contentsOfDirectoryAtPath:[url path] error:error];
    if (names == nil)
        return nil;
    NSMutableArray *out = [NSMutableArray arrayWithCapacity:[names count]];
    for (NSString *n in names) {
        if ((options & 4 /* NSDirectoryEnumerationSkipsHiddenFiles */) && [n hasPrefix:@"."])
            continue;
        [out addObject:[url URLByAppendingPathComponent:n]];
    }
    return out;
}
@end

static const char sf_readability_key;

@implementation NSFileHandle (SFActivityMonitor)
- (void)setReadabilityHandler:(void (^)(NSFileHandle *))handler {
    dispatch_source_t old = objc_getAssociatedObject(self, &sf_readability_key);
    if (old) {
        dispatch_source_cancel(old);
        objc_setAssociatedObject(self, &sf_readability_key, nil, OBJC_ASSOCIATION_RETAIN);
    }
    if (handler == nil)
        return;
    void (^h)(NSFileHandle *) = [[handler copy] autorelease];
    dispatch_source_t src = dispatch_source_create(DISPATCH_SOURCE_TYPE_READ, (uintptr_t)[self fileDescriptor], 0,
        dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0));
    NSFileHandle *me = self;
    dispatch_source_set_event_handler(src, ^{ h(me); });
    objc_setAssociatedObject(self, &sf_readability_key, (id)src, OBJC_ASSOCIATION_RETAIN);
    dispatch_resume(src);
    dispatch_release(src);
}
- (void (^)(NSFileHandle *))readabilityHandler { return nil; }
@end

static const char sf_minint_key, sf_round_key, sf_format_key, sf_grouping_key;

@implementation NSNumberFormatter (SFActivityMonitor)
- (void)setMinimumIntegerDigits:(NSUInteger)n { objc_setAssociatedObject(self, &sf_minint_key, [NSNumber numberWithUnsignedInteger:n], OBJC_ASSOCIATION_RETAIN); }
- (NSUInteger)minimumIntegerDigits { return [objc_getAssociatedObject(self, &sf_minint_key) unsignedIntegerValue]; }
- (void)setRoundingMode:(NSUInteger)m { objc_setAssociatedObject(self, &sf_round_key, [NSNumber numberWithUnsignedInteger:m], OBJC_ASSOCIATION_RETAIN); }
- (NSUInteger)roundingMode { return [objc_getAssociatedObject(self, &sf_round_key) unsignedIntegerValue]; }
- (void)setFormat:(NSString *)f { objc_setAssociatedObject(self, &sf_format_key, f, OBJC_ASSOCIATION_COPY); }
- (NSString *)format { return objc_getAssociatedObject(self, &sf_format_key); }
- (BOOL)usesGroupingSeparator {
    NSNumber *v = objc_getAssociatedObject(self, &sf_grouping_key);
    return v ? [v boolValue] : [self numberStyle] == NSNumberFormatterDecimalStyle;
}
- (void)setUsesGroupingSeparator:(BOOL)b { objc_setAssociatedObject(self, &sf_grouping_key, [NSNumber numberWithBool:b], OBJC_ASSOCIATION_RETAIN); }

/* As macOS: setting a style sets that style's fraction digits (decimal: 0 to 3; percent and none:
   none), which a later -setMinimum/MaximumFractionDigits: changes. libFoundation's setter only
   kept the style. */
- (void)sf_setNumberStyle:(NSNumberFormatterStyle)style {
    Ivar iv = class_getInstanceVariable([NSNumberFormatter class], "_numberStyle");
    if (iv)
        *(NSNumberFormatterStyle *)((char *)self + ivar_getOffset(iv)) = style;
    [self setMinimumFractionDigits:0];
    [self setMaximumFractionDigits:style == NSNumberFormatterDecimalStyle ? 3 : (style == NSNumberFormatterPercentStyle || style == NSNumberFormatterNoStyle ? 0 : 2)];
}

/* The styles a number is formatted in (libFoundation printed every style as "%.3f"): none (the
   integer), decimal (grouped in threes), percent (times a hundred, "%"), each with the minimum and
   maximum fraction digits and the minimum integer digits; the rest as decimal. English symbols. */
- (NSString *)sf_stringFromNumber:(NSNumber *)number {
    if (number == nil)
        return nil;
    NSNumberFormatterStyle style = [self numberStyle];
    double v = [number doubleValue];
    if (v != v)
        return @"NaN";
    if (style == NSNumberFormatterPercentStyle)
        v *= 100;
    NSUInteger minf = [self minimumFractionDigits], maxf = [self maximumFractionDigits];
    if (style == NSNumberFormatterNoStyle)
        minf = maxf = 0;
    if (minf > maxf)
        maxf = minf;
    if (maxf > 15)
        maxf = 15;
    BOOL negative = v < 0;
    char buf[64];
    snprintf(buf, sizeof(buf), "%.*f", (int)maxf, negative ? -v : v);
    char *dot = strchr(buf, '.');
    /* drop trailing zeros down to the minimum */
    if (dot) {
        char *end = buf + strlen(buf) - 1;
        while (end > dot && (NSUInteger)(end - dot) > minf && *end == '0')
            *end-- = '\0';
        if (end == dot)
            *end = '\0';
    }
    NSString *digits = [NSString stringWithUTF8String:buf];
    NSRange point = [digits rangeOfString:@"."];
    NSString *integer = point.location == NSNotFound ? digits : [digits substringToIndex:point.location];
    NSString *fraction = point.location == NSNotFound ? @"" : [digits substringFromIndex:point.location];
    NSUInteger minint = [self minimumIntegerDigits];
    while ([integer length] < minint)
        integer = [@"0" stringByAppendingString:integer];
    if ([self usesGroupingSeparator] && [integer length] > 3) {
        NSMutableString *g = [NSMutableString string];
        NSUInteger n = [integer length];
        for (NSUInteger i = 0; i < n; i++) {
            if (i > 0 && (n - i) % 3 == 0)
                [g appendString:@","];
            [g appendFormat:@"%C", [integer characterAtIndex:i]];
        }
        integer = g;
    }
    NSMutableString *out = [NSMutableString stringWithString:negative && ![[integer stringByAppendingString:fraction] isEqualToString:@"0"] ? @"-" : @""];
    [out appendString:integer];
    [out appendString:fraction];
    if (style == NSNumberFormatterPercentStyle)
        [out appendString:@"%"];
    return out;
}
@end

/* formatters' formatting context (capitalisation of a formatted string at the start of a sentence,
   in a list ...): kept; these formatters' strings do not depend on it in English */
static const char sf_context_key;
@implementation NSFormatter (SFFormattingContext)
- (NSInteger)formattingContext { return [objc_getAssociatedObject(self, &sf_context_key) integerValue]; }
- (void)setFormattingContext:(NSInteger)c { objc_setAssociatedObject(self, &sf_context_key, [NSNumber numberWithInteger:c], OBJC_ASSOCIATION_RETAIN); }
@end

/* libFoundation's own -setNumberStyle: and -stringFromNumber: are replaced by the two above when
   this library loads (a category method of the same name in this image did not take their place) */
@interface SFNumberFormatterMethods : NSObject
@end
@implementation SFNumberFormatterMethods
+ (void)load {
    Class c = [NSNumberFormatter class];
    SEL pairs[2][2] = { { @selector(setNumberStyle:), @selector(sf_setNumberStyle:) },
                        { @selector(stringFromNumber:), @selector(sf_stringFromNumber:) } };
    for (int i = 0; i < 2; i++) {
        Method original = class_getInstanceMethod(c, pairs[i][0]), ours = class_getInstanceMethod(c, pairs[i][1]);
        if (original && ours)
            method_setImplementation(original, method_getImplementation(ours));
    }
}
@end
