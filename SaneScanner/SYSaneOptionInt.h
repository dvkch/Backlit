//
//  SYSaneOptionInt.h
//  SaneScanner
//
//  Created by rominet on 06/05/15.
//  Copyright (c) 2015 Syan. All rights reserved.
//

#import "SYSaneOption.h"

@interface SYSaneOptionInt : SYSaneOption

@property (nonatomic, strong) NSArray <NSNumber *> *constraintValues;
@property (nonatomic, assign) int minValue;
@property (nonatomic, assign) int maxValue;
@property (nonatomic, assign) int stepValue;
@property (nonatomic, assign) int value;

- (NSArray <NSNumber *> *)rangeValues;
- (NSArray <NSString *> *)constraintValuesWithUnit:(BOOL)withUnit;

@end
