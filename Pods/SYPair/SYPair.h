//
//  SYPair.h
//  SYPair
//
//  Created by Stan Chevallier on 10/01/2016.
//  Copyright © 2016 Syan. All rights reserved.
//

#import <Foundation/Foundation.h>

@class SYMutablePair;

@interface SYPair <ObjectType1, ObjectType2> : NSObject

@property (nonatomic, strong, readonly) ObjectType1 object1;
@property (nonatomic, strong, readonly) ObjectType2 object2;

+ (instancetype)pairWithObject:(ObjectType1)object1 andObject:(ObjectType2)object2;
- (instancetype)initWithObject:(ObjectType1)object1 andObject:(ObjectType2)object2 NS_DESIGNATED_INITIALIZER;

- (SYMutablePair *)mutableCopy;

@end

@interface SYMutablePair <ObjectType1, ObjectType2> : SYPair

@property (nonatomic, strong, readwrite) ObjectType1 object1;
@property (nonatomic, strong, readwrite) ObjectType2 object2;

- (SYPair *)copy;

@end
