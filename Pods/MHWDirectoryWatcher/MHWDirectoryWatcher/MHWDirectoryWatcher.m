/*
 *  MHWDirectoryWatcher.h
 *  Created by Martin Hwasser on 12/19/11.
 */

#import "MHWDirectoryWatcher.h"
#import "MHWFileWatcher.h"

typedef NSDictionary        <NSString *, id> MHWMetadata;
typedef NSMutableDictionary <NSString *, id> MHWMutableMetadata;

static double const kMHWDirectoryWatcherDefaultFileChangesTimeout      = 0.5;
static double const kMHWDirectoryWatcherDefaultDirectoryChangesTimeout = 0.1;

@interface MHWDirectoryChanges ()
@property (nonatomic, strong, readwrite) NSArray <NSString *> *addedFiles;
@property (nonatomic, strong, readwrite) NSArray <NSString *> *removedFiles;
@end

@implementation MHWDirectoryChanges

- (instancetype)initWithPreviousMetadata:(MHWMetadata *)oldMetadata newMetadata:(MHWMetadata *)newMetadata
{
    self = [super init];
    if (self)
    {
        NSArray <NSString *> *oldFileList = oldMetadata.allKeys;
        NSArray <NSString *> *newFileList = newMetadata.allKeys;
        
        // Deal with removed files
        {
            NSMutableArray <NSString *> *removedItems = [oldFileList mutableCopy];
            [removedItems removeObjectsInArray:newFileList];
            self.removedFiles = removedItems;
        }
        
        // Deal with new files
        {
            NSMutableArray <NSString *> *addedItems = [newFileList mutableCopy];
            [addedItems removeObjectsInArray:oldFileList];
            self.addedFiles = addedItems;
        }
    }
    return self;
}

@end

@interface MHWDirectoryWatcher ()

@property (atomic) dispatch_source_t watchSource;
@property (atomic) dispatch_source_t timerSource;
@property (atomic, copy, readwrite) NSString *watchedPath;
@property (atomic, strong) MHWMetadata *previousMetadata;
@property (atomic, strong) MHWMetadata *changesStartedMetadata;
@property (atomic, strong) NSMutableArray <MHWFileWatcher *> *fileWatchers;
@property (atomic, strong) NSMutableArray <MHWDirectoryWatcher *> *subdirWatchers;
@property (atomic, strong) NSLock *lockFiles;
@property (atomic, strong) NSLock *lockSubdirs;
@property (atomic) BOOL directoryIsChanging;

@end

@implementation MHWDirectoryWatcher

- (void)dealloc
{
    [self stopWatching];
    _changesStartedBlock = nil;
    _filesAddedBlock   = nil;
    _filesRemovedBlock = nil;
    _changesEndedBlock = nil;
}

- (id)initWithPath:(NSString *)path
{
    self = [super init];
    if (self) {
        _fileChangesTimeout = kMHWDirectoryWatcherDefaultFileChangesTimeout;
        _directoryChangesTimeout = kMHWDirectoryWatcherDefaultDirectoryChangesTimeout;
        _watchedPath = [path copy];
        _fileWatchers = [NSMutableArray array];
        _subdirWatchers = [NSMutableArray array];
        _lockFiles = [[NSLock alloc] init];
        _lockSubdirs = [[NSLock alloc] init];
    }
    return self;
}

