//
//  SYTools.m
//  SaneScanner
//
//  Created by rominet on 06/05/15.
//  Copyright (c) 2015 Syan. All rights reserved.
//

#import "SYTools.h"
#import "mach/mach.h"
#import "SaneScanner-Swift.h"
#import <SYKit-Swift.h>

vm_size_t usedMemory(void) {
    struct task_basic_info info;
    mach_msg_type_number_t size = sizeof(info);
    kern_return_t kerr = task_info(mach_task_self(), TASK_BASIC_INFO, (task_info_t)&info, &size);
    return (kerr == KERN_SUCCESS) ? info.resident_size : 0; // size in bytes
}

vm_size_t freeMemory(void) {
    mach_port_t host_port = mach_host_self();
    mach_msg_type_number_t host_size = sizeof(vm_statistics_data_t) / sizeof(integer_t);
    vm_size_t pagesize;
    vm_statistics_data_t vm_stat;
    
    host_page_size(host_port, &pagesize);
    (void) host_statistics(host_port, HOST_VM_INFO, (host_info_t)&vm_stat, &host_size);
    return vm_stat.free_count * pagesize;
}

void logMemUsage(void) {
    // compute memory usage and log if different by >= 100k
    static long prevMemUsage = 0;
    long curMemUsage = usedMemory();
    long memUsageDiff = curMemUsage - prevMemUsage;
    
    if (memUsageDiff > 100000 || memUsageDiff < -100000) {
        prevMemUsage = curMemUsage;
        NSLog($$("Memory used %7.1f (%+5.0f), free %7.1f kb"), curMemUsage/1000.0f, memUsageDiff/1000.0f, freeMemory()/1000.0f);
    }
}

@implementation SYTools

+ (NSString *)documentsPath
{
    static dispatch_once_t onceToken;
    static NSString *path = nil;
    dispatch_once(&onceToken, ^{
        NSArray *paths = NSSearchPathForDirectoriesInDomains(NSDocumentDirectory, NSUserDomainMask, YES);
        path = paths.firstObject;
    });
    return path;
}

+ (NSString *)cachePath
{
    static dispatch_once_t onceToken;
    static NSString *path = nil;
    dispatch_once(&onceToken, ^{
        path = NSSearchPathForDirectoriesInDomains(NSCachesDirectory, NSUserDomainMask, YES).lastObject;
    });
    return path;
}

+ (NSString *)appSupportPath:(BOOL)create
{
    NSArray *paths = NSSearchPathForDirectoriesInDomains(NSApplicationSupportDirectory, NSUserDomainMask, YES);
    NSString *path = [paths.firstObject stringByAppendingPathComponent:$$("SaneScanner")];

    if (create && ![[NSFileManager defaultManager] fileExistsAtPath:path isDirectory:NULL])
    {
        [[NSFileManager defaultManager] createDirectoryAtPath:path
                                  withIntermediateDirectories:YES
                                                   attributes:nil
                                                        error:NULL];
    }
    
    return path;
}

+ (NSString *)pathForFile:(NSString *)filename
{
    return [[self documentsPath] stringByAppendingPathComponent:filename];
}

+ (void)createTestImages:(NSUInteger)count
{
    for (NSUInteger i = 0; i < count; ++i)
    {
        // https://gist.github.com/kylefox/1689973
        CGFloat hue = ( arc4random() % 256 / 256.0 );  //  0.0 to 1.0
        CGFloat saturation = ( arc4random() % 128 / 256.0 ) + 0.5;  //  0.5 to 1.0, away from white
        CGFloat brightness = ( arc4random() % 128 / 256.0 ) + 0.5;  //  0.5 to 1.0, away from black
        UIColor *color = [UIColor colorWithHue:hue saturation:saturation brightness:brightness alpha:1];
        
        UIImage *image = [UIImage sy_imageWithColor:color size:CGSizeMake(10, 10) cornerRadius:0];
        NSString *path = [SYTools pathForFile:[NSString stringWithFormat:$$("testimage-%@.jpg"), [[NSUUID UUID] UUIDString]]];
        [UIImageJPEGRepresentation(image, JPEG_COMP) writeToFile:path atomically:YES];
    }
}

+ (NSDateFormatter *)logDateFormatter
{
    static dispatch_once_t onceToken;
    static NSDateFormatter *formatter;
    dispatch_once(&onceToken, ^{
        formatter = [[NSDateFormatter alloc] init];
        [formatter setDateFormat:$$("yyyy-MM-dd HH:mm:ss.SSS")];
        [formatter setLocale:[NSLocale localeWithLocaleIdentifier:$$("en_US_POSIX")]];
    });
    return formatter;
}

@end
