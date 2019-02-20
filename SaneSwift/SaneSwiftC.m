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
