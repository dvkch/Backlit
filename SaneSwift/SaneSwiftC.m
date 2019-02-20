//
//  SaneSwiftC.m
//  SaneScanner
//
//  Created by rominet on 06/05/15.
//  Copyright (c) 2015 Syan. All rights reserved.
//

#import "SaneSwiftC.h"
#import "sane.h"

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
