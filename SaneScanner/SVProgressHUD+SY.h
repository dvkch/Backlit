//
//  SVProgressHUD+SY.h
//  SaneScanner
//
//  Created by Stanislas Chevallier on 31/01/2017.
//  Copyright © 2017 Syan. All rights reserved.
//

#import <SVProgressHUD/SVProgressHUD.h>

@interface SVProgressHUD (SY)

+ (void)showSuccessWithStatus:(NSString*)status duration:(NSTimeInterval)duration;

@end
