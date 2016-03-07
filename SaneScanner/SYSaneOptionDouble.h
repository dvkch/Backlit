//
//  SYSaneOptionDouble.h
//  SaneScanner
//
//  Created by rominet on 06/05/15.
//  Copyright (c) 2015 Syan. All rights reserved.
//

#import "SYSaneOption.h"

@interface SYSaneOptionDouble : SYSaneOption

@property (nonatomic, strong) NSArray <NSNumber *> *constraintValues;
@property (nonatomic, assign) double minValue;
@property (nonatomic, assign) double maxValue;
@property (nonatomic, assign) double stepValue;
@property (nonatomic, assign) double value;

- (NSArray <NSNumber *> *)rangeValues;
- (NSArray <NSString *> *)constraintValuesWithUnit:(BOOL)withUnit;

@end
