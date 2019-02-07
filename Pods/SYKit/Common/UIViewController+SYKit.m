//
//  UIViewController+SYKit.m
//  SYKit
//
//  Created by Stan Chevallier on 26/04/2016.
//  Copyright © 2016 Syan. All rights reserved.
//

#import "UIViewController+SYKit.h"

@implementation UIViewController (SYKit)

- (BOOL)sy_isModal
{
    // http://stackoverflow.com/questions/2798653/is-it-possible-to-determine-whether-viewcontroller-is-presented-as-modal/16764496#16764496
    return self.presentingViewController.presentedViewController == self
    || (self.navigationController != nil && self.navigationController.presentingViewController.presentedViewController == self.navigationController)
    || [self.tabBarController.presentingViewController isKindOfClass:[UITabBarController class]];
}

#if !TARGET_OS_TV
- (void)sy_setBackButtonWithText:(NSString * _Nullable)text font:(UIFont * _Nullable)font
{
    UIBarButtonItem *back = [[UIBarButtonItem alloc] initWithTitle:(text.length ? text : @" ")
                                                             style:UIBarButtonItemStylePlain
                                                            target:nil action:nil];
    
    if (font != nil) {
        [back setTitleTextAttributes:@{NSFontAttributeName:font} forState:UIControlStateNormal];
    }
    [back setBackButtonTitlePositionAdjustment:UIOffsetZero forBarMetrics:UIBarMetricsDefault];
    [self.navigationItem setBackBarButtonItem:back];
}
#endif

@end
