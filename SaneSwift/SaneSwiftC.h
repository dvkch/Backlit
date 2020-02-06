//
//  SaneSwiftC.h
//  SaneScanner
//
//  Created by rominet on 06/05/15.
//  Copyright (c) 2015 Syan. All rights reserved.
//

#import <Foundation/Foundation.h>
#import "sane.h"
#import "saneopts.h"

FOUNDATION_EXPORT void SaneSetLogLevel(int logLevel);
FOUNDATION_EXPORT int SaneGetLogLevel(void);

FOUNDATION_EXPORT SANE_Word SaneFixedFromDouble(double value);
FOUNDATION_EXPORT double SaneDoubleFromFixed(SANE_Word value);

typedef NS_ENUM(NSUInteger, SaneValueScanMode) {
    SaneValueScanModeColor,
    SaneValueScanModeColorLineart,
    SaneValueScanModeColorHalftone,
    SaneValueScanModeGray,
    SaneValueScanModeHalftone,
    SaneValueScanModeLineart
};

FOUNDATION_EXPORT NSString * _Nullable NSStringFromSaneValueScanMode(SaneValueScanMode value);

@interface NSObject (SaneSwift)

- (void)saneSwift_performBlock:(void(^ _Nullable)(void))block onThread:(NSThread  * _Nonnull)thread;

@end
