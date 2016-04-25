//
//  IFMHTTPServer.m
//  iOSFileManager
//
//  Created by John Wong on 9/7/15.
//  Copyright (c) 2015 John Wong. All rights reserved.
//

#import "IFMHTTPServer.h"
#import "IFMHTTPConfig.h"

@implementation IFMHTTPServer

- (IFMHTTPConfig *)config {
    IFMHTTPConfig *config = [[IFMHTTPConfig alloc] initWithServer:self documentRoot:documentRoot queue:connectionQueue];
    config.docRoot = _docRoot;
    return config;
}

@end
