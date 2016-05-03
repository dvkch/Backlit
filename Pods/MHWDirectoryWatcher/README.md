# MHWDirectoryWatcher

`MHWDirectoryWatcher` is a lightweight class that uses GCD to monitor a given path for changes.
When any change to the directory occurs, `MHWDirectoryWatcher` starts polling the monitored path, making sure that file transfers are finished before posting notifications.

### Installing
Copy `MHWDirectoryWatcher.h+m` into your project.

(or use [CocoaPods](http://cocoapods.org))

### Usage via blocks
Get an instance of `MHWDirectoryWatcher` using the method below and it will create a new watcher for the specified path immediately. Use callbacks to update your local copy of files or wait for a batch of updates to finish to update from directory listing.

Example:

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
	 *  Be carefull with added folders though, this library is not yet bullet proof when it comes to those. When using iTunes 	file
	 *  sharing to add a copy a folder to the application's sandbox some apps like iFunBox first creates the complete directory 	structure
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
	                              changesEndedBlock:(void(^)(void))changesEndedBlock;


Call `-stopWatching` / `-startWatching` to pause/resume.

### Known issues

Quick list, see doc for more details :

- if changes where started before a paused watcher has been resumed it may not detect them. You should be okay if resuming from last snapshot for files, no certainty for directories currently
- new directories are hard to watch, so `maxDepthWhenCopying` should probably be kept at 0 at the moment, stay tuned!

### Current limitations

Not watching the subdirectories that already exist, only newly created ones until they are considered no longer changing in content and up to a certain depth.


---

Used in [Kobo](https://itunes.apple.com/se/app/kobo-books/id301259483?l=en&mt=8) and [Readmill](https://itunes.apple.com/se/app/readmill-book-reader-for-epub/id438032664?l=en&mt=8) (RIP, acquired by Dropbox). 

If you like this repository and use it in your project, I'd love to hear about it!
