//
//  SaneSwiftC.m
//  SaneScanner
//
//  Created by rominet on 06/05/15.
//  Copyright (c) 2015 Syan. All rights reserved.
//

#import "SaneSwiftC.h"
#import "sane.h"
#import "saneopts.h"

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
