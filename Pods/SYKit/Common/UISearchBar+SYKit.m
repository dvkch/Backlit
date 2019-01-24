//
//  UISearchBar+SYKit.m
//  SYKit
//
//  Created by Stanislas Chevallier on 15/12/2018.
//  Copyright © 2018 Syan. All rights reserved.
//

#import "UISearchBar+SYKit.h"

@implementation UISearchBar (SYKit)

- (nullable UITextField *)sy_textField
{
    for (UIView *view in self.subviews) {
        if ([view isKindOfClass:UITextField.class])
            return (UITextField *)view;
        
        for (UIView *subview in view.subviews) {
            if ([subview isKindOfClass:UITextField.class])
                return (UITextField *)subview;
        }
    }
    
    return nil;
}

@end
