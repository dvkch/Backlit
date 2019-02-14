//
//  SYSaneOption.h
//  SaneScanner
//
//  Created by rominet on 06/05/15.
//  Copyright (c) 2015 Syan. All rights reserved.
//

#import <Foundation/Foundation.h>
#import "sane.h"

@class Device;
@class SYSaneOptionGroup;

NSString *NSStringFromSANE_Value_Type(SANE_Value_Type type);
NSString *NSStringFromSANE_Unit(SANE_Unit unit);

@interface SYSaneOption : NSObject

@property (nonatomic, assign) int index;
@property (nonatomic, strong) Device *device;
@property (nonatomic, strong) NSString *identifier;
@property (nonatomic, strong) NSString *localizedTitle;
@property (nonatomic, strong) NSString *localizedDesc;
@property (nonatomic, assign) SANE_Value_Type type;
@property (nonatomic, assign) SANE_Unit unit;
@property (nonatomic, assign) int size;
@property (nonatomic, assign) BOOL capSetAuto; // can be set automatically by backend
@property (nonatomic, assign) BOOL capReadable;
@property (nonatomic, assign) BOOL capEmulated;
@property (nonatomic, assign) BOOL capInactive;
@property (nonatomic, assign) BOOL capAdvanced;
@property (nonatomic, assign) BOOL capSettableViaSoftware;
@property (nonatomic, assign) BOOL capSettableViaHardware;
@property (nonatomic, assign) SANE_Constraint_Type constraintType;

+ (instancetype)bestOptionWithCOpt:(const SANE_Option_Descriptor*)opt index:(int)index device:(Device *)device;

- (instancetype)initWithCOpt:(const SANE_Option_Descriptor*)opt index:(int)index device:(Device *)device;

- (BOOL)disabledOrReadOnly;
- (BOOL)readOnlyOrSingleOption;

- (NSString *)descriptionConstraint;
- (NSString *)debugDescriptionCapabilities;
- (NSString *)debugDescriptionHuman;

- (NSString *)valueStringWithUnit:(BOOL)withUnit;
- (NSString *)stringForValue:(id)value withUnit:(BOOL)withUnit;

- (void)refreshValue:(void(^)(NSError *error))block;

+ (NSArray <SYSaneOptionGroup *> *)groupedElements:(NSArray <SYSaneOption *> *)elements
                                 removeEmptyGroups:(BOOL)removeEmptyGroups;

@end
