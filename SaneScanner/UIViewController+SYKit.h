//
//  UIViewController+SYKit.h
//  SYKit
//
//  Created by Stan Chevallier on 26/04/2016.
//  Copyright © 2016 Syan. All rights reserved.
//

#import <UIKit/UIKit.h>

@interface UIViewController (SYKit)

- (BOOL)sy_isModal;
- (void)sy_setBackButtonWithText:(NSString *)text font:(UIFont *)font;

@end
