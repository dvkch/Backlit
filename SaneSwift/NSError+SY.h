//
//  NSError+SY.h
//  SaneScanner
//
//  Created by Stan Chevallier on 04/03/2016.
//  Copyright © 2016 Syan. All rights reserved.
//

#import <Foundation/Foundation.h>
#import "sane.h"

extern NSString * const SSErrorDomain;

typedef enum : NSUInteger {
    SSErrorCode_SaneError,
    SSErrorCode_UserCancelled,
    SSErrorCode_DeviceNotOpened,
    SSErrorCode_GetValueForTypeButton,
    SSErrorCode_GetValueForTypeGroup,
    SSErrorCode_GetValueForInactiveOption,
    SSErrorCode_SetValueForTypeGroup,
    SSErrorCode_NoImageData,
    SSErrorCode_CannotGenerateImage,
    SSErrorCode_UnsupportedChannels,
} SSErrorCode;

@interface NSError (SS)

+ (instancetype)ss_errorWithCode:(SSErrorCode)code;
+ (instancetype)ss_errorWithSaneStatus:(SANE_Status)saneStatus;

- (BOOL)ss_isErrorWithCode:(SSErrorCode)code;

- (BOOL)ss_isCancellation;

- (NSString *)ss_alertTitle;
- (NSString *)ss_alertMessage;

@end
