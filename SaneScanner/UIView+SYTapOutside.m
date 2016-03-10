//
//  UIView+SYTapOutside.m
//  SaneScanner
//
//  Created by Stan Chevallier on 10/03/2016.
//  Copyright © 2016 Syan. All rights reserved.
//

#import "UIView+SYTapOutside.h"
#import <objc/runtime.h>

@implementation UIView (SYTapOutside)

@dynamic sy_tapInsets;

+ (void)sy_swizzlePointInside
{
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        Class class = [self class];
        
        // When swizzling a class method, use the following:
        // Class class = object_getClass((id)self);
        
        SEL originalSelector = @selector(pointInside:withEvent:);
        SEL swizzledSelector = @selector(sy_pointInside:withEvent:);
        
        Method originalMethod = class_getInstanceMethod(class, originalSelector);
        Method swizzledMethod = class_getInstanceMethod(class, swizzledSelector);
        
        BOOL didAddMethod =
        class_addMethod(class,
                        originalSelector,
                        method_getImplementation(swizzledMethod),
                        method_getTypeEncoding(swizzledMethod));
        
        if (didAddMethod) {
            class_replaceMethod(class,
                                swizzledSelector,
                                method_getImplementation(originalMethod),
                                method_getTypeEncoding(originalMethod));
        } else {
            method_exchangeImplementations(originalMethod, swizzledMethod);
        }
    });
}

- (void)setSy_tapInsets:(UIEdgeInsets)sy_tapInsets {
    [[self class] sy_swizzlePointInside];
    
    objc_setAssociatedObject(self,
                             @selector(sy_tapInsets),
                             [NSValue valueWithUIEdgeInsets:sy_tapInsets],
                             OBJC_ASSOCIATION_RETAIN_NONATOMIC);
}

- (UIEdgeInsets)sy_tapInsets {
    NSValue *value = objc_getAssociatedObject(self, @selector(sy_tapInsets));
    return [value UIEdgeInsetsValue];
}

- (BOOL)sy_pointInside:(CGPoint)point withEvent:(UIEvent *)event
{
    if (CGRectContainsPoint(UIEdgeInsetsInsetRect(self.bounds, self.sy_tapInsets), point))
        return YES;
    
    return [self sy_pointInside:point withEvent:event];
}

@end
