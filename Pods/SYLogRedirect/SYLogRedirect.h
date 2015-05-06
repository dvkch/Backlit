//
//  SYLogRedirect.h
//  Syan
//
//  Created by rominet on 05/07/12.
//  Copyright (c) 2012 __MyCompanyName__. All rights reserved.
//

#import <Foundation/Foundation.h>

extern NSInteger const SYLogType_AlwaysLog;

///---------------------------------------------------------------------------------------
/// @name Functions
///---------------------------------------------------------------------------------------

/** Writes a line to the log file
 @param type Value to be compared to a mask of enabled types
 @param showDate Controls the presence of the date on the beginning of the line to write
 @param format String to log as a format
 */
void SYLog(NSInteger type, BOOL showDate, NSString *format, ...);

/** Writes a line to the log file
 @param type Value to be compared to a mask of enabled types
 @param showDate Controls the presence of the date on the beginning of the line to write
 @param file Source file name, pass `__FILE__` as argument
 @param line Source line number, pass `__LINE__` as argument
 @param function Source function, pass `__PRETTY_FUNCTION__` as argument
 @param format String to log as a format
 */
void SYLogWithFileLineAndMethod(NSInteger type, BOOL showDate, const char* file, int line, const char* function, NSString *format, ...);

/** Enable/disable log redirection. Enabled by default
 @param enabled New enable status
 */
void SYLogSetEnabled(BOOL enabled);

/** Defines enabled types mask. By default: -1, aka all types
 @param enabledTypesMask enabled types mask
 */
void SYLogSetEnabledTypesMask(NSInteger enabledTypesMask);

/** To get the log path
 @param nilIfDisabled If set to `YES` and logging is disabled the method will return `nil`
 @return log file path
 */
NSString* SYLogFilePath(BOOL nilIfDisabled);

/** Empties log file and reopens it to continue logging immediately */
void SYLogClear();

///---------------------------------------------------------------------------------------
/// @name Redirect Macros
///---------------------------------------------------------------------------------------

/** Redirects SYLogFull to SYLogWithFileLineAndMethod. */
#define SYLogFull(type, showDate, args...) SYLogWithFileLineAndMethod(type, showDate, __FILE__, __LINE__, __PRETTY_FUNCTION__, args)

/** Redirects NSLog to SYLog. */
#ifdef DEFINE_NSLOG_AS_SYLOG
#define NSLog(args...) {NSLog(args);SYLog(SYLogType_AlwaysLog, YES, args);}
#endif

/** Redirects NSLog to SYLogFull. */
#ifdef DEFINE_NSLOG_AS_SYLOGFULL
#define NSLog(args...) {NSLog(args);SYLogFull(SYLogType_AlwaysLog, YES, args);}
#endif
