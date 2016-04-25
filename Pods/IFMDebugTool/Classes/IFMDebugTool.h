//
//  IFMDebugTool.h
//  iOSFileManager
//
//  Created by John Wong on 9/7/15.
//  Copyright (c) 2015 John Wong. All rights reserved.
//

#import "IFMHTTPServer.h"

@interface IFMDebugTool : NSObject {
    IFMHTTPServer *_server;
}
/**
 *  Folder to inspect. Default is sandbox path.
 */
@property (nonatomic, assign) NSString *docRoot;
/**
 *  Host port
 */
@property (nonatomic, assign) UInt16 port;
/**
 *  Type for bonjour service
 */
@property (nonatomic, strong) NSString *type;
/**
 *  Start file manager
 */
- (void)start;
/**
 *  Complete access url
 *
 *  @return Complete access url
 */
- (NSString *)accessUrl;

/**
 * Set whether to start automaticly. Default is YES.
 *
 */
+ (void)setAutoStart:(BOOL)isAutoStart;

+ (instancetype)sharedInstance;

@end
