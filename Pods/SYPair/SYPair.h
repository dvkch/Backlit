//
//  SYPair.h
//  SYPair
//
//  Created by Stan Chevallier on 10/01/2016.
//  Copyright © 2016 Syan. All rights reserved.
//

#import <Foundation/Foundation.h>

@interface SYPair <ObjectType1, ObjectType2> : NSObject

+ (instancetype)pairWithObject:(ObjectType1)object1 andObject:(ObjectType2)object2;
- (instancetype)initWithObject:(ObjectType1)object1 andObject:(ObjectType2)object2 NS_DESIGNATED_INITIALIZER;

- (ObjectType1)object1;
- (ObjectType2)object2;

@end
