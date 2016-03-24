//
//  NSArray+SY.m
//  SaneScanner
//
//  Created by Stan Chevallier on 04/03/2016.
//  Copyright © 2016 Syan. All rights reserved.
//

#import "NSArray+SY.h"
#import "SYTools.h"

@implementation NSArray (SY)

+ (NSArray *)arrayWithNullableObjects:(NSUInteger)count, ...
{
    NSMutableArray *array = [NSMutableArray arrayWithCapacity:count];
    va_list arguments;
    va_start(arguments, count);
    
    for (NSUInteger i = 0; i < count; ++i)
    {
        id value = va_arg(arguments, id) ?: [NSNull null];
        
        if (NSObjectIsBlock(value))
            [array addObject:[value copy]];
        else
            [array addObject:value];
    }
    
    va_end(arguments);
    return [array copy];
}

- (id)realObjectAtIndex:(NSUInteger)index
{
    id object = [self objectAtIndex:index];
    if ([[NSNull null] isEqual:object])
        return nil;
    return object;
}

- (id)nullableObjectAtIndex:(NSUInteger)index
{
    if (index < self.count)
        return self[index];
    return nil;
}

@end
