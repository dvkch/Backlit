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

@implementation NSError (SY)

+ (instancetype)sy_errorWithCode:(SYErrorCode)code
{
    return [self sy_errorWithCode:code customMessage:nil];
}

+ (instancetype)sy_errorWithSaneStatus:(SANE_Status)saneStatus
{
    NSString *statusString = [NSString stringWithCString:sane_strstatus(saneStatus)
                                                encoding:NSUTF8StringEncoding];
    
    return [self sy_errorWithCode:SYErrorCode_SaneError customMessage:statusString];
}

+ (instancetype)sy_errorWithCode:(SYErrorCode)code customMessage:(NSString *)customMessage
{
    NSMutableDictionary <NSString *, id> *userInfo = [NSMutableDictionary dictionary];
    userInfo[NSLocalizedDescriptionKey] = customMessage ?: [self sy_localizedDescriptionForCode:code];
    userInfo[SYLocalizedTitleKey]       = [self sy_localizedTitleForCode:code];
    
    return [NSError errorWithDomain:SYErrorDomain
                               code:code
                           userInfo:[userInfo copy]];
}

// TODO: translate messages

+ (NSString *)sy_localizedDescriptionForCode:(SYErrorCode)code
{
    switch (code) {
        case SYErrorCode_SaneError:
            return $("ERROR MESSAGE NO INTERNET");
        case SYErrorCode_UserCancelled:
            return $("ERROR MESSAGE EMPTY FIELD");
        case SYErrorCode_DeviceNotOpened:
            return $("ERROR MESSAGE PASSWORD DONT MATCH");
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
    }
    return nil;
}

- (BOOL)sy_isErrorWithCode:(SYErrorCode)code
{
    return [self.domain isEqualToString:SYErrorDomain] && (self.code == code);
}

- (BOOL)sy_isCancellation
{
    return [self sy_isErrorWithCode:SYErrorCode_UserCancelled];
}

- (NSError *)sy_equivalentError
{
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
