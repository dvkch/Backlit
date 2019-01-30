//
//  NSError+SY.m
//  SaneScanner
//
//  Created by Stan Chevallier on 04/03/2016.
//  Copyright © 2016 Syan. All rights reserved.
//

#import "NSError+SY.h"

NSString * const SSErrorDomain = @"SYErrorDomain";

static NSString * const SSLocalizedTitleKey = @"SYLocalizedTitleKey";
static NSString * const SSSaneStatusKey     = @"SYSaneStatusKey";

@implementation NSError (SS)

+ (instancetype)ss_errorWithCode:(SSErrorCode)code
{
    NSMutableDictionary <NSString *, id> *userInfo = [NSMutableDictionary dictionary];
    userInfo[NSLocalizedDescriptionKey] = [self ss_localizedDescriptionForCode:code];
    userInfo[SSLocalizedTitleKey]       = [self ss_localizedTitleForCode:code];
    
    return [NSError errorWithDomain:SSErrorDomain
                               code:code
                           userInfo:[userInfo copy]];
}

+ (instancetype)ss_errorWithSaneStatus:(SANE_Status)saneStatus
{
    NSString *statusString = [NSString stringWithCString:sane_strstatus(saneStatus)
                                                encoding:NSUTF8StringEncoding];
    
    NSMutableDictionary <NSString *, id> *userInfo = [NSMutableDictionary dictionary];
    userInfo[NSLocalizedDescriptionKey] = statusString;
    userInfo[SSLocalizedTitleKey]       = [self ss_localizedTitleForCode:SSErrorCode_SaneError];
    userInfo[SSSaneStatusKey]           = @(saneStatus);
    
    return [NSError errorWithDomain:SSErrorDomain
                               code:SSErrorCode_SaneError
                           userInfo:[userInfo copy]];
}

+ (NSString *)ss_localizedDescriptionForCode:(SSErrorCode)code
{
    switch (code) {
            // TODO: translate
        case SSErrorCode_SaneError:
            return @"ERROR MESSAGE SANE ERROR";
        case SSErrorCode_UserCancelled:
            return @"ERROR MESSAGE USER CANCELLED";
        case SSErrorCode_DeviceNotOpened:
            return @"ERROR MESSAGE DEVICE NOT OPENED";
        case SSErrorCode_GetValueForTypeButton:
            return @"ERROR MESSAGE GET VALUE TYPE BUTTON";
        case SSErrorCode_GetValueForTypeGroup:
            return @"ERROR MESSAGE GET VALUE TYPE GROUP";
        case SSErrorCode_GetValueForInactiveOption:
            return @"ERROR MESSAGE GET VALUE INACTIVE OPTION";
        case SSErrorCode_SetValueForTypeGroup:
            return @"ERROR MESSAGE SET VALUE TYPE GROUP";
        case SSErrorCode_NoImageData:
            return @"ERROR MESSAGE NO IMAGE DATA";
        case SSErrorCode_CannotGenerateImage:
            return @"ERROR MESSAGE CANNOT GENERATE IMAGE";
        case SSErrorCode_UnsupportedChannels:
            return @"ERROR MESSAGE UNSUPPORTED CHANNELS";
    }
    return nil;
}

+ (NSString *)ss_localizedTitleForCode:(SSErrorCode)code
{
    switch (code)
    {
        case SSErrorCode_SaneError:
            return nil;
        case SSErrorCode_UserCancelled:
            return nil;
        case SSErrorCode_DeviceNotOpened:
            return nil;
        case SSErrorCode_GetValueForTypeButton:
            return nil;
        case SSErrorCode_GetValueForTypeGroup:
            return nil;
        case SSErrorCode_GetValueForInactiveOption:
            return nil;
        case SSErrorCode_SetValueForTypeGroup:
            return nil;
        case SSErrorCode_NoImageData:
            return nil;
        case SSErrorCode_CannotGenerateImage:
            return nil;
        case SSErrorCode_UnsupportedChannels:
            return nil;
    }
    return nil;
}

- (BOOL)ss_isErrorWithCode:(SSErrorCode)code
{
    return [self.domain isEqualToString:SSErrorDomain] && (self.code == code);
}

- (BOOL)ss_isCancellation
{
    if ([self ss_isErrorWithCode:SSErrorCode_SaneError])
        return [self.userInfo[SSSaneStatusKey] intValue] == SANE_STATUS_CANCELLED;
    
    return [self ss_isErrorWithCode:SSErrorCode_UserCancelled];
}

- (NSError *)ss_equivalentError
{
    if ([self ss_isCancellation])
        return [[self class] ss_errorWithCode:SSErrorCode_UserCancelled];
    
    return self;
}

- (NSString *)ss_alertTitle
{
    // TODO: translate
    return [self ss_equivalentError].userInfo[SSLocalizedTitleKey] ?: @"ERROR TITLE GENERIC";
}

- (NSString *)ss_alertMessage
{
    return [[self ss_equivalentError] localizedDescription];
}

@end
