//
//  UIViewController+SYKit.m
//  SYKit
//
//  Created by Stan Chevallier on 26/04/2016.
//  Copyright © 2016 Syan. All rights reserved.
//

#import "UIViewController+SYKit.h"

@implementation UIViewController (SYKit)

// http://stackoverflow.com/questions/2798653/is-it-possible-to-determine-whether-viewcontroller-is-presented-as-modal/16764496#16764496
- (BOOL)sy_isModal
{
    return self.presentingViewController.presentedViewController == self
    || (self.navigationController != nil && self.navigationController.presentingViewController.presentedViewController == self.navigationController)
    || [self.tabBarController.presentingViewController isKindOfClass:[UITabBarController class]];
}

@end
