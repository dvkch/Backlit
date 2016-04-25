//
//  IFMFileListConnection.m
//  iOSFileManager
//
//  Created by John Wong on 9/5/15.
//  Copyright (c) 2015 John Wong. All rights reserved.
//

#import "IFMFileListConnection.h"
#import <CocoaHTTPServer/HTTPFileResponse.h>
#import <CocoaHTTPServer/HTTPDynamicFileResponse.h>
#import "IFMHTTPConfig.h"
#import "IFMDowloadFileResponse.h"
#import "HTTPMessage.h"
#import <CocoaHTTPServer/HTTPDataResponse.h>

@interface IFMURL : NSObject

@property (nonatomic, strong) NSString *path;
@property (nonatomic, strong) NSDictionary *params;

+ (instancetype)instanceFromURL:(NSString *)url;

@end

@implementation IFMURL

+ (instancetype)instanceFromURL:(NSString *)url {
    IFMURL *instance = [[IFMURL alloc] init];
    NSRange range = [url rangeOfString:@"?"];
    if (range.location == NSNotFound) {
        instance.path = url;
    } else {
        instance.path = [url substringToIndex:range.location];
        NSString *params = [url substringFromIndex:range.location + 1];
        NSMutableDictionary *dict = [NSMutableDictionary dictionary];
        for (NSString *str in [params componentsSeparatedByString:@"&"]) {
            NSArray *kv = [str componentsSeparatedByString:@"="];
            if (kv.count == 2) {
                NSString *key = kv[0];
                NSString *value = kv[1];
                if ([key length] > 0 && [value length] > 0) {
                    NSString *newValue;
                    if ([value containsString:@"%"]) {
                        newValue = [value stringByReplacingPercentEscapesUsingEncoding:NSUTF8StringEncoding];
                    }
                    dict[key] = newValue.length > 0? newValue : value;
                }
            }
        }
        instance.params = [dict copy];
    }
    return instance;
}

@end

@implementation IFMFileListConnection

static NSString *const kActionShow = @"/open/";
static NSString *const kActionDelete = @"/delete/";

- (NSString *)docRoot {
    return ((IFMHTTPConfig *)config).docRoot;
}

- (NSArray *)fileList:(NSString *)path {
    NSFileManager *fileManager = [NSFileManager defaultManager];
    NSMutableArray *mutableArray = [NSMutableArray array];
    NSString *absoluteParent = [[self docRoot] stringByAppendingPathComponent:path];
    for (NSString *fileName in [fileManager contentsOfDirectoryAtPath:absoluteParent error:nil]) {
        NSString *relativePath = [path stringByAppendingPathComponent:fileName];
        NSString *absolutePath = [absoluteParent stringByAppendingPathComponent:fileName];
        BOOL isDictionary;
        if ([fileManager fileExistsAtPath:absolutePath isDirectory:&isDictionary]) {
            NSMutableDictionary *item = [[NSMutableDictionary alloc] init];
            item[@"iden"] = relativePath;
            item[@"name"] = fileName;
            NSDictionary *attributes = [fileManager attributesOfItemAtPath:absolutePath error:nil];
            if (attributes.fileSize < 1024) {
                item[@"size"] = [NSString stringWithFormat:@"%@B", @(attributes.fileSize)];
            } else if (attributes.fileSize < 1024 * 1024) {
                item[@"size"] = [NSString stringWithFormat:@"%.1fK", attributes.fileSize / 1024.0];
            } else {
                item[@"size"] = [NSString stringWithFormat:@"%.1fM", attributes.fileSize / 1024.0 / 1024.0];
            }
            item[@"owner"] = [NSString stringWithFormat:@"%@/%@", [attributes fileOwnerAccountName]?:@"--", [attributes fileGroupOwnerAccountName]?:@""];
            NSUInteger permission = [attributes filePosixPermissions];
            NSString *permissionDesc = [NSString stringWithFormat:@"%@%@%@%@%@%@%@%@%@%@",
                              [NSFileTypeSymbolicLink isEqualToString:[attributes fileType]]?@"l":@"-",
                              permission & 1<<8? @"r": @"-",
                              permission & 1<<7? @"w": @"-",
                              permission & 1<<6? @"x": @"-",
                              permission & 1<<5? @"r": @"-",
                              permission & 1<<4? @"w": @"-",
                              permission & 1<<3? @"x": @"-",
                              permission & 1<<2? @"r": @"-",
                              permission & 1<<1? @"w": @"-",
                              permission & 1<<0? @"x": @"-"];
                              
            
            item[@"permi"] = permissionDesc;
            NSDateFormatter *formater = [[NSDateFormatter alloc] init];
            [formater setDateFormat:@"yyyy-MM-dd HH:mm:ss"];
            
            item[@"creat"] = [formater stringFromDate:[attributes fileCreationDate]];
            item[@"modif"] = [formater stringFromDate:[attributes fileModificationDate]];
            
            if (isDictionary) {
                item[@"type"] = @"d";
                item[@"subs"] = [self fileList:relativePath];
            } else {
                item[@"type"] = @"f";
            }
            [mutableArray addObject:item];
        }
    }
    return [mutableArray copy];
}

