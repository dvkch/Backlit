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
static NSString * const kPrefKey_SaveAsPNG                  = $$("SaveAsPNG");

NSString * const SYPreferencesChangedNotification   = $$("SYPreferencesChangedNotification");

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
                                          NSStringFromSelector(@selector(showIncompleteScanImages)),
                                          NSStringFromSelector(@selector(saveAsPNG)),
                                          ];
    SYPair<NSString *, NSArray <NSString *> *> *previewGroup = [SYPair pairWithObject:$("PREFERENCES SECTION PREVIEW") andObject:previewKeys];

    NSArray <NSString *> *deviceKeys = @[NSStringFromSelector(@selector(showAdvancedOptions))];
    SYPair<NSString *, NSArray <NSString *> *> *deviceGroup = [SYPair pairWithObject:$("PREFERENCES SECTION SCAN") andObject:deviceKeys];

    return @[previewGroup, deviceGroup];
}

- (NSString *)titleForKey:(NSString *)key
{
    SEL selector = NSSelectorFromString(key);
    NSAssert1([self respondsToSelector:selector], $$("The key %@ is not a valid preferences key"), key);
    
    if (selector == @selector(previewWithAutoColorMode))
        return $("PREFERENCES TITLE PREVIEW DEFAULT COLOR MODE");
    
    if (selector == @selector(showIncompleteScanImages))
        return $("PREFERENCES TITLE SHOW INCOMPLETE SCAN");
    
    if (selector == @selector(showAdvancedOptions))
        return $("PREFERENCES TITLE SHOW ADVANCED OPTIONS");
    
    if (selector == @selector(saveAsPNG))
        return $("PREFERENCES TITLE SAVE AS PNG");
    
    return nil;
}

- (NSString *)descriptionForKey:(NSString *)key
{
    SEL selector = NSSelectorFromString(key);
    NSAssert1([self respondsToSelector:selector], $$("The key %@ is not a valid preferences key"), key);
    
    if (selector == @selector(previewWithAutoColorMode))
        return $("PREFERENCES MESSAGE PREVIEW DEFAULT COLOR MODE");
    
    if (selector == @selector(showIncompleteScanImages))
        return $("PREFERENCES MESSAGE SHOW INCOMPLETE SCAN");
    
    if (selector == @selector(showAdvancedOptions))
        return $("PREFERENCES MESSAGE SHOW ADVANCED OPTIONS");
    
    if (selector == @selector(saveAsPNG))
        return $("PREFERENCES MESSAGE SAVE AS PNG");
    
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
    
    if (selector == @selector(saveAsPNG))
        return SYPreferenceTypeBool;
    
    return SYPreferenceTypeUnknown;
}

- (id)objectForKey:(NSString *)key
{
    NSAssert1([self respondsToSelector:NSSelectorFromString(key)],
              $$("The key %@ is not a valid preferences key"), key);
    
    return [self valueForKey:key];
}

- (void)setObject:(id)object forKey:(NSString *)key
{
    NSAssert1([self respondsToSelector:NSSelectorFromString(key)],
              $$("The key %@ is not a valid preferences key"), key);
    
    [self setValue:object forKey:key];
    
    [[NSNotificationCenter defaultCenter] postNotificationName:SYPreferencesChangedNotification object:nil];
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

- (BOOL)saveAsPNG
{
    [[NSUserDefaults standardUserDefaults] registerDefaults:@{kPrefKey_SaveAsPNG:@(NO)}];
    return [[NSUserDefaults standardUserDefaults] boolForKey:kPrefKey_SaveAsPNG];
}

- (void)setSaveAsPNG:(BOOL)saveAsPNG
{
    [[NSUserDefaults standardUserDefaults] setBool:saveAsPNG forKey:kPrefKey_SaveAsPNG];
    [[NSUserDefaults standardUserDefaults] synchronize];
}

@end
