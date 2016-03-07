//
//  SYSaneOptionDouble.m
//  SaneScanner
//
//  Created by rominet on 06/05/15.
//  Copyright (c) 2015 Syan. All rights reserved.
//

#import "SYSaneOptionDouble.h"
#import "SYSaneHelper.h"

@interface SYSaneOptionDouble ()
@end

@implementation SYSaneOptionDouble

- (instancetype)initWithCOpt:(const SANE_Option_Descriptor*)opt index:(int)index device:(SYSaneDevice *)device
{
    self = [super initWithCOpt:opt index:index device:device];
    if (self)
    {
        if (self.constraintType == SANE_CONSTRAINT_WORD_LIST)
        {
            NSMutableArray *values = [NSMutableArray array];
            for(uint i = 0; i < opt->constraint.word_list[0]; ++i)
                [values addObject:@(SANE_UNFIX(opt->constraint.word_list[i+1]))];
            self.constraintValues = [values copy];
        }
        
        if (self.constraintType == SANE_CONSTRAINT_RANGE)
        {
            self.minValue  = SANE_UNFIX(opt->constraint.range->min);
            self.maxValue  = SANE_UNFIX(opt->constraint.range->max);
            self.stepValue = SANE_UNFIX(opt->constraint.range->quant);
        }
    }
    return self;
}

- (BOOL)readOnlyOrSingleOption
{
    if ([super readOnlyOrSingleOption])
        return YES;
    
    switch (self.constraintType) {
        case SANE_CONSTRAINT_NONE:
            return NO;
        case SANE_CONSTRAINT_RANGE:
            if (self.stepValue == 0.)
                return (self.minValue == self.maxValue);
            else
                return (self.rangeValues.count <= 1);
        case SANE_CONSTRAINT_WORD_LIST:
            return (self.constraintValues.count <= 1);
        case SANE_CONSTRAINT_STRING_LIST:
            return YES; // should never happen, let's at least prevent setting the value
    }
}

- (void)refreshValue:(void (^)(NSString *))block
{
    if (self.capInactive)
    {
        if (block)
            block(nil);
        return;
    }
    
    [[SYSaneHelper shared] getValueForOption:self block:^(id value, NSString *error) {
        if (!error)
            self.value = SANE_UNFIX([value intValue]);
        
        if(block)
            block(error);
    }];
}

- (NSString *)stringForValue:(id)value withUnit:(BOOL)withUnit
{
    NSString *unitString = @"";
    if (self.unit != SANE_UNIT_NONE)
        unitString = [NSString stringWithFormat:@" %@", NSStringFromSANE_Unit(self.unit)];
    
    return [NSString stringWithFormat:@"%.02lf%@", [value doubleValue], unitString];
}

- (NSString *)valueStringWithUnit:(BOOL)withUnit
{
    if (self.capInactive)
        return @"";
    
    return [self stringForValue:@(self.value) withUnit:withUnit];
}

- (NSArray <NSString *> *)constraintValuesWithUnit:(BOOL)withUnit
{
    NSMutableArray *values = [NSMutableArray array];
    for (NSNumber *value in self.constraintValues)
        [values addObject:[self stringForValue:value withUnit:withUnit]];
    
    return [values copy];
}

- (NSArray<NSNumber *> *)rangeValues
{
    if (self.stepValue <= 0.)
        return nil;
    
    NSMutableArray *values = [NSMutableArray array];
    float value = self.minValue;
    while (value <= self.maxValue)
    {
        [values addObject:@(value)];
        value += self.stepValue;
    }
    
    return [values copy];
}

- (NSString *)descriptionConstraint
{
    if(self.constraintType == SANE_CONSTRAINT_RANGE) {
        if (self.stepValue)
            return [NSString stringWithFormat:@"Constrained to range from %lf to %lf with step of %lf",
                    self.minValue, self.maxValue, self.stepValue];
        else
            return [NSString stringWithFormat:@"Constrained to range from %lf to %lf",
                    self.minValue, self.maxValue];
    }
    else if (self.constraintType == SANE_CONSTRAINT_WORD_LIST) {
        return [NSString stringWithFormat:@"Constrained to list: %@",
                [self.constraintValues componentsJoinedByString:@", "]];
    }
    return @"not constrained";
}

@end
