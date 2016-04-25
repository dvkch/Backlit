//
//  IFMDowloadFileResponse.m
//  iOSFileManager
//
//  Created by John Wong on 9/8/15.
//  Copyright (c) 2015 John Wong. All rights reserved.
//

#import "IFMDowloadFileResponse.h"

@implementation IFMDowloadFileResponse

- (NSDictionary *)httpHeaders {
    if (_fileName) {
        NSString *ext = [[_fileName pathExtension] lowercaseString];
        NSString *contentType;
        if ([@"txt" isEqualToString:ext] ||
            [@"log" isEqualToString:ext]) {
            contentType = @"text/plain";
           
        } else if ([@"json" isEqualToString:ext]) {
            contentType = @"text/json";
        } else if ([@"xml" isEqualToString:ext]) {
            contentType = @"text/xml";
        } else if ([@"html" isEqualToString:ext] ||
                   [@"htm" isEqualToString:ext]) {
            contentType = @"text/html";
        } else if ([@"js" isEqualToString:ext]) {
            contentType = @"text/javascript";
        } else if ([@"css" isEqualToString:ext]) {
            contentType = @"text/css";
        } else if ([@"jpg" isEqualToString:ext]) {
            contentType = @"image/jpeg";
        } else if ([@"jpeg" isEqualToString:ext] ||
                   [@"gif" isEqualToString:ext] ||
                   [@"png" isEqualToString:ext]) {
            contentType = [NSString stringWithFormat:@"image/%@", ext];
        }
        if (contentType.length > 0) {
            return @{
                     @"Content-Type": contentType
                     };
        } else {
            return @{
                     @"Content-Disposition": [NSString stringWithFormat:@"inline; filename=%@", _fileName]
                     };
        }
    }
    return nil;
}

@end
