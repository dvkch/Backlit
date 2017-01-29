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

@end