+ (MHWDirectoryWatcher *)directoryWatcherAtPath:(NSString *)watchedPath
                               startImmediately:(BOOL)startImmediately
                            changesStartedBlock:(void(^)(void))changesStartedBlock
                                filesAddedBlock:(void(^)(NSArray <NSString *> *addedFiles))filesAddedBlock
                              filesRemovedBlock:(void(^)(NSArray <NSString *> *removedFiles))filesRemovedBlock
                              changesEndedBlock:(void(^)(MHWDirectoryChanges *))changesEndedBlock
{
    NSAssert(watchedPath != nil, @"The directory to watch must not be nil");
    MHWDirectoryWatcher *directoryWatcher = [[MHWDirectoryWatcher alloc] initWithPath:watchedPath];
    directoryWatcher.changesStartedBlock = changesStartedBlock;
    directoryWatcher.filesAddedBlock   = filesAddedBlock;
    directoryWatcher.filesRemovedBlock = filesRemovedBlock;
    directoryWatcher.changesEndedBlock = changesEndedBlock;
    
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

- (void)stopWatching
{
    if (_watchSource != NULL) {
        dispatch_source_cancel(_watchSource);
        _watchSource = NULL;
    }
    if (_timerSource != NULL) {
        dispatch_source_cancel(_timerSource);
        _timerSource = NULL;
    }
    
    [self.lockSubdirs lock];
    {
        for (MHWDirectoryWatcher *subdirWatcher in self.subdirWatchers) {
            [subdirWatcher stopWatching];
        }
        [self.subdirWatchers removeAllObjects];
    }
    [self.lockSubdirs unlock];
    
    [self.lockFiles lock];
    {
        for (MHWFileWatcher *fileWatcher in self.fileWatchers) {
            [fileWatcher stopWatcher];
        }
        [self.fileWatchers removeAllObjects];
    }
    [self.lockFiles unlock];
}

- (BOOL)startWatching
{
    return [self startWatchingFromLastSnapshot:NO];
}

- (BOOL)startWatchingFromLastSnapshot:(BOOL)useLastSnapshot;
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
    
    // Monitor the directory for writes
    self.watchSource = dispatch_source_create(DISPATCH_SOURCE_TYPE_VNODE, // Monitors a file descriptor
                                              fd, // our file descriptor
                                              DISPATCH_VNODE_WRITE, // The file-system object data changed.
                                              queue); // the queue to dispatch on
    
    if (!self.watchSource) {
        cleanup();
        return NO;
    }
    
    __weak typeof (self) _weakSelf = self;
    // Call directoryDidChange on event callback
    dispatch_source_set_event_handler(self.watchSource, ^{
        [_weakSelf processChanges];
    });
    
    // Dispatch source destructor
    dispatch_source_set_cancel_handler(self.watchSource, cleanup);
    
    // Snapshot current directory metadata
    if (!_previousMetadata || !useLastSnapshot)
        _previousMetadata = [self directoryMetadata];

    // Reset current state
    _directoryIsChanging = NO;
    
    dispatch_async(queue, ^{
        // In case we stopped the watch (e.g. when going to background)
        // we need to know what happened and start file watchers if needed
        if (useLastSnapshot)
            [self processChanges];
        
        // Sources are create in suspended state, so resume it
        dispatch_resume(self.watchSource);
    });
    
    // Everything was OK
    return YES;
}

- (void)forceTimeout
{
    // forces the changesEndedBlock to be called when timeout is finished,
    // even if no changes occured
    self.directoryIsChanging = YES;
    [self setTimeout];
}

- (NSArray<NSString *> *)filesBeingWrittenTo
{
    NSMutableArray <NSString *> *items = [NSMutableArray array];
    [self.lockFiles lock];
    {
        for (MHWFileWatcher *watcher in self.fileWatchers)
            [items addObject:watcher.watchedPath];
    }
    [self.lockFiles unlock];
    return items;
}

- (NSArray<NSString *> *)directoriesBeingWrittenTo
{
    NSMutableArray <NSString *> *items = [NSMutableArray array];
    [self.lockSubdirs lock];
    {
        for (MHWDirectoryWatcher *watcher in self.subdirWatchers)
            [items addObject:watcher.watchedPath];
    }
    [self.lockSubdirs unlock];
    return items;
}

#pragma mark - Private methods

#pragma mark General

- (void)callbackIfEnded
{
    if (!self.directoryIsChanging)
        return;
    
    [self.lockFiles lock];
    BOOL filesEnded = (self.fileWatchers.count == 0);
    [self.lockFiles unlock];
    
    [self.lockSubdirs lock];
    BOOL subdirEnded = (self.subdirWatchers.count == 0);
    [self.lockSubdirs unlock];
    
    if (!filesEnded || !subdirEnded)
        return;
    
    MHWMetadata *newMetadata = [self directoryMetadata];
    MHWDirectoryChanges *changes = [[MHWDirectoryChanges alloc] initWithPreviousMetadata:self.changesStartedMetadata newMetadata:newMetadata];
    
    self.directoryIsChanging = NO;
    dispatch_async(dispatch_get_main_queue(), ^{
        if (self.changesEndedBlock)
            self.changesEndedBlock(changes);
    });
}

