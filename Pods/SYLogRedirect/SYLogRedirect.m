//
//  SYLogRedirect.m
//  Syan
//
//  Created by Stanislas Chevallier on 05/07/12.
//  Copyright (c) 2012 Syan. All rights reserved.
//

#import "SYLogRedirect.h"

NSInteger const SYLogType_AlwaysLog = -1;

/* Static file handle to access log */
static NSFileHandle* _fileHandle;
/* Grand central dispatch queue for sync */
static dispatch_queue_t _queue;
/* Date formatter */
static NSDateFormatter *_dateFormatter;
/* Enabled types mask */
static NSInteger _enabledTypesMask = SYLogType_AlwaysLog;
/* Log enabled */
static BOOL _enabled = YES;

dispatch_queue_t SYLogDispatchQueue()
{
    if(!_queue)
        _queue = dispatch_queue_create([@"SYLogRedirect" UTF8String], DISPATCH_QUEUE_PRIORITY_DEFAULT);
    
    return _queue;
}

NSDateFormatter *SYLogDateFormatter()
{
    if(!_dateFormatter)
    {
        _dateFormatter = [[NSDateFormatter alloc] init];
        [_dateFormatter setDateFormat:@"yyyy-MM-dd' 'HH:mm:ss.SSS' '"];
    }
    
    return _dateFormatter;
}

NSString* SYLogFilePath(BOOL nilIfDisabled)
{
    if(nilIfDisabled && !_enabled)
        return nil;
    
    NSString *path = nil;
#if TARGET_OS_IPHONE
    path = NSSearchPathForDirectoriesInDomains(NSDocumentDirectory, NSUserDomainMask, YES)[0];
#elif TARGET_OS_MAC
    path = [NSSearchPathForDirectoriesInDomains(NSApplicationSupportDirectory, NSUserDomainMask, YES)[0]
            stringByAppendingPathComponent:@"/Yuppie"];
#endif
    
    return [path stringByAppendingPathComponent:@"/log.txt"];
}

void SYLogSetEnabled(BOOL enabled)
{
    _enabled = enabled;
}

void SYLogOpenFile(BOOL overwrite)
{
    dispatch_sync(SYLogDispatchQueue(), ^{
        NSString *path = SYLogFilePath(NO);
        
        // Create log file if not here
        NSFileManager *fm = [NSFileManager defaultManager];
        if(![fm fileExistsAtPath:path] || overwrite)
            [@"" writeToFile:path atomically:YES encoding:NSUTF8StringEncoding error:nil];
        
        // creating file handle
        _fileHandle = [NSFileHandle fileHandleForUpdatingAtPath:path];
        
        if(_fileHandle)
            [_fileHandle seekToEndOfFile];
        else
            printf("Couldn't open log file at path %s\n", [SYLogFilePath(NO) UTF8String]);
    });
}

void SYLog(NSInteger type, BOOL showDate, NSString *format, ...)
{
    if(!_enabled)
        return;
    
    if(type != SYLogType_AlwaysLog && _enabledTypesMask != SYLogType_AlwaysLog && !(_enabledTypesMask & type))
        return;
    
    if(!_fileHandle)
        SYLogOpenFile(NO);
    
    NSString *date = (showDate ? [SYLogDateFormatter() stringFromDate:[NSDate date]] : @"");
    
    va_list args;
    va_start(args, format);
    NSString *string = [[NSString alloc] initWithFormat:format arguments:args];
    va_end(args);
    
    dispatch_sync(_queue, ^{
        [_fileHandle writeData:[[NSString stringWithFormat:@"%@%@\n", date, string] dataUsingEncoding:NSUTF8StringEncoding]];
    });
}

void SYLogWithFileLineAndMethod(NSInteger type, BOOL showDate, const char* file, int line, const char* function, NSString *format, ...)
{
    NSString *string0 = [NSString stringWithFormat:@"\n%s:%d - %s", file, line, function];
    
    va_list args;
    va_start(args, format);
    NSString *string1 = [[NSString alloc] initWithFormat:format arguments:args];
    va_end(args);
    
    SYLog(type, NO,       string0);
    SYLog(type, showDate, string1);
}

void SYLogClear(void)
{
    [_fileHandle closeFile];
    SYLogOpenFile(YES);
}

