SYPair
=======

Pair object for Objective-C with generics support!

	#import <Foundation/Foundation.h>

	@interface SYPair <ObjectType1, ObjectType2> : NSObject

	+ (instancetype)pairWithObject:(ObjectType1)object1 andObject:(ObjectType2)object2;
	- (instancetype)initWithObject:(ObjectType1)object1 andObject:(ObjectType2)object2;

	- (ObjectType1)object1;
	- (ObjectType2)object2;

	@end



License
===

Use it as you like in every project you want, redistribute with mentions of my name and don't blame me if it breaks :)

-- dvkch
 