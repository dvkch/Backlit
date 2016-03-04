//
//  NSObject+SY.m
//  SaneScanner
//
//  Created by Stan Chevallier on 03/03/2016.
//  Copyright © 2016 Syan. All rights reserved.
//

#import "NSObject+SY.h"

@implementation NSObject (SY)

- (void)performSelector:(SEL)selector onThread:(NSThread *)thread withArguments:(NSUInteger)argumentsCount, ...
{
    va_list arguments;
    va_start(arguments, argumentsCount);
    
    for (NSUInteger i = 0; i < argumentsCount; ++i)
    {
        NSLog(@"Arguments %ld: %@", (long)i, va_arg(arguments, id));
    }
    
    va_end(arguments);
}

@end
