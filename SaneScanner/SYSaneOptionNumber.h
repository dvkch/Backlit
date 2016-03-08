//
//  SYSaneOptionNumber.h
//  SaneScanner
//
//  Created by rominet on 06/05/15.
//  Copyright (c) 2015 Syan. All rights reserved.
//

#import "SYSaneOption.h"

@interface SYSaneOptionNumber : SYSaneOption

@property (nonatomic, strong) NSArray <NSNumber *> *constraintValues;
@property (nonatomic, strong) NSNumber *minValue;
@property (nonatomic, strong) NSNumber *maxValue;
@property (nonatomic, strong) NSNumber *stepValue;
@property (nonatomic, strong) NSNumber *value;

- (NSArray <NSNumber *> *)rangeValues;
- (NSArray <NSString *> *)constraintValuesWithUnit:(BOOL)withUnit;

- (NSNumber *)bestValueForPreview;

@end
