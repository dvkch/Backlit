//
//  NSObject+SY.m
//  SaneScanner
//
//  Created by Stan Chevallier on 03/03/2016.
//  Copyright © 2016 Syan. All rights reserved.
//

#import "NSObject+SY.h"

@implementation NSObject (SY)

- (void)performBlock:(void(^)(void))block onThread:(NSThread *)thread
{
    if (thread == [NSThread currentThread])
    {
        if (block)
            block();
        return;
    }
    
    [self performSelector:@selector(sy_runBlock:) onThread:thread withObject:block waitUntilDone:NO];
}

- (void)sy_runBlock:(void(^)(void))block
{
    if (block)
        block();
}

@end
