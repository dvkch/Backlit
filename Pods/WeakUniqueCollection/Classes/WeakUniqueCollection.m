//
//  WeakUniqueCollection.m
//  book-shelf
//
//  Created by Artem Gladkov on 28.06.16.
//  Copyright © 2016 Sibext Ltd. All rights reserved.
//

#import "WeakUniqueCollection.h"

@interface WeakUniqueCollection ()

@property(nonatomic)dispatch_queue_t accessQueue;
@property(nonatomic)NSHashTable *hashTable;

@end

@implementation WeakUniqueCollection

#pragma mark - Initialization

- (instancetype)init
{
    if (self = [super init]) {
        _accessQueue = dispatch_queue_create("WEAKUNIQUECOLLECTION_QUEUE", DISPATCH_QUEUE_CONCURRENT);
        _hashTable = [NSHashTable hashTableWithOptions:NSPointerFunctionsWeakMemory];
    }
    return self;
}

#pragma mark - Public

- (void)addObject:(id)object
{
    dispatch_barrier_async(_accessQueue, ^{
            [self.hashTable addObject:object];
    });
}

- (void)removeObject:(id)object
{
    dispatch_barrier_async(_accessQueue, ^{
        [self.hashTable removeObject:object];
    });
}

- (void)removeAllObjects
{
    dispatch_barrier_async(_accessQueue, ^{
        [self.hashTable removeAllObjects];
    });
}

- (NSUInteger)count
{
    NSUInteger __block count = 0;
    dispatch_sync(_accessQueue, ^{
        NSArray *allObjects = [self.hashTable allObjects];
        count = allObjects.count;
    });
    return count;
}

- (id)anyObject
{
    id __block obj;
    dispatch_sync(_accessQueue, ^{
        obj = [self.hashTable anyObject];
    });
    return obj;
}

- (NSArray *)allObjects
{
    NSArray *__block array;
    dispatch_sync(_accessQueue, ^{
        array = [self.hashTable allObjects];
    });
    return array;
}

- (BOOL)member:(id)object
{
    BOOL __block isMember = NO;
    dispatch_sync(_accessQueue, ^{
        isMember = [self.hashTable member:object] != nil;
    });
    return isMember;
}

@end