- (NSObject<HTTPResponse> *)httpResponseForMethod:(NSString *)method URI:(NSString *)path
{
    // Use HTTPConnection's filePathForURI method.
    // This method takes the given path (which comes directly from the HTTP request),
    // and converts it to a full path by combining it with the configured document root.
    //
    // It also does cool things for us like support for converting "/" to "/index.html",
    // and security restrictions (ensuring we don't serve documents outside configured document root folder).
    
    IFMURL *url = [IFMURL instanceFromURL:path];
    if ([url.path hasPrefix:kActionShow] || [url.path hasPrefix:kActionDelete]) {
        NSString *filePath = [url.path substringFromIndex:kActionShow.length];
        if ([filePath rangeOfString:@"%"].location != NSNotFound) {
            filePath = [filePath stringByReplacingPercentEscapesUsingEncoding:NSUTF8StringEncoding]?:filePath;
        }
        NSString *docRoot = [self docRoot];
        filePath = [docRoot stringByAppendingPathComponent:filePath];
        
        if ([url.path hasPrefix:kActionShow]) {
            HTTPFileResponse *response = [[HTTPFileResponse alloc] initWithFilePath:filePath forConnection:self];
            return response;
        } else if ([url.path hasPrefix:kActionDelete]) {
            NSError *error;
            NSString *result = [[NSFileManager defaultManager] removeItemAtPath:filePath error:&error]? @"success": error.description;
            return [[HTTPDataResponse alloc] initWithData:[result dataUsingEncoding:NSUTF8StringEncoding]];
        }
    }
    
    NSString *filePath = [self filePathForURI:path];
    
    // Convert to relative path
    
    NSString *documentRoot = [config documentRoot];
    
    
    if (![filePath hasPrefix:documentRoot])
    {
        // Uh oh.
        // HTTPConnection's filePathForURI was supposed to take care of this for us.
        return nil;
    }
    
    NSString *relativePath = [filePath substringFromIndex:[documentRoot length]];
    
    if ([relativePath isEqualToString:@"/index.html"] || [relativePath isEqualToString:@"/"])
    {
        NSArray *fileList = [self fileList: @""];
        NSError *error = nil;
        NSData *json = [NSJSONSerialization dataWithJSONObject:fileList options:NSJSONWritingPrettyPrinted error:&error];
        NSDictionary *replacement = @{
                                      @"DATA": [[NSString alloc] initWithData:json encoding:NSUTF8StringEncoding]
                                      };
        return [[HTTPDynamicFileResponse alloc] initWithFilePath: filePath
                                                                                forConnection: self
                                                                                    separator: @"%%"
                                                                        replacementDictionary: replacement];
    }
    return [super httpResponseForMethod:method URI:path];
}

- (NSData *)preprocessErrorResponse:(HTTPMessage *)response
{
    NSString *message = [NSString stringWithFormat:@"Error: %@", @(response.statusCode)];
    NSData *msgData = [message dataUsingEncoding:NSUTF8StringEncoding];
    [response setBody:msgData];
    NSString *contentLengthStr = [NSString stringWithFormat:@"%lu", (unsigned long)[msgData length]];
    [response setHeaderField:@"Content-Length" value:contentLengthStr];
    return [super preprocessErrorResponse:response];
}

@end
