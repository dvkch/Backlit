//
//  NSError+SY.h
//  SaneScanner
//
//  Created by Stan Chevallier on 04/03/2016.
//  Copyright © 2016 Syan. All rights reserved.
//

#import <Foundation/Foundation.h>
#import "sane.h"

extern NSString * const SYErrorDomain;

typedef enum : NSUInteger {
    SYErrorCode_SaneError,
    SYErrorCode_UserCancelled,
    SYErrorCode_DeviceNotOpened,
    SYErrorCode_GetValueForTypeButton,
    SYErrorCode_GetValueForTypeGroup,
    SYErrorCode_GetValueForInactiveOption,
    SYErrorCode_SetValueForTypeGroup,
    SYErrorCode_NoImageData,
    SYErrorCode_CannotAllocateColorSpace,
    SYErrorCode_CannotAllocateSourceImageRef,
    SYErrorCode_CannotAllocateMemoryForBitmap,
    SYErrorCode_CannotCreateImageContext,
    SYErrorCode_UnsupportedChannels,
} SYErrorCode;

@interface NSError (SY)

+ (instancetype)sy_errorWithCode:(SYErrorCode)code;
+ (instancetype)sy_errorWithSaneStatus:(SANE_Status)saneStatus;

- (BOOL)sy_isErrorWithCode:(SYErrorCode)code;

- (BOOL)sy_isCancellation;

- (NSString *)sy_alertTitle;
- (NSString *)sy_alertMessage;

@end
