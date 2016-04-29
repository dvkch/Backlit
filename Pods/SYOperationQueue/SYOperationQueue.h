//
//  SYOperationQueue.h
//  Plexy
//
//  Created by Stan Chevallier on 13/12/2015.
//  Copyright © 2015 Syan. All rights reserved.
//

#import <Foundation/Foundation.h>

typedef enum : NSUInteger {
    SYOperationQueueModeFIFO,
    SYOperationQueueModeLIFO,
} SYOperationQueueMode;

@interface SYOperationQueue : NSObject

@property (readonly, copy) NSArray <__kindof NSOperation *> *operations;
@property (readonly) NSUInteger operationCount;
@property (getter=isSuspended) BOOL suspended;
@property (copy) NSString *name;

@property (nonatomic) NSInteger maxConcurrentOperationCount;
@property (nonatomic) NSInteger maxSurvivingOperations;
@property (nonatomic) SYOperationQueueMode mode;

- (void)addOperation:(__kindof NSOperation *)op;
- (void)addOperationWithBlock:(void(^)(void))block;
- (void)cancelAllOperations;

@end
