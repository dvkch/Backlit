//
//  SYPreferences.h
//  SaneScanner
//
//  Created by Stan Chevallier on 11/03/2016.
//  Copyright © 2016 Syan. All rights reserved.
//

#import <Foundation/Foundation.h>
#import <SYPair.h>

typedef enum : NSUInteger {
    SYPreferenceTypeUnknown,
    SYPreferenceTypeBool,
    SYPreferenceTypeInt,
    SYPreferenceTypeString,
} SYPreferenceType;

@interface SYPreferences : NSObject

+ (SYPreferences *)shared;

@property (nonatomic) BOOL previewWithAutoColorMode;
@property (nonatomic) BOOL showAdvancedOptions;
@property (nonatomic) BOOL showIncompleteScanImages;

- (NSArray <SYPair<NSString *, NSArray <NSString *> *> *> *)allKeysGrouped;

- (NSString *)titleForKey:(NSString *)key;
- (NSString *)descriptionForKey:(NSString *)key;
- (SYPreferenceType)typeForKey:(NSString *)key;
- (id)objectForKey:(NSString *)key;
- (void)setObject:(id)object forKey:(NSString *)key;

@end
