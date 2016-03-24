//
//  NSArray+SY.h
//  SaneScanner
//
//  Created by Stan Chevallier on 04/03/2016.
//  Copyright © 2016 Syan. All rights reserved.
//

#import <Foundation/Foundation.h>

@interface NSArray (SY)

+ (NSArray *)arrayWithNullableObjects:(NSUInteger)count, ...;

- (id)realObjectAtIndex:(NSUInteger)index;
- (id)nullableObjectAtIndex:(NSUInteger)index;

@end
