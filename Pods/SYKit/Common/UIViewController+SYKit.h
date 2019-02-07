//
//  UIViewController+SYKit.h
//  SYKit
//
//  Created by Stan Chevallier on 26/04/2016.
//  Copyright © 2016 Syan. All rights reserved.
//

#import <UIKit/UIKit.h>

@interface UIViewController (SYKit)

@property (readonly) BOOL sy_isModal;

#if !TARGET_OS_TV
- (void)sy_setBackButtonWithText:(NSString * _Nullable)text font:(UIFont * _Nullable)font;
#endif

@end
