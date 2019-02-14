//
//  SYSaneOption.h
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

NSString *NSStringFromSANEStatus(SANE_Status status) {
    // TODO: swift ext?
    return [NSString stringWithCString:sane_strstatus(status) encoding:NSUTF8StringEncoding];
}

static SaneAuthBlock kSaneAuthBlock = NULL;
void SaneSetAuthBlock(SaneAuthBlock block) {
    kSaneAuthBlock = [block copy];
}

void SaneAuthCallBack(SANE_String_Const resource, SANE_Char *username, SANE_Char *password) {
    // TODO: handle max length
    if (kSaneAuthBlock != NULL) {
        NSString *user = nil;
        NSString *pass = nil;
        
        kSaneAuthBlock([NSString stringWithCString:resource encoding:NSUTF8StringEncoding], &user, &pass);
        
        if (user != NULL && username != NULL) {
            *username = [user cStringUsingEncoding:NSUTF8StringEncoding];
        }
        
        if (pass != NULL && password != NULL) {
            *password = [pass cStringUsingEncoding:NSUTF8StringEncoding];
        }
    }
}

SANE_Word SaneFixedFromDouble(double value) {
    return SANE_FIX(value);
}

NSString * _Nullable NSStringFromSaneString(SANE_String_Const _Nullable cString) {
    if (cString == NULL) {
        return nil;
    }
    return [[NSString alloc] initWithCString:cString encoding:NSUTF8StringEncoding];
}
