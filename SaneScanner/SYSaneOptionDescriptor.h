//
//  SYSaneOptionDescriptor.h
//  SaneScanner
//
//  Created by rominet on 06/05/15.
//  Copyright (c) 2015 Syan. All rights reserved.
//

#import <Foundation/Foundation.h>
#import "sane.h"

typedef enum : NSUInteger {
    SYSaneOptionsSet_None = 0,
    SYSaneOptionsSet_ViaSoftware,
    SYSaneOptionsSet_ViaHardware,
} SYSaneOptionsSet;

@class SYSaneDevice;

NSString *NSStringFromSANE_Value_Type(SANE_Value_Type type);
NSString *NSStringFromSANE_Unit(SANE_Unit unit);

@interface SYSaneOptionDescriptor : NSObject

@property (nonatomic, assign) int index;
@property (nonatomic, strong) SYSaneDevice *device;
@property (nonatomic, strong) NSString *name;
@property (nonatomic, strong) NSString *title;
@property (nonatomic, strong) NSString *desc;
@property (nonatomic, assign) SANE_Value_Type type;
@property (nonatomic, assign) SANE_Unit unit;
@property (nonatomic, assign) NSUInteger size;
@property (nonatomic, assign) SYSaneOptionsSet capSet;
@property (nonatomic, assign) BOOL capSetAuto; // can be set automatically by backend
@property (nonatomic, assign) BOOL capReadable;
@property (nonatomic, assign) BOOL capEmulated;
@property (nonatomic, assign) BOOL capInactive;
@property (nonatomic, assign) BOOL capAdvanced;
@property (nonatomic, assign) SANE_Constraint_Type constraintType;
@property (nonatomic, strong) NSArray *constraintStringValues;
@property (nonatomic, strong) NSArray *constraintIntValues;
@property (nonatomic, strong) NSNumber *constraintMin;
@property (nonatomic, strong) NSNumber *constraintMax;
@property (nonatomic, strong) NSNumber *constraintStep;
@property (nonatomic, strong) NSArray *groupChildren;
@property (nonatomic, strong) id value;

- (instancetype)initWithCOpt:(const SANE_Option_Descriptor*)opt index:(int)index device:(SYSaneDevice *)device;

- (NSString *)descriptionConstraint;
- (NSString *)descriptionCapabilities;
- (NSString *)descriptionHuman;

- (void)updateValue:(void(^)(NSString *error))block;

+ (NSArray *)groupedElements:(NSArray *)elements;

@end
