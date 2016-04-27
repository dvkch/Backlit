/*
 *  MHWDirectoryWatcher.h
 *  Created by Martin Hwasser on 12/19/11.
 */

#import <Foundation/Foundation.h>

@interface MHWDirectoryWatcher : NSObject

// Returns an initialized MHWDirectoryWatcher and begins to watch the path, if specified
+ (MHWDirectoryWatcher *)directoryWatcherAtPath:(NSString *)watchedPath
                               startImmediately:(BOOL)startImmediately
                                 changesStarted:(void(^)(void))changesStarted
                                   changesEnded:(void(^)(BOOL allChangesFinished))changesEnded;

// Returns YES if started watching, NO if already is watching
- (BOOL)startWatching;

// Returns YES if stopped watching, NO if not watching
- (BOOL)stopWatching;

// The path being watched
@property (atomic, readonly, copy) NSString *watchedPath;

// Time interval between two polls in seconds. Polls are used to determine if a write has been finished
@property (atomic, assign) double pollInterval;

// Number of polls needed to confirm no more changes are being made into the watched directory
@property (atomic, assign) int pollRetryCount;

// Polling can be done in two different ways :
// - check if file list and file sizes are the same, until all files
//   haven't completed the changesEnded callback won't be called
// - check independently each file for presence and size, report when
//   some changes have finished, for instance if files have been deleted
//   the callback will be called instantly, even if files are being written
//   to and are not safe to use yet. Those files can be known via `filesBeingWrittenTo`
//
// The first method will be quicker on large directories, but no changes will
// be reported to the callback until all changes are finished which may take some time
@property (atomic, assign) BOOL useQuickerPoll;

// Files currently being written to
- (NSArray<NSString *> *)filesBeingWrittenTo;

@end
