//
//  SYPair.m
//  SYPair
//
//  Created by Stan Chevallier on 10/01/2016.
//  Copyright © 2016 Syan. All rights reserved.
//

#import "SYPair.h"

@interface SYPair ()
{
    @protected
    id _object1;
    id _object2;
}
@property (nonatomic, strong) id object1;
@property (nonatomic, strong) id object2;
@end

@implementation SYPair

@synthesize object1 = _object1;
@synthesize object2 = _object2;

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
    self = [super init];
    if (self)
    {
        self->_object1 = object1;
        self->_object2 = object2;
    }
    return self;
}

- (void)setObject1:(id)object1
{
    [NSException raise:NSInternalInconsistencyException format:@"Object is immutable"];
}

- (void)setObject2:(id)object2
{
    [NSException raise:NSInternalInconsistencyException format:@"Object is immutable"];
}

- (SYMutablePair *)mutableCopy
{
    return [SYMutablePair pairWithObject:self.object1 andObject:self.object2];
}

- (NSString *)description
{
    return [NSString stringWithFormat:@"<%@: %p, object 1: %@, object 2: %@>",
            NSStringFromClass(self.class),
            self,
            self.object1,
            self.object2];
}

@end

@implementation SYMutablePair

@dynamic object1;
@dynamic object2;

- (instancetype)initWithObject:(id)object1 andObject:(id)object2
{
    self = [super initWithObject:object1 andObject:object2];
    return self;
}

- (void)setObject1:(id)object1
{
    self->_object1 = object1;
}

- (void)setObject2:(id)object2
{
    self->_object2 = object2;
}

- (SYPair *)copy
{
    return [SYPair pairWithObject:self.object1 andObject:self.object2];
}

@end

