//
//  SVProgressHUD+SY.m
//  SaneScanner
//
//  Created by Stanislas Chevallier on 31/01/2017.
//  Copyright © 2017 Syan. All rights reserved.
//

#import "SVProgressHUD+SY.h"

@implementation SVProgressHUD (SY)

+ (void)showSuccessWithStatus:(NSString*)status duration:(NSTimeInterval)duration
{
    [self showSuccessWithStatus:status];
    [self dismissWithDelay:duration];
}

@end
