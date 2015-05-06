//
//  SYTools.m
//  SaneScanner
//
//  Created by rominet on 06/05/15.
//  Copyright (c) 2015 Syan. All rights reserved.
//

#import "SYTools.h"

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

@end
