//
//  MHWFileWatcher.h
//  Created by Stan Chevallier on 29/04/2016.
//

#import <Foundation/Foundation.h>

/**
 *  "Single shot" file watcher. After changes to the watched file have stopped for more than `timeout` second(s)
 *  the watcher will be stopped and the new status will be reported via `completionBlock`.
 */
@interface MHWFileWatcher : NSObject

/**
 *  Time after to wait after last change has occured to declare the file as "done copying"
 */
@property (atomic, assign, readonly) double timeout;

/**
 *  Path of watched file
 */
@property (atomic, copy,   readonly) NSString *watchedPath;

/**
 *  Completion block, called when the file is no longer changing
 */
@property (atomic, copy,   readonly) void(^completionBlock)(MHWFileWatcher *watcher);

/**
 *  Creates a new instance of file watcher
 *
 *  @param path            Path of watched file
 *  @param timeout         Time after to wait after last change has occured to declare the file as "done copying"
 *  @param completionBlock Completion block, called when the file is no longer changing
 *
 *  @return new instance
 */
- (instancetype)initWithPath:(NSString *)path
                     timeout:(double)timeout
             completionBlock:(void(^)(MHWFileWatcher *watcher))completionBlock NS_DESIGNATED_INITIALIZER;

/**
 *  Manually stops the watcher
 */
- (void)stopWatcher;

@end
