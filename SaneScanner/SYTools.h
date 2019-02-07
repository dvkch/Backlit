//
//  SYTools.h
//  SaneScanner
//
//  Created by rominet on 06/05/15.
//  Copyright (c) 2015 Syan. All rights reserved.
//

#import <UIKit/UIKit.h>

void logMemUsage(void);

@interface SYTools : NSObject

@property (class, readonly, nonnull) NSString *documentsPath;
@property (class, readonly, nonnull) NSString *cachePath;

+ (NSString *)appSupportPath:(BOOL)create;
+ (NSString *)pathForFile:(NSString *)filename;
+ (void)createTestImages:(NSUInteger)count;
+ (NSDateFormatter *)logDateFormatter;


@end
