//
//  SaneSwiftC.h
//  SaneScanner
//
//  Created by rominet on 06/05/15.
//  Copyright (c) 2015 Syan. All rights reserved.
//

#import <Foundation/Foundation.h>
#import "sane.h"

FOUNDATION_EXPORT void SaneSetLogLevel(int logLevel);
FOUNDATION_EXPORT int SaneGetLogLevel(void);

FOUNDATION_EXPORT NSString *NSStringFromSANEStatus(SANE_Status status);

FOUNDATION_EXPORT SANE_Word SaneFixedFromDouble(double value);
FOUNDATION_EXPORT double SaneDoubleFromFixed(SANE_Word value);

FOUNDATION_EXPORT NSString * _Nullable NSStringFromSaneString(SANE_String_Const _Nullable cString);
