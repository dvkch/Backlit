//
//  UIApplication+SY.h
//  SaneScanner
//
//  Created by Stanislas Chevallier on 29/01/2017.
//  Copyright © 2017 Syan. All rights reserved.
//

#import <UIKit/UIKit.h>

@interface UIApplication (SY)

- (NSString *)sy_localizedName;
+ (NSString *)sy_appVersion;
+ (NSString *)sy_buildVersion;
+ (NSString *)sy_appVersionAndBuild;

@end
