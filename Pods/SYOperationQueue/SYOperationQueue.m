//
//  SYOperationQueue.m
//  Plexy
//
//  Created by Stan Chevallier on 13/12/2015.
//  Copyright © 2015 Syan. All rights reserved.
//

#import "SYOperationQueue.h"

@interface SYOperationQueue () {
    NSLock *_lock;
}
@property (nonatomic, strong) NSMutableArray <NSOperation *> *mutableOperations;
@property (nonatomic, strong) NSOperationQueue *queue;
@end

@implementation SYOperationQueue

- (instancetype)init
{
    self = [super init];
    if (self)
    {
        _lock = [[NSLock alloc] init];
        self.mutableOperations = [NSMutableArray array];
        self.queue = [[NSOperationQueue alloc] init];
        [self.queue addObserver:self forKeyPath:@"operations" options:0 context:NULL];
        [self.queue addObserver:self forKeyPath:@"maxConcurrentOperationCount" options:0 context:NULL];
    }
    return self;
}

- (void)observeValueForKeyPath:(NSString *)keyPath
                      ofObject:(id)object
                        change:(NSDictionary *)change
                       context:(void *)context
{
    if ((object == self.queue && [keyPath isEqualToString:@"operations"]) ||
        (object == self.queue && [keyPath isEqualToString:@"maxConcurrentOperationCount"]))
    {
        [self processOperations];
        return;
    }
    
    if ([object isKindOfClass:[NSOperation class]] && [keyPath isEqualToString:@"isFinished"])
    {
        if ([change[NSKeyValueChangeNewKey] boolValue] == YES)
        {
            [self purgeFinishedOperations];
        }
        return;
    }

    [super observeValueForKeyPath:keyPath ofObject:object change:change context:context];
}

- (void)processOperations
{
    NSInteger numberOfAvailableSlots = self.queue.maxConcurrentOperationCount - self.queue.operations.count;
    
    for (NSInteger i = 0; i < numberOfAvailableSlots; ++i)
    {
        NSOperation *operation = [self nextOperationToRun];
        [operation removeObserver:self forKeyPath:@"isFinished"];

        [_lock lock];
        [self.mutableOperations removeObject:operation];
        [_lock unlock];
        
        [self.queue addOperation:operation];
    }
}

- (void)purgeFinishedOperations
{
    [_lock lock];
    [self.mutableOperations filterUsingPredicate:[NSPredicate predicateWithFormat:@"SELF.isFinished = 0"]];
    [_lock unlock];
}

- (void)purgeOldOperations
{
    if (!self.maxSurvivingOperations || self.operationCount < self.maxSurvivingOperations)
        return;
    
    NSUInteger numberOfItemsToPurge = self.operationCount - self.maxSurvivingOperations;
    
    NSArray *operationsToCancel;
    [_lock lock];
    {
        // Shouldn't happen but well, you know
        if (numberOfItemsToPurge >= self.mutableOperations.count)
            numberOfItemsToPurge = self.mutableOperations.count;
        
        operationsToCancel = [self.mutableOperations subarrayWithRange:NSMakeRange(0, numberOfItemsToPurge)];
    }
    [_lock unlock];
    
    [operationsToCancel makeObjectsPerformSelector:@selector(cancel)];
}

- (NSOperation *)nextOperationToRun
{
    NSOperation *operation;
    [_lock lock];

    for (NSUInteger i = 0; i < self.mutableOperations.count; ++i)
    {
        NSOperation *op;
        if (self.mode == SYOperationQueueModeFIFO)
            op = self.mutableOperations[i];
        else
            op = self.mutableOperations[self.mutableOperations.count - i - 1];
        
        if (![op isCancelled])
        {
            operation = op;
            break;
        }
    }
    
    [_lock unlock];
    return operation;
}

#pragma mark - Public methods

- (NSArray<NSOperation *> *)operations
{
    NSMutableArray *operations = [NSMutableArray array];
    [operations addObjectsFromArray:self.queue.operations];
    [_lock lock];
    [operations addObjectsFromArray:self.mutableOperations];
    [_lock unlock];
    return [operations copy];
}

- (NSUInteger)operationCount
{
    NSUInteger totalCount = self.queue.operationCount;
    [_lock lock];
    totalCount += self.mutableOperations.count;
    [_lock unlock];
    return totalCount;
}

- (NSInteger)maxConcurrentOperationCount
{
    return self.queue.maxConcurrentOperationCount;
}

- (void)setMaxConcurrentOperationCount:(NSInteger)maxConcurrentOperationCount
{
    [self.queue setMaxConcurrentOperationCount:maxConcurrentOperationCount];
}

- (void)setMaxSurvivingOperations:(NSInteger)maxSurvivingOperation
{
    if (maxSurvivingOperation == self.maxSurvivingOperations)
        return;
    
    NSInteger value = maxSurvivingOperation;
    
    if (maxSurvivingOperation > 0 && maxSurvivingOperation < self.maxConcurrentOperationCount)
        maxSurvivingOperation = self.maxConcurrentOperationCount;
    
    self->_maxSurvivingOperations = value;
    [self purgeOldOperations];
}

- (BOOL)isSuspended
{
    return self.queue.isSuspended;
}

- (void)setSuspended:(BOOL)suspended
{
    [self.queue setSuspended:suspended];
}

- (NSString *)name
{
    return self.queue.name;
}

- (void)setName:(NSString *)name
{
    [self.queue setName:name];
}

- (void)addOperation:(__kindof NSOperation *)op
{
    [op addObserver:self forKeyPath:@"isFinished" options:NSKeyValueObservingOptionNew context:NULL];
    
    [_lock lock];
    [self.mutableOperations addObject:op];
    [_lock unlock];
    [self purgeOldOperations];
    [self processOperations];
}

- (void)addOperationWithBlock:(void (^)(void))block
{
    NSBlockOperation *op = [NSBlockOperation blockOperationWithBlock:block];
    [self addOperation:op];
}

- (void)cancelAllOperations
{
    [_lock lock];
    NSArray *operationsToCancel = [self.mutableOperations copy];
    [_lock unlock];
    
    for (NSOperation *op in operationsToCancel)
    {
        [op cancel];
    }
    
    [self.queue cancelAllOperations];
}

@end
