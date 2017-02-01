//
//  SYGettextTranslation.m
//  SaneScanner
//
//  Created by Stanislas Chevallier on 01/02/2017.
//  Copyright © 2017 Syan. All rights reserved.
//

#import "SYGettextTranslation.h"

@interface SYGettextTranslation ()
@property (nonatomic, strong) NSDictionary <NSString *, NSString *> *translations;
@end

@implementation SYGettextTranslation

+ (instancetype)gettextTranslationWithContentsOfFile:(NSString *)filename
{
    NSString *content = [NSString stringWithContentsOfFile:filename encoding:NSUTF8StringEncoding error:NULL];
    if (!content)
        return nil;
    
    SYGettextTranslation *instance = [[self alloc] init];
    [instance loadFromString:content];
    return instance;
}

- (void)loadFromString:(NSString *)content
{
    NSArray <NSString *> *lines = [content componentsSeparatedByString:$$("\n")];
    
    NSString *keyToken = $$("msgid \"");
    NSString *valueToken = $$("msgstr \"");
    
    // keep only lines that start with:
    // - "
    // - msgid "
    // - msgstr "
    @autoreleasepool
    {
        NSMutableArray <NSString *> *filteredLines = [NSMutableArray arrayWithCapacity:2500];
        for (NSString *line in lines)
        {
            if ([line hasPrefix:$$("\"")] || [line hasPrefix:keyToken] || [line hasPrefix:valueToken])
            {
                [filteredLines addObject:line];
            }
        }
        lines = [filteredLines copy];
    }
    
    // strings can be on multiple lines, here we merge them.
    // we read line by line, and merge the next line as long as it starts
    // with a ", then remove all occurences of "".
    @autoreleasepool
    {
        NSMutableArray <NSString *> *mergedLines = [NSMutableArray arrayWithCapacity:2500];
        for (NSUInteger i = 0; i < lines.count; ++i)
        {
            NSString *line = lines[i];

            while (i < lines.count - 1 && [lines[i+1] hasPrefix:$$("\"")])
            {
                line = [line stringByAppendingString:lines[i+1]];
                line = [line stringByReplacingOccurrencesOfString:$$("\"\"") withString:$$("")];
                ++i;
            }
            
            [mergedLines addObject:line];
        }
        lines = [mergedLines copy];
    }
    
    // read content line by line, save key and value as they come, each
    // time we have a value we save it if its key is valid
    NSMutableDictionary <NSString *, NSString *> *translations = [NSMutableDictionary dictionaryWithCapacity:1000];
    @autoreleasepool
    {
        NSString *key;
        for (NSString *line in lines)
        {
            if ([line hasPrefix:keyToken])
            {
                key = [line substringWithRange:NSMakeRange(keyToken.length, line.length - keyToken.length - 1)];
                key = [key stringByReplacingOccurrencesOfString:$$("\\\"") withString:$$("\"")];
                continue;
            }
            
            if ([line hasPrefix:valueToken])
            {
                NSString *value = [line substringWithRange:NSMakeRange(valueToken.length, line.length - valueToken.length - 1)];
                value = [value stringByReplacingOccurrencesOfString:$$("\\\"") withString:$$("\"")];
                
                if (value.length && key.length)
                    translations[key] = value;
            }
            
            key = nil;
        }
    }
    
    self.translations = [translations copy];
}

- (NSString *)translationForKey:(NSString *)key
{
    return self.translations[key];
}

@end
