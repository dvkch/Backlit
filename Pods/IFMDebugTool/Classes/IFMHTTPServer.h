//
//  IFMHTTPServer.h
//  iOSFileManager
//
//  Created by John Wong on 9/7/15.
//  Copyright (c) 2015 John Wong. All rights reserved.
//

#import <CocoaHTTPServer/HTTPServer.h>

@interface IFMHTTPServer : HTTPServer

@property (nonatomic, strong) NSString *docRoot;

@end
