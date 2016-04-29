SYOperationQueue
=============

An operation queue subclass that allows LIFO style queuing and a max number of operations.

I made it to allow loading images in a large `UICollectionView` while scrolling. When set properly this will load images needed by the collection view even if the user scrolls, and loading first images needed quickly.


Header
=========

This queue doesn't inherit from `NSOperationQueue` but should expose a similar enough interface to easily switch between the two.

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

