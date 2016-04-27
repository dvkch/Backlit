/*
 *  MHWDirectoryWatcher.h
 *  Created by Martin Hwasser on 12/19/11.
 */

#import "MHWDirectoryWatcher.h"

static double     const kMHWDirectoryWatcherDefaultPollInterval   = 0.2;
static NSUInteger const kMHWDirectoryWatcherDefaultPollRetryCount = 5;

@interface MHWDirectoryWatcher ()

@property (atomic, assign) dispatch_source_t source;
@property (atomic, assign) dispatch_queue_t queue;
@property (atomic, assign) NSInteger retriesLeft;
@property (atomic, getter = isDirectoryChanging) BOOL directoryChanging;
@property (atomic, readwrite, copy) NSString *watchedPath;
@property (atomic, copy) void(^startedCallback)(void);
@property (atomic, copy) void(^endedCallback)(BOOL allChangesFinished);
@property (atomic, strong) NSDictionary <NSString *, NSNumber *> *polledFiles;

@end

@implementation MHWDirectoryWatcher

- (void)dealloc
{
    [self stopWatching];
    _startedCallback = nil;
    _endedCallback   = nil;
}

- (id)initWithPath:(NSString *)path
{
    self = [super init];
    if (self) {
        _pollInterval = kMHWDirectoryWatcherDefaultPollInterval;
        _pollRetryCount = kMHWDirectoryWatcherDefaultPollRetryCount;
        _watchedPath = [path copy];
        _queue = dispatch_queue_create("MHWDirectoryWatcherQueue", 0);
    }
    return self;
}

+ (MHWDirectoryWatcher *)directoryWatcherAtPath:(NSString *)watchedPath
                               startImmediately:(BOOL)startImmediately
                                 changesStarted:(void (^)(void))changesStarted
                                   changesEnded:(void (^)(BOOL))changesEnded
{
    NSAssert(watchedPath != nil, @"The directory to watch must not be nil");
    MHWDirectoryWatcher *directoryWatcher = [[MHWDirectoryWatcher alloc] initWithPath:watchedPath];
    directoryWatcher.startedCallback = changesStarted;
    directoryWatcher.endedCallback   = changesEnded;
    
    if (startImmediately) {
        if (![directoryWatcher startWatching]) {
            // Something went wrong, return nil
            return nil;
        }
    }
    return directoryWatcher;
}

#pragma mark -
#pragma mark - Public methods

- (BOOL)stopWatching
{
    if (_source != NULL) {
        dispatch_source_cancel(_source);
        _source = NULL;
        return YES;
    }
    return NO;
}

