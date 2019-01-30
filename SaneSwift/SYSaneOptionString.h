//
//  SYSaneOptionString.h
//  SaneScanner
//
//  Created by rominet on 06/05/15.
//  Copyright (c) 2015 Syan. All rights reserved.
//

#import "SYSaneOption.h"

@interface SYSaneOptionString : SYSaneOption

@property (nonatomic, strong) NSArray <NSString *> *constraintValues;
@property (nonatomic, strong) NSString *value;

- (NSArray <NSNumber *> *)constraintValuesWithUnit:(BOOL)withUnit;

@end
