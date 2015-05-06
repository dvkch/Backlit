//
//  SYTools.h
//  SaneScanner
//
//  Created by rominet on 06/05/15.
//  Copyright (c) 2015 Syan. All rights reserved.
//

#import <Foundation/Foundation.h>

@interface SYTools : NSObject

+ (NSString *)documentsPath;
+ (NSString *)pathForFile:(NSString *)filename;
+ (NSString *)hostsFile;

@end
