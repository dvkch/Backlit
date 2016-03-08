//
//  NSObject+SY.h
//  SaneScanner
//
//  Created by Stan Chevallier on 03/03/2016.
//  Copyright © 2016 Syan. All rights reserved.
//

#import <Foundation/Foundation.h>

@interface NSObject (SY)

- (void)performBlock:(void(^)(void))block onThread:(NSThread *)thread;

@end
