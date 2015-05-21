//
//  MBProgressHUD+SYAdditions.m
//  SaneScanner
//
//  Created by rominet on 06/05/15.
//  Copyright (c) 2015 Syan. All rights reserved.
//

#import "MBProgressHUD+SYAdditions.h"
#import "SYAppDelegate.h"

@implementation SVProgressHUD (SYAdditions)
/*
+ (instancetype)showWithMode:(MBProgressHUDMode)mode label:(NSString *)label
{
    return [self showWithMode:mode label:label addedToView:nil];
}

+ (instancetype)showWithMode:(MBProgressHUDMode)mode label:(NSString *)label addedToView:(UIView *)view
{
    MBProgressHUD *hud;
    
    if(view)
    {
        hud = [[self alloc] initWithView:view];
        [view addSubview:hud];
    }
    else
    {
        UIWindow *window = [[SYAppDelegate obtain] window];
        hud = [[self alloc] initWithWindow:window];
        [window addSubview:hud];
    }
    
    [hud setDimBackground:NO];
    [hud setLabelText:label];
    [hud setMode:mode];
    [hud show:YES];
    [hud setRemoveFromSuperViewOnHide:YES];
    [hud layoutIfNeeded];
    
    return hud;
}

- (void)setModeTextWithTitle:(NSString *)title hideAfter:(NSTimeInterval)hideAfter
{
    [self setLabelText:title];
    [self setMode:MBProgressHUDModeText];
    
    if(hideAfter >= 0)
        [self hide:YES afterDelay:hideAfter];
}
*/
@end