- (BOOL)startWatching
{
    // Already monitoring
    if (self.source != NULL) {
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
    
    // Monitor the directory for writes
    self.source = dispatch_source_create(DISPATCH_SOURCE_TYPE_VNODE, // Monitors a file descriptor
                                         fd, // our file descriptor
                                         DISPATCH_VNODE_WRITE, // The file-system object data changed.
                                         queue); // the queue to dispatch on
    
    if (!_source) {
        cleanup();
        return NO;
    }
    
    __weak typeof (self) _weakSelf = self;
    // Call directoryDidChange on event callback
    dispatch_source_set_event_handler(self.source, ^{
        [_weakSelf directoryDidChange];
    });
    
    // Dispatch source destructor
    dispatch_source_set_cancel_handler(self.source, cleanup);
    
    // Sources are create in suspended state, so resume it
    dispatch_resume(self.source);
    
    // Everything was OK
    return YES;
}

- (NSArray<NSString *> *)filesBeingWrittenTo
{
    return self.polledFiles.allKeys;
}

#pragma mark -
#pragma mark - Private methods

- (NSArray <NSString *> *)directoryMetadata
{
    NSFileManager *fileManager = [[NSFileManager alloc] init];
    NSArray *contents = [fileManager contentsOfDirectoryAtPath:self.watchedPath
                                                         error:NULL];
    
    NSMutableArray *directoryMetadata = [NSMutableArray array];
    
    for (NSString *fileName in contents) {
        @autoreleasepool {
            NSString *filePath = [self.watchedPath stringByAppendingPathComponent:fileName];
            NSDictionary *fileAttributes = [[NSFileManager defaultManager] attributesOfItemAtPath:filePath
                                                                                            error:nil];
            NSInteger fileSize = [[fileAttributes objectForKey:NSFileSize] intValue];
            // The fileName and fileSize will be our hash key
            NSString *fileHash = [NSString stringWithFormat:@"%@%ld", fileName, (long)fileSize];
            // Add it to our metadata list
            [directoryMetadata addObject:fileHash];
        }
    }
    return directoryMetadata;
}

- (NSDictionary<NSString *, NSNumber *> *)detailedDirectoryMetadata
{
    NSFileManager *fileManager = [[NSFileManager alloc] init];
    NSArray *contents = [fileManager contentsOfDirectoryAtPath:self.watchedPath
                                                         error:NULL];
    
    NSMutableDictionary *directoryMetadata = [NSMutableDictionary dictionaryWithCapacity:contents.count];
    
    for (NSString *fileName in contents) {
        @autoreleasepool {
            NSString *filePath = [self.watchedPath stringByAppendingPathComponent:fileName];
            NSDictionary *fileAttributes = [[NSFileManager defaultManager] attributesOfItemAtPath:filePath
                                                                                            error:nil];
            NSUInteger fileSize = [[fileAttributes objectForKey:NSFileSize] unsignedIntegerValue];
            
            // Add it to our metadata list
            [directoryMetadata setObject:@(fileSize) forKey:fileName];
        }
    }
    return directoryMetadata;
}

- (void)pollDirectoryForChangesWithMetadata:(NSArray *)oldDirectoryMetadata
{
    NSArray *newDirectoryMetadata = [self directoryMetadata];
    
    // Check if metadata has changed
    self.directoryChanging = ![newDirectoryMetadata isEqualToArray:oldDirectoryMetadata];
    
    // Reset retries if it's still changing
    self.retriesLeft = self.isDirectoryChanging ? self.pollRetryCount : self.retriesLeft;
    
    if (self.isDirectoryChanging || 0 < self.retriesLeft--) {
        // Either the directory is changing or
        // we should try again as more changes may occur
        __weak typeof (self) _weakSelf = self;
        dispatch_time_t popTime = dispatch_time(DISPATCH_TIME_NOW, self.pollInterval * NSEC_PER_SEC);
        dispatch_after(popTime, self.queue, ^(void){
            [_weakSelf pollDirectoryForChangesWithMetadata:newDirectoryMetadata];
        });
    } else {
        // Changes appear to be completed
        // Post a notification informing that the directory did change
        dispatch_async(dispatch_get_main_queue(), ^{
            if (self.endedCallback)
                self.endedCallback(YES);
        });
    }
}

- (void)pollDirectoryForChangesWithDetailedMetadata:(NSDictionary <NSString *, NSNumber *> *)oldDirectoryMetadata
{
    NSDictionary <NSString *, NSNumber *> *newDirectoryMetadata = [self detailedDirectoryMetadata];
    
    // Determine possible changes
    NSArray <NSString *> *oldFileList = [oldDirectoryMetadata allKeys];
    NSArray <NSString *> *newFileList = [newDirectoryMetadata allKeys];
#warning less variables
    
    NSMutableArray <NSString *> *addedItems   = [newFileList mutableCopy];
    [addedItems removeObjectsInArray:oldFileList];
    
    NSMutableArray <NSString *> *removedItems = [oldFileList mutableCopy];
    [removedItems removeObjectsInArray:newFileList];
    
    NSMutableSet <NSString *> *commonItems = [NSMutableSet setWithArray:oldFileList];
    [commonItems intersectSet:[NSSet setWithArray:newFileList]];
    
    NSMutableArray <NSString *> *changedItems = [NSMutableArray array];
    for (NSString *commonItem in commonItems) {
        if (![oldDirectoryMetadata[commonItem] isEqualToNumber:newDirectoryMetadata[commonItem]]) {
            [changedItems addObject:commonItem];
        }
    }
    
    // Polled files = newly changed files + changing files + previously polling files
    NSMutableSet <NSString *> *allPolledFiles = [NSMutableSet set];
    [allPolledFiles addObjectsFromArray:addedItems];
    [allPolledFiles addObjectsFromArray:changedItems];
    [allPolledFiles addObjectsFromArray:self.polledFiles.allKeys];
    
    NSMutableDictionary *polledFiles = [NSMutableDictionary dictionaryWithDictionary:self.polledFiles];
    
    // Update number of retries left for each polled files
    for (NSString *polledFile in allPolledFiles)
    {
        NSNumber *retriesLeft = polledFiles[polledFile] ?: @(self.pollRetryCount);
        polledFiles[polledFile] = @(retriesLeft.intValue-1);
    }
    
    // Remove finished files
    NSArray <NSString *> *finishedItems = [polledFiles allKeysForObject:@(0)];
    [polledFiles removeObjectsForKeys:finishedItems];
    self.polledFiles = [polledFiles copy];
    
    // Check if metadata has changed
    self.directoryChanging = (self.polledFiles.count > 0);
    
    if (self.isDirectoryChanging) {
        // Either the directory is changing or
        // we should try again as more changes may occur
        __weak typeof (self) _weakSelf = self;
        dispatch_time_t popTime = dispatch_time(DISPATCH_TIME_NOW, self.pollInterval * NSEC_PER_SEC);
        dispatch_after(popTime, self.queue, ^(void){
            [_weakSelf pollDirectoryForChangesWithDetailedMetadata:newDirectoryMetadata];
        });
    }
    
    if (!self.isDirectoryChanging || removedItems.count || finishedItems.count) {
        // Changes appear to be completed,
        // Post a notification informing that the directory did change
        dispatch_async(dispatch_get_main_queue(), ^{
            if (self.endedCallback)
                self.endedCallback(!self.directoryChanging);
        });
    }
}

- (void)directoryDidChange
{
    // Ignore if we already are polling
    if (self.isDirectoryChanging) {
        return;
    }
    
    // Changes just occurred
    self.directoryChanging = YES;
    self.retriesLeft = self.pollRetryCount;
    
    //NSLog(@"------------------------------------");
    // Tell the delegate changes have started
    dispatch_async(dispatch_get_main_queue(), ^{
        if (self.startedCallback) {
            self.startedCallback();
        }
    });
    
    __weak typeof (self) _weakSelf = self;
    dispatch_time_t popTime = dispatch_time(DISPATCH_TIME_NOW, self.pollInterval * NSEC_PER_SEC);
    
    if (self.useQuickerPoll)
    {
        NSArray <NSString *> *metadata = [self directoryMetadata];
        dispatch_after(popTime, self.queue, ^(void){
            [_weakSelf pollDirectoryForChangesWithMetadata:metadata];
        });
    }
    else
    {
        NSDictionary <NSString *, NSNumber *> *metadata = [self detailedDirectoryMetadata];
        dispatch_after(popTime, self.queue, ^(void){
            [_weakSelf pollDirectoryForChangesWithDetailedMetadata:metadata];
        });
    }
}

@end
