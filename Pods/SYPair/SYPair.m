//
//  SYPair.m
//  SYPair
//
//  Created by Stan Chevallier on 10/01/2016.
//  Copyright © 2016 Syan. All rights reserved.
//

#import "SYPair.h"

@interface SYPair ()
@property (nonatomic, strong) id object1;
@property (nonatomic, strong) id object2;
@end

@implementation SYPair

+ (instancetype)pairWithObject:(id)object1 andObject:(id)object2
{
    return [[self alloc] initWithObject:object1 andObject:object2];
}

- (instancetype)init
{
    self = [self initWithObject:nil andObject:nil];
    return self;
}

- (instancetype)initWithObject:(id)object1 andObject:(id)object2
{
    NSAssert(object1 != nil, @"Objects cannot be nil");
    NSAssert(object2 != nil, @"Objects cannot be nil");
    
    self = [super init];
    if (self)
    {
        self.object1 = object1;
        self.object2 = object2;
    }
    return self;
}

@end
