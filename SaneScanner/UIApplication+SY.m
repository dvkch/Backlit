//
//  UIApplication+SY.m
//  SaneScanner
//
//  Created by Stanislas Chevallier on 29/01/2017.
//  Copyright © 2017 Syan. All rights reserved.
//

#import "UIApplication+SY.h"

@implementation UIApplication (SY)

- (NSString *)sy_localizedName
{
    return [[NSBundle mainBundle] objectForInfoDictionaryKey:(NSString *)kCFBundleNameKey];
}

+ (NSString *)sy_appVersion
{
    return [[[NSBundle mainBundle] infoDictionary] objectForKey:$$("CFBundleShortVersionString")];
}

+ (NSString *)sy_buildVersion
{
    return [[[NSBundle mainBundle] infoDictionary] objectForKey:$$("CFBundleVersion")];
}

+ (NSString *)sy_appVersionAndBuild
{
    return [NSString stringWithFormat:$$("%@ (%@)"),
            [self sy_appVersion],
            [self sy_buildVersion]];
}

@end
