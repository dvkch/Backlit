//
//  SYTools.m
//  SaneScanner
//
//  Created by rominet on 06/05/15.
//  Copyright (c) 2015 Syan. All rights reserved.
//

#import "SYTools.h"

// https://gist.github.com/steipete/6ee378bd7d87f276f6e0
BOOL NSObjectIsBlock(id object)
{
    static Class blockClass;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        blockClass = [^{} class];
        while ([blockClass superclass] != NSObject.class) {
            blockClass = [blockClass superclass];
        }
    });
    
    return [object isKindOfClass:blockClass];
}

@implementation SYTools

+ (NSString *)documentsPath
{
    NSArray *paths = NSSearchPathForDirectoriesInDomains(NSDocumentDirectory, NSUserDomainMask, YES);
    return ([paths count] ? paths[0] : nil);
}

+ (NSString *)pathForFile:(NSString *)filename
{
    return [[self documentsPath] stringByAppendingPathComponent:filename];
}

+ (NSString *)hostsFile
{
    return [self pathForFile:@"hosts.cfg"];
}

@end
