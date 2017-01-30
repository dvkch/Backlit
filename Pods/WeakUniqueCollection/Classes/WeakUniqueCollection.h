//
//  WeakUniqueCollection.h
//  book-shelf
//
//  Created by Artem Gladkov on 28.06.16.
//  Copyright © 2016 Sibext Ltd. All rights reserved.
//

#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

/**
 WeakUniqueCollection keeps weak references to the objects and maintains uniqueness.
 It's public API is fully thread safe.
 
 WeakUniqueCollection is not optimized for working with large amount of objects.
 */
@interface WeakUniqueCollection<ObjectType> : NSObject

@property(readonly)NSUInteger count;

/**
 Adds object to the collection

 @param object ObjectType to be added to the collection
 */
- (void)addObject:(ObjectType)object;

/**
 Removes object from the collection (if collection contains it).

 @param object ObjectType to be removed from the collection
 */
- (void)removeObject:(ObjectType)object;

/**
 Removes all objects from the collection.
 */
- (void)removeAllObjects;

/**
 Returns any object from the collection.

 @return ObjectType or nil (if the collection is empty).
 */
- (nullable ObjectType)anyObject;

/**
 Returns array with all objects from the collection.

 @return NSArray with objects (cound be empty if the collection is empty).
 */
- (NSArray <ObjectType> *)allObjects;

/**
 Determines if the object is already contained in the collection.

 @param object ObjectType to be verified
 @return YES if object is in the collection
         NO if object is not in the collection
 */
- (BOOL)member:(ObjectType)object;

@end

NS_ASSUME_NONNULL_END
