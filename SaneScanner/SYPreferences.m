//
//  SYPreferences.m
//  SaneScanner
//
//  Created by Stan Chevallier on 11/03/2016.
//  Copyright © 2016 Syan. All rights reserved.
//

#import "SYPreferences.h"

static NSString * const kPrefKey_PreviewWithAutoColorMode   = $$("PreviewWithAutoColorMode");
static NSString * const kPrefKey_ShowAdvancedOptions        = $$("ShowAdvancedOptions");
static NSString * const kPrefKey_ShowIncompleteScanImages   = $$("ShowIncompleteScanImages");

@implementation SYPreferences

+ (SYPreferences *)shared
{
    static dispatch_once_t onceToken;
    static SYPreferences *sharedInstance;
    dispatch_once(&onceToken, ^{
        sharedInstance = [[self alloc] init];
    });
    return sharedInstance;
}

- (instancetype)init
{
    self = [super init];
    if (self)
    {
    }
    return self;
}

#pragma mark - Generic methods

- (NSArray<SYPair<NSString *,NSArray<NSString *> *> *> *)allKeysGrouped
{
    NSArray <NSString *> *previewKeys = @[NSStringFromSelector(@selector(previewWithAutoColorMode)),
                                          NSStringFromSelector(@selector(showIncompleteScanImages))];
    SYPair<NSString *, NSArray <NSString *> *> *previewGroup = [SYPair pairWithObject:@"Scan" andObject:previewKeys];

    NSArray <NSString *> *deviceKeys = @[NSStringFromSelector(@selector(showAdvancedOptions))];
    SYPair<NSString *, NSArray <NSString *> *> *deviceGroup = [SYPair pairWithObject:@"Preview" andObject:deviceKeys];

    return @[deviceGroup, previewGroup];
}

- (NSString *)titleForKey:(NSString *)key
{
    SEL selector = NSSelectorFromString(key);
    NSAssert1([self respondsToSelector:selector], $$("The key %@ is not a valid preferences key"), key);
    
    if (selector == @selector(previewWithAutoColorMode))
        return @"Preview with default color mode";
    
    if (selector == @selector(showIncompleteScanImages))
        return @"Show incomplete scan image";
    
    if (selector == @selector(showAdvancedOptions))
        return @"Show advanced options";

    return nil;
}

- (NSString *)descriptionForKey:(NSString *)key
{
    SEL selector = NSSelectorFromString(key);
    NSAssert1([self respondsToSelector:selector], $$("The key %@ is not a valid preferences key"), key);
    
    if (selector == @selector(previewWithAutoColorMode))
        return @"Will use the default color mode, if auto value is supported and option is found. Allows for more consistent render accross previews, whatever the option you set, but can reduce preview speed and reset other options depending on the device.";

    if (selector == @selector(showIncompleteScanImages))
        return @"If enabled the incomplete image during a scan operation (preview or actual scan) will be displayed on screen. On an old device or when scanning with a high resolution this is not recommended.";
    
    if (selector == @selector(showAdvancedOptions))
        return @"Some devices have advanced options, toggling this setting ON will allow you to access them.";

    return nil;
}

- (SYPreferenceType)typeForKey:(NSString *)key
{
    SEL selector = NSSelectorFromString(key);
    NSAssert1([self respondsToSelector:selector], $$("The key %@ is not a valid preferences key"), key);
    
    if (selector == @selector(previewWithAutoColorMode))
        return SYPreferenceTypeBool;
    
    if (selector == @selector(showIncompleteScanImages))
        return SYPreferenceTypeBool;
    
    if (selector == @selector(showAdvancedOptions))
        return SYPreferenceTypeBool;
    
    return SYPreferenceTypeUnknown;
}

- (id)objectForKey:(NSString *)key
{
    SEL selector = NSSelectorFromString(key);
    NSAssert1([self respondsToSelector:selector], $$("The key %@ is not a valid preferences key"), key);
    return [self valueForKey:key];
}

- (void)setObject:(id)object forKey:(NSString *)key
{
    SEL selector = NSSelectorFromString(key);
    NSAssert1([self respondsToSelector:selector], $$("The key %@ is not a valid preferences key"), key);
    
    [self setValue:object forKey:key];
}

#pragma mark - Preferences

- (BOOL)previewWithAutoColorMode
{
    [[NSUserDefaults standardUserDefaults] registerDefaults:@{kPrefKey_PreviewWithAutoColorMode:@(YES)}];
    return [[NSUserDefaults standardUserDefaults] boolForKey:kPrefKey_PreviewWithAutoColorMode];
}

- (void)setPreviewWithAutoColorMode:(BOOL)previewWithAutoColorMode
{
    [[NSUserDefaults standardUserDefaults] setBool:previewWithAutoColorMode forKey:kPrefKey_PreviewWithAutoColorMode];
    [[NSUserDefaults standardUserDefaults] synchronize];
}

- (BOOL)showAdvancedOptions
{
    [[NSUserDefaults standardUserDefaults] registerDefaults:@{kPrefKey_ShowAdvancedOptions:@(NO)}];
    return [[NSUserDefaults standardUserDefaults] boolForKey:kPrefKey_ShowAdvancedOptions];
}

- (void)setShowAdvancedOptions:(BOOL)showAdvancedOptions
{
    [[NSUserDefaults standardUserDefaults] setBool:showAdvancedOptions forKey:kPrefKey_ShowAdvancedOptions];
    [[NSUserDefaults standardUserDefaults] synchronize];
}

- (BOOL)showIncompleteScanImages
{
    [[NSUserDefaults standardUserDefaults] registerDefaults:@{kPrefKey_ShowIncompleteScanImages:@(YES)}];
    return [[NSUserDefaults standardUserDefaults] boolForKey:kPrefKey_ShowIncompleteScanImages];
}

- (void)setShowIncompleteScanImages:(BOOL)showIncompleteScanImages
{
    [[NSUserDefaults standardUserDefaults] setBool:showIncompleteScanImages forKey:kPrefKey_ShowIncompleteScanImages];
    [[NSUserDefaults standardUserDefaults] synchronize];
}

@end
