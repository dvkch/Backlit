//
//  NSObject+SYKit.h
//  SYKit
//
//  Created by Stan Chevallier on 03/03/2016.
//  Copyright © 2016 Syan. All rights reserved.
//

#import <Foundation/Foundation.h>

@interface NSObject (SYKit)

- (void)sy_performBlock:(void(^_Nonnull)(void))block onThread:(NSThread * _Nonnull)thread;
+ (void)sy_swizzleSelector:(_Nonnull SEL)originalSelector withSelector:(_Nonnull SEL)swizzledSelector;
+ (BOOL)sy_instance:(_Nonnull id)instance overridesSelector:(_Nonnull SEL)selector;

@end
