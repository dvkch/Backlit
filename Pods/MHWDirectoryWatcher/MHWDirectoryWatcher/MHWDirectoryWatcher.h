/*
 *  MHWDirectoryWatcher.h
 *  Created by Martin Hwasser on 12/19/11.
 */

#import <Foundation/Foundation.h>

@interface MHWDirectoryChanges : NSObject

@property (nonatomic, strong, readonly) NSArray <NSString *> *addedFiles;
@property (nonatomic, strong, readonly) NSArray <NSString *> *removedFiles;

@end

@interface MHWDirectoryWatcher : NSObject

/**
 *  Creates a new instance of `MHWDirectoryWatcher`.
 *
 *  @param watchedPath         path of directory to watch for changes
 *  @param startImmediately    immediately start watching directory after instance has been created
 *  @param changesStartedBlock block to be called when a batch of changes starts
 *  @param filesAddedBlock     block to be called when new files are detected & are presumed "done copying"
 *  @param filesRemovedBlock   block to be called when files have been deleted
 *  @param changesEndedBlock   block to be called when the batch of operations has finished
 *
 *  The `filesAddedBlock` and `filesRemovedBlock` will always be called between `changesStartedBlock` and `changesEndedBlock`,
 *  allowing you for instance to show an HUD or some other kind of activity indicator to your user while the directory is being
 *  modified.
 *
 *  Be carefull with added folders though, this library is not yet bullet proof when it comes to those. When using iTunes file
 *  sharing to add a copy a folder to the application's sandbox some apps like iFunBox first creates the complete directory structure
 *  then add files, while iTunes creates directories right before adding files to them.
 *
 *  For now this library tries to watch newly created subfolders for changes, and once a first "batch" of changes is finished (no
 *  changes for `fileChangesTimeout` seconds && no watched files or subdirectories) the directory is considered "done copying"
 *  and future changes are ignored. This strategy works when files are copied via iTunes but not iFunBox since no changes occur
 *  in the newly created folders before the batch times out and the folder is then declared "done copying" before files are even
 *  copied to it.
 *
 *  A future strategy will be to always watch subdirectories (up to a certain depth) for changed content, which should allow the top
 *  level one to be declared "still copying" while all subfolders are done with their changes.
 *
 *  @return new instance
 */
+ (MHWDirectoryWatcher *)directoryWatcherAtPath:(NSString *)watchedPath
                               startImmediately:(BOOL)startImmediately
                            changesStartedBlock:(void(^)(void))changesStartedBlock
                                filesAddedBlock:(void(^)(NSArray <NSString *> *addedFiles))filesAddedBlock
                              filesRemovedBlock:(void(^)(NSArray <NSString *> *removedFiles))filesRemovedBlock
                              changesEndedBlock:(void(^)(MHWDirectoryChanges *changes))changesEndedBlock;


/**
 *  Called when changes are detected
 */
@property (atomic, copy) void(^changesStartedBlock)(void);

/**
 *  Called when a new file has been added to the watched folder, will not be called until the file is supposedly no longer being edited.
 *  @see filesChangesTimeout
 */
@property (atomic, copy) void(^filesAddedBlock)(NSArray <NSString *> *addedFiles);

/**
 *  Called when a file has been removed from the directory
 */
@property (atomic, copy) void(^filesRemovedBlock)(NSArray <NSString *> *filesRemoved);

/**
 *  Called when all changes in the folder are finished
 */
@property (atomic, copy) void(^changesEndedBlock)(MHWDirectoryChanges *changes);

/**
 *  The path being watched
 */
@property (atomic, readonly, copy) NSString *watchedPath;

/**
 *  Time interval for which a file must not change to be considered available (e.g. writer has finished writing to it)
 *  This also applies to subdirectories that are watched while being copied.
 *  Default value is 0.5s
 */
@property (atomic, assign) double fileChangesTimeout;

/**
 *  Time interval for which this directory's content must not change to call the `changesEndedBlock` callback. Default value is 0.1s
 *
 *  The best value for this property depends on your use of the `changesStartedBlock` and `changesEndedBlock` callbacks.
 *
 *  When deleting a lot of files using a value greater than zero (e.g. 0.2s) will help grouping deletions to a single batch, meaning all 
 *  deletions have a far greater chance of being completed before `changesEndedBlock` is called.
 *
 *  In the same scenario, but with a value of 0 deletions should be reported in multiple batches, sometimes even 1 batch / deletion.
 *
 *  Note: in no case will this influence the frequency at which `filesRemovedBlock` or `filesAddedBlock` are called.
 */
@property (atomic, assign) double directoryChangesTimeout;

/**
 *  Depth to which we watch changes for a **new** folder
 *
 *  This is used to emulate the "done copying" behaviour used for files. In the same way that changes to a single file
 *  won't be detected after it's been written for the first time, no changes made to a subdirectory after it's been fully
 *  copied will be reported. This is in no way a permanent watch of subdirectories. 
 *
 *  For instance: if set to 0, the new subdirectories of watchedPath will be considered "done copying" even if someone is still copying items in them
 *  If it is set to 1 you'll know when the files added to the subdirectories are no longer being edited, but not for the eventual subdirectories, etc.
 *
 *  Note: if the app is sent to the background when a subdirectory is being copied to the watched folder, then restarted while the same copy is not finished,
 *  there is no way of knowing when the copy will be finished
 */
@property (atomic, assign) int maxDepthWhenCopying;

/**
 *  Starts watching the selected directory. Similar to passing `NO` to `startWatchingFromLastSnapshot:`
 *
 *  @return `YES` if started watching, `NO` if already is watching
 */
- (BOOL)startWatching;

/**
 *  Starts watching the selected directory
 *
 *  @param useLastSnapshot
 *  When passing `YES` the last snapshot of the directory will be used to determine changes made since last time it was running. This may be useful when resuming from background. When starting the watcher for the first time this will have no impact.
 *  When passing `NO` a new snapshot is made and changes are determined only from there.
 *  Note that a snapshot is never persisted to the disk, a new one is always made when starting the app.
 *
 *  @discussion
 *  If a file is being modified when you start the watcher you may not be notified because the directory doesn't receive any events for a file other than
 *  added/removed. THis being said when setting `useLastSnapshot` to `YES` you have far better chances to detect those changes.
 *
 *  @return `YES` if started watching, `NO` if already is watching
 */
- (BOOL)startWatchingFromLastSnapshot:(BOOL)useLastSnapshot;

/**
 *  Stops watching directory. All files being watched (e.g. still being written to this folder) won't be any longer.
 *  No callbacks will be called.
 */
- (void)stopWatching;

/**
 *  Files whose size are still changing, so not "done copying"
 *
 *  @return List of files still changing
 */
- (NSArray<NSString *> *)filesBeingWrittenTo;

/**
 *  Directories whose files are still changing, so not "done copying"
 *
 *  @return List of directories still changing
 */
- (NSArray<NSString *> *)directoriesBeingWrittenTo;

@end
