//
//  SYOperationQueue.h
//  Plexy
//
//  Created by Stan Chevallier on 13/12/2015.
//  Copyright © 2015 Syan. All rights reserved.
//

#import <Foundation/Foundation.h>

typedef NS_ENUM(NSUInteger, SYOperationQueueMode) {
    SYOperationQueueModeFIFO,
    SYOperationQueueModeLIFO,
};

@interface SYOperationQueue : NSObject

@property (readonly, copy) NSArray <__kindof NSOperation *> * _Nonnull operations;
@property (readonly) NSInteger operationCount;
@property (getter=isSuspended) BOOL suspended;
@property (copy) NSString * _Nullable name;

@property (nonatomic) NSInteger maxConcurrentOperationCount;
@property (nonatomic) NSInteger maxSurvivingOperations;
@property (nonatomic) SYOperationQueueMode mode;

- (void)addOperation:(__kindof NSOperation * _Nonnull)op;
- (void)addOperationWithBlock:(void(^ _Nonnull)(void))block;
- (void)cancelAllOperations;

@end
