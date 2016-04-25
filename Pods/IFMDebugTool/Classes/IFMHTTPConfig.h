//
//  IFMHTTPConfig.h
//  iOSFileManager
//
//  Created by John Wong on 9/7/15.
//  Copyright (c) 2015 John Wong. All rights reserved.
//

#import <CocoaHttpServer/HTTPConnection.h>

@interface IFMHTTPConfig : HTTPConfig
    
@property (nonatomic, strong) NSString *docRoot;

@end
