//
//  SaneSwiftC.m
//  SaneScanner
//
//  Created by rominet on 06/05/15.
//  Copyright (c) 2015 Syan. All rights reserved.
//

#import "SaneSwiftC.h"

// reimport symbol from sane-net.a
extern int sanei_debug_net;

void SaneSetLogLevel(int logLevel) {
    sanei_debug_net = logLevel;
}

int SaneGetLogLevel(void) {
    return sanei_debug_net;
}

SANE_Word SaneFixedFromDouble(double value) {
    return SANE_FIX(value);
}

double SaneDoubleFromFixed(SANE_Word value) {
    return SANE_UNFIX(value);
}

typedef struct {
    UInt8 b7;
    UInt8 b6;
    UInt8 b5;
    UInt8 b4;
    UInt8 b3;
    UInt8 b2;
    UInt8 b1;
    UInt8 b0;
} Bits;

Bits bitsFromByte(UInt8 byte)
{
    return (Bits) {
        .b7 = byte & 0b10000000 >> 7,
        .b6 = byte & 0b01000000 >> 6,
        .b5 = byte & 0b00100000 >> 5,
        .b4 = byte & 0b00010000 >> 4,
        .b3 = byte & 0b00001000 >> 3,
        .b2 = byte & 0b00000100 >> 2,
        .b1 = byte & 0b00000010 >> 1,
        .b0 = byte & 0b00000001 >> 0
    };
}

UInt8 byteFromBits(UInt8 b7, UInt8 b6, UInt8 b5, UInt8 b4, UInt8 b3, UInt8 b2, UInt8 b1, UInt8 b0)
{
    return (b0 << 0) + (b1 << 1) + (b2 << 2) + (b3 << 3) + (b4 << 4) + (b5 << 5) + (b6 << 6) + (b7 << 7);
}

void unpackPixels(UInt8* _Nonnull r, UInt8* _Nonnull g, UInt8* _Nonnull b)
{
    Bits rBits = bitsFromByte(*r);
    Bits gBits = bitsFromByte(*g);
    Bits bBits = bitsFromByte(*b);
    
    UInt8 byte0 = byteFromBits(rBits.b0, gBits.b0, bBits.b0, rBits.b1, gBits.b1, bBits.b1, rBits.b2, gBits.b2);
    UInt8 byte1 = byteFromBits(bBits.b2, rBits.b3, gBits.b3, bBits.b3, rBits.b4, gBits.b4, bBits.b4, rBits.b5);
    UInt8 byte2 = byteFromBits(gBits.b5, bBits.b5, rBits.b6, gBits.b6, bBits.b6, rBits.b7, gBits.b7, bBits.b7);
    
    *r = byte0;
    *g = byte1;
    *b = byte2;
}

NSOperatingSystemVersion SaneVersionFromInt(SANE_Int version) {
    NSOperatingSystemVersion v = {};
    v.majorVersion = SANE_VERSION_MAJOR(version);
    v.minorVersion = SANE_VERSION_MINOR(version);
    v.patchVersion = SANE_VERSION_BUILD(version);
    return v;
}

NSString * _Nullable NSStringFromSaneValueScanMode(SaneValueScanMode value) {
    // we do this in ObjC because SANE_I18N() macro doesn't expand properly and is not available in Swift..
    switch (value) {
        case SaneValueScanModeColor:            return [NSString stringWithCString:SANE_VALUE_SCAN_MODE_COLOR           encoding:NSUTF8StringEncoding];
        case SaneValueScanModeColorLineart:     return [NSString stringWithCString:SANE_VALUE_SCAN_MODE_COLOR_LINEART   encoding:NSUTF8StringEncoding];
        case SaneValueScanModeColorHalftone:    return [NSString stringWithCString:SANE_VALUE_SCAN_MODE_COLOR_HALFTONE  encoding:NSUTF8StringEncoding];
        case SaneValueScanModeGray:             return [NSString stringWithCString:SANE_VALUE_SCAN_MODE_GRAY            encoding:NSUTF8StringEncoding];
        case SaneValueScanModeHalftone:         return [NSString stringWithCString:SANE_VALUE_SCAN_MODE_HALFTONE        encoding:NSUTF8StringEncoding];
        case SaneValueScanModeLineart:          return [NSString stringWithCString:SANE_VALUE_SCAN_MODE_LINEART         encoding:NSUTF8StringEncoding];
    }
    return nil;
}

@implementation NSObject (SaneSwift)

- (void)saneSwift_performBlock:(void(^ _Nullable)(void))block onThread:(NSThread  * _Nonnull)thread
{
    if (thread == [NSThread currentThread])
    {
        if (block)
            block();
        return;
    }
    
    [self performSelector:@selector(saneSwift_runBlock:) onThread:thread withObject:block waitUntilDone:NO];
}

- (void)saneSwift_runBlock:(void(^ _Nullable)(void))block
{
    if (block)
        block();
}

@end
