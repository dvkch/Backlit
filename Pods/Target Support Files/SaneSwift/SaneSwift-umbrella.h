#ifdef __OBJC__
#import <UIKit/UIKit.h>
#else
#ifndef FOUNDATION_EXPORT
#if defined(__cplusplus)
#define FOUNDATION_EXPORT extern "C"
#else
#define FOUNDATION_EXPORT extern
#endif
#endif
#endif

#import "NSObject+SY.h"
#import "SaneSwiftC.h"
#import "SYSaneDevice.h"
#import "SYSaneOption.h"
#import "SYSaneOptionBool.h"
#import "SYSaneOptionButton.h"
#import "SYSaneOptionGroup.h"
#import "SYSaneOptionNumber.h"
#import "SYSaneOptionString.h"
#import "SYSaneScanParameters.h"
#import "sane.h"
#import "saneopts.h"

FOUNDATION_EXPORT double SaneSwiftVersionNumber;
FOUNDATION_EXPORT const unsigned char SaneSwiftVersionString[];

