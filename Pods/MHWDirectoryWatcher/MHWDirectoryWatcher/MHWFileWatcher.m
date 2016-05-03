//
//  MHWFileWatcher.m
//  Created by Stan Chevallier on 29/04/2016.
//

#import "MHWFileWatcher.h"

@interface MHWFileWatcher ()
@property (atomic, assign, readwrite) double timeout;
@property (atomic, copy,   readwrite) NSString *watchedPath;
@property (atomic, copy,   readwrite) void(^completionBlock)(MHWFileWatcher *watcher);
@property (atomic) dispatch_source_t watchSource;
@property (atomic) dispatch_source_t timerSource;
@end

@implementation MHWFileWatcher

- (instancetype)init
{
    self = [self initWithPath:nil timeout:0. completionBlock:nil];
    return self;
}

- (instancetype)initWithPath:(NSString *)path
                     timeout:(double)timeout
             completionBlock:(void(^)(MHWFileWatcher *watcher))completionBlock
{
    BOOL exists = [[NSFileManager defaultManager] fileExistsAtPath:path isDirectory:NULL];
    
    if (!exists)
        return nil;
    
    if (!completionBlock || timeout <= 0)
        return nil;
    
    self = [super init];
    if (self)
    {
        self.watchedPath = path;
        self.timeout = timeout;
        self.completionBlock = completionBlock;
        if (![self startWatching])
            return nil;
        [self setTimeout];
    }
    return self;
}

- (void)dealloc
{
    [self stopWatcher];
    _completionBlock = nil;
}

- (BOOL)startWatching
{
    // Already monitoring
    if (self.watchSource != NULL) {
        return NO;
    }
    
    // Open an event-only file descriptor associated with the directory
    int fd = open([self.watchedPath fileSystemRepresentation], O_EVTONLY);
    if (fd < 0) {
        return NO;
    }
    
    void (^cleanup)() = ^{
        close(fd);
    };
    
    // Get a low priority queue
    dispatch_queue_t queue = dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_LOW, 0);
    
    // Monitor the files for writes
    self.watchSource = dispatch_source_create(DISPATCH_SOURCE_TYPE_VNODE, // Monitors a file descriptor
                                              fd, // our file descriptor
                                              DISPATCH_VNODE_WRITE, // The file-system object data changed.
                                              queue); // the queue to dispatch on
    
    if (!self.watchSource) {
        cleanup();
        return NO;
    }
    
    // Call startTimeout on event callback
    __weak typeof (self) _weakSelf = self;
    dispatch_source_set_event_handler(self.watchSource, ^{
        [_weakSelf setTimeout];
    });
    
    // Dispatch source destructor
    dispatch_source_set_cancel_handler(self.watchSource, cleanup);
    
    // Sources are create in suspended state, so resume it
    dispatch_resume(self.watchSource);
    
    // Everything was OK
    return YES;
}

- (void)setTimeout
{
    if (!self.timerSource)
    {
        // Get a low priority queue
        dispatch_queue_t queue = dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_LOW, 0);
        
        // create our timer source
        self.timerSource = dispatch_source_create(DISPATCH_SOURCE_TYPE_TIMER, 0, 0, queue);
        
        // Call timedOut on event callback
        __weak typeof (self) _weakSelf = self;
        dispatch_source_set_event_handler(self.timerSource, ^{
            [_weakSelf timedOut];
        });
        
        // now that our timer is all set to go, start it
        dispatch_resume(self.timerSource);
    }
    
    dispatch_source_set_timer(self.timerSource,
                              dispatch_time(DISPATCH_TIME_NOW, self.timeout * NSEC_PER_SEC),
                              DISPATCH_TIME_FOREVER, 0);
}

- (void)stopWatcher
{
    if (self.watchSource != NULL) {
        dispatch_source_cancel(self.watchSource);
        self.watchSource = NULL;
    }
    if (self.timerSource != NULL) {
        dispatch_source_cancel(self.timerSource);
        self.timerSource = NULL;
    }
}

- (void)timedOut
{
    [self stopWatcher];
    self.completionBlock(self);
}

@end