#pragma mark Timeout

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
                              dispatch_time(DISPATCH_TIME_NOW, self.directoryChangesTimeout * NSEC_PER_SEC),
                              DISPATCH_TIME_FOREVER, 0);
}

- (void)timedOut
{
    [self callbackIfEnded];
}

#pragma mark Sub-Watchers

- (void)watchFile:(NSString *)file
{
    __weak typeof(self) wSelf = self;
    
    NSString *fullPath = [self.watchedPath stringByAppendingPathComponent:file];
    
    BOOL isDir = NO;
    BOOL exists = [[NSFileManager defaultManager] fileExistsAtPath:fullPath isDirectory:&isDir];
    
    if (!exists)
        return;
    
    if (isDir && self.maxDepthWhenCopying > 0)
    {
        MHWDirectoryWatcher *subdirWatcher = [[self class] directoryWatcherAtPath:fullPath
                                                                 startImmediately:NO
                                                              changesStartedBlock:nil
                                                                  filesAddedBlock:nil
                                                                filesRemovedBlock:nil
                                                                changesEndedBlock:nil];
        
        __weak typeof(subdirWatcher) wWatcher = subdirWatcher;
        [subdirWatcher setChangesEndedBlock:^(MHWDirectoryChanges *changes){
            [wSelf watchedSubdirCompleted:wWatcher];
        }];
        [subdirWatcher setFileChangesTimeout:self.fileChangesTimeout];
        [subdirWatcher setDirectoryChangesTimeout:self.fileChangesTimeout];
        [subdirWatcher setMaxDepthWhenCopying:(self.maxDepthWhenCopying-1)];
        
        if ([subdirWatcher startWatching])
        {
            [subdirWatcher forceTimeout];
            
            [self.lockSubdirs lock];
            [self.subdirWatchers addObject:subdirWatcher];
            [self.lockSubdirs unlock];
        }
    }

    if (!isDir)
    {
        MHWFileWatcher *fileWatcher =
        [[MHWFileWatcher alloc] initWithPath:[self.watchedPath stringByAppendingPathComponent:file]
                                     timeout:self.fileChangesTimeout
                             completionBlock:^(MHWFileWatcher *watcher)
        {
            [wSelf watchedFileCompleted:watcher];
        }];
    
        if (fileWatcher)
        {
            [self.lockFiles lock];
            [self.fileWatchers addObject:fileWatcher];
            [self.lockFiles unlock];
        }
    }
}

- (void)watchedFileCompleted:(MHWFileWatcher *)fileWatcher
{
    [self.lockFiles lock];
    [self.fileWatchers removeObject:fileWatcher];
    [self.lockFiles unlock];
    
    NSString *path = fileWatcher.watchedPath;
    
    dispatch_async(dispatch_get_main_queue(), ^{
        if (self.filesAddedBlock)
            self.filesAddedBlock(@[path]);
    });
    
    [self callbackIfEnded];
}

- (void)watchedSubdirCompleted:(MHWDirectoryWatcher *)dirWatcher
{
    [self.lockSubdirs lock];
    [self.subdirWatchers removeObject:dirWatcher];
    [self.lockSubdirs unlock];
    
    NSString *path = dirWatcher.watchedPath;
    
    dispatch_async(dispatch_get_main_queue(), ^{
        if (self.filesAddedBlock)
            self.filesAddedBlock(@[path]);
    });
    
    [self callbackIfEnded];
}

#pragma mark Snapshots processing

- (void)processChanges
{
    MHWMetadata *newMetadata = [self directoryMetadata];
    if ([newMetadata isEqualToDictionary:self.previousMetadata])
        return;
    
    if (!self.directoryIsChanging)
    {
        self.directoryIsChanging = YES;
        self.changesStartedMetadata = newMetadata;
        
        // Tell the delegate changes have started
        dispatch_async(dispatch_get_main_queue(), ^{
            if (self.changesStartedBlock) {
                self.changesStartedBlock();
            }
        });
    }
    
    [self setTimeout];
    [self processChangesBetweenOldSnapshot:self.previousMetadata newSnapshot:newMetadata];
    self.previousMetadata = newMetadata;
}

