//
//  SYPreferences.h
//  SaneScanner
//
//  Created by Stan Chevallier on 11/03/2016.
//  Copyright © 2016 Syan. All rights reserved.
//

#import <Foundation/Foundation.h>
#import <SYPair.h>

typedef NS_ENUM(NSUInteger, SYPreferenceType) {
    SYPreferenceTypeUnknown,
    SYPreferenceTypeBool,
    SYPreferenceTypeInt,
    SYPreferenceTypeString,
};

extern NSString * const SYPreferencesChangedNotification;

@interface SYPreferences : NSObject

@property (nonatomic, readonly, class, nonnull) SYPreferences *shared;

@property (nonatomic) BOOL previewWithAutoColorMode;
@property (nonatomic) BOOL showAdvancedOptions;
@property (nonatomic) BOOL showIncompleteScanImages;
@property (nonatomic) BOOL saveAsPNG;

- (NSArray <SYPair<NSString *, NSArray <NSString *> *> *> *)allKeysGrouped;

- (NSString *)titleForKey:(nonnull NSString *)key;
- (NSString *)descriptionForKey:(nonnull NSString *)key;
- (SYPreferenceType)typeForKey:(nonnull NSString *)key;
- (id)objectForKey:(NSString *)key;
- (void)setObject:(id)object forKey:(NSString *)key;

@end
