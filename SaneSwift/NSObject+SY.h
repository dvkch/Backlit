//
//  NSObject+SYKit.h
//  SYKit
//
//  Created by Stan Chevallier on 03/03/2016.
//  Copyright © 2016 Syan. All rights reserved.
//

#import <Foundation/Foundation.h>

@interface NSObject (SaneSwift)

- (void)ss_performBlock:(void(^)(void))block onThread:(NSThread *)thread;

@end