- (void)processChangesBetweenOldSnapshot:(MHWMetadata *)oldMetadata newSnapshot:(MHWMetadata *)newMetadata
{
    NSArray <NSString *> *oldFileList = oldMetadata.allKeys;
    NSArray <NSString *> *newFileList = newMetadata.allKeys;
    
    // Deal with removed files
    {
        NSMutableArray <NSString *> *removedItems = [oldFileList mutableCopy];
        [removedItems removeObjectsInArray:newFileList];
        
        // Callback for deleted files
        if (removedItems.count)
        {
            NSMutableArray *removedPaths = [NSMutableArray array];
            for (NSString *removedItem in removedItems)
                [removedPaths addObject:[self.watchedPath stringByAppendingPathComponent:removedItem]];
            
            dispatch_async(dispatch_get_main_queue(), ^{
                if (self.filesRemovedBlock)
                    self.filesRemovedBlock(removedPaths);
            });
        }
    }
    
    // Deal with new files
    {
        NSMutableArray <NSString *> *addedItems = [newFileList mutableCopy];
        [addedItems removeObjectsInArray:oldFileList];
        
        // Watch newly added files for changes
        for (NSString *addedItem in addedItems)
            [self watchFile:addedItem];
    }
    
    // Deal with files whose content have changed
    {
        // Items already present in previous listing
        NSMutableSet *commonItemsSet = [NSMutableSet setWithArray:oldFileList];
        [commonItemsSet intersectSet:[NSSet setWithArray:newFileList]];
        
        // Remove items already being watched
        NSMutableArray *commonItems = [commonItemsSet.allObjects mutableCopy];
        for (NSString *path in self.filesBeingWrittenTo)
            [commonItems removeObject:path.lastPathComponent];
        
        for (NSString *path in self.directoriesBeingWrittenTo)
            [commonItems removeObject:path.lastPathComponent];
        
        // Start watcher for files whose content seems to have changed (different file sizes)
        for (NSString *commonItem in commonItems)
        {
            if ([oldMetadata[commonItem] isEqual:newMetadata[commonItem]])
                continue;
            
            [self watchFile:commonItem];
        }
    }
    
    [self callbackIfEnded];
}

- (MHWMetadata *)directoryMetadata
{
    MHWMetadata *metadata = [self directoryMetadataWithURL:[NSURL fileURLWithPath:self.watchedPath]
                                                  maxDepth:self.maxDepthWhenCopying];
    
    return metadata;
}

- (MHWMetadata *)directoryMetadataWithURL:(NSURL *)url maxDepth:(int)maxDepth
{
    if (maxDepth < 0)
        return nil;
    
    NSArray<NSURL *> *items =
    [[NSFileManager defaultManager] contentsOfDirectoryAtURL:url
                                  includingPropertiesForKeys:@[NSURLIsDirectoryKey, NSURLFileSizeKey]
                                                     options:NSDirectoryEnumerationSkipsSubdirectoryDescendants
                                                       error:NULL];
    
    MHWMutableMetadata *metadata = [MHWMutableMetadata dictionaryWithCapacity:items.count];
    
    NSNumber *size  = nil;
    NSNumber *isDir = nil;
    
    for (NSURL *item in items)
    {
        if (![item getResourceValue:&isDir forKey:NSURLIsDirectoryKey error:NULL])
            continue;
        if (![item getResourceValue:&size forKey:NSURLFileSizeKey error:NULL])
            continue;
        
        if (isDir.boolValue && maxDepth)
        {
            MHWMetadata *subMetadata = [self directoryMetadataWithURL:item maxDepth:(maxDepth-1)];
            [metadata setObject:subMetadata forKey:item.path.lastPathComponent];
        }
        else
        {
            [metadata setObject:@(size.unsignedIntegerValue) forKey:item.path.lastPathComponent];
        }
    }
    
    return metadata;
}

@end
