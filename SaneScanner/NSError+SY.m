//
//  NSError+SY.m
//  SaneScanner
//
//  Created by Stan Chevallier on 04/03/2016.
//  Copyright © 2016 Syan. All rights reserved.
//

#import "NSError+SY.h"

NSString * const SYErrorDomain = $$("SYErrorDomain");

static NSString * const SYLocalizedTitleKey = $$("SYLocalizedTitleKey");
static NSString * const SYSaneStatusKey     = $$("SYSaneStatusKey");

@implementation NSError (SY)

+ (instancetype)sy_errorWithCode:(SYErrorCode)code
{
    NSMutableDictionary <NSString *, id> *userInfo = [NSMutableDictionary dictionary];
    userInfo[NSLocalizedDescriptionKey] = [self sy_localizedDescriptionForCode:code];
    userInfo[SYLocalizedTitleKey]       = [self sy_localizedTitleForCode:code];
    
    return [NSError errorWithDomain:SYErrorDomain
                               code:code
                           userInfo:[userInfo copy]];
}

+ (instancetype)sy_errorWithSaneStatus:(SANE_Status)saneStatus
{
    NSString *statusString = [NSString stringWithCString:sane_strstatus(saneStatus)
                                                encoding:NSUTF8StringEncoding];
    
    NSMutableDictionary <NSString *, id> *userInfo = [NSMutableDictionary dictionary];
    userInfo[NSLocalizedDescriptionKey] = statusString;
    userInfo[SYLocalizedTitleKey]       = [self sy_localizedTitleForCode:SYErrorCode_SaneError];
    userInfo[SYSaneStatusKey]           = @(saneStatus);
    
    return [NSError errorWithDomain:SYErrorDomain
                               code:SYErrorCode_SaneError
                           userInfo:[userInfo copy]];
}

+ (NSString *)sy_localizedDescriptionForCode:(SYErrorCode)code
{
    switch (code) {
        case SYErrorCode_SaneError:
            return $("ERROR MESSAGE SANE ERROR");
        case SYErrorCode_UserCancelled:
            return $("ERROR MESSAGE USER CANCELLED");
        case SYErrorCode_DeviceNotOpened:
            return $("ERROR MESSAGE DEVICE NOT OPENED");
        case SYErrorCode_GetValueForTypeButton:
            return $("ERROR MESSAGE GET VALUE TYPE BUTTON");
        case SYErrorCode_GetValueForTypeGroup:
            return $("ERROR MESSAGE GET VALUE TYPE GROUP");
        case SYErrorCode_GetValueForInactiveOption:
            return $("ERROR MESSAGE GET VALUE INACTIVE OPTION");
        case SYErrorCode_SetValueForTypeGroup:
            return $("ERROR MESSAGE SET VALUE TYPE GROUP");
        case SYErrorCode_NoImageData:
            return $("ERROR MESSAGE NO IMAGE DATA");
        case SYErrorCode_CannotGenerateImage:
            return $("ERROR MESSAGE CANNOT GENERATE IMAGE");
        case SYErrorCode_UnsupportedChannels:
            return $("ERROR MESSAGE UNSUPPORTED CHANNELS");
    }
    return nil;
}

+ (NSString *)sy_localizedTitleForCode:(SYErrorCode)code
{
    switch (code)
    {
        case SYErrorCode_SaneError:
            return nil;
        case SYErrorCode_UserCancelled:
            return nil;
        case SYErrorCode_DeviceNotOpened:
            return nil;
        case SYErrorCode_GetValueForTypeButton:
            return nil;
        case SYErrorCode_GetValueForTypeGroup:
            return nil;
        case SYErrorCode_GetValueForInactiveOption:
            return nil;
        case SYErrorCode_SetValueForTypeGroup:
            return nil;
        case SYErrorCode_NoImageData:
            return nil;
        case SYErrorCode_CannotGenerateImage:
            return nil;
        case SYErrorCode_UnsupportedChannels:
            return nil;
    }
    return nil;
}

- (BOOL)sy_isErrorWithCode:(SYErrorCode)code
{
    return [self.domain isEqualToString:SYErrorDomain] && (self.code == code);
}

- (BOOL)sy_isCancellation
{
    if ([self sy_isErrorWithCode:SYErrorCode_SaneError])
        return [self.userInfo[SYSaneStatusKey] intValue] == SANE_STATUS_CANCELLED;
    
    return [self sy_isErrorWithCode:SYErrorCode_UserCancelled];
}

- (NSError *)sy_equivalentError
{
    if ([self sy_isCancellation])
        return [[self class] sy_errorWithCode:SYErrorCode_UserCancelled];
    
    return self;
}

- (NSString *)sy_alertTitle
{
    return [self sy_equivalentError].userInfo[SYLocalizedTitleKey] ?: $("ERROR TITLE GENERIC");
}

- (NSString *)sy_alertMessage
{
    return [[self sy_equivalentError] localizedDescription];
}

@end
