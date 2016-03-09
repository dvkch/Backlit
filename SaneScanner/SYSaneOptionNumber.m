//
//  SYSaneOptionNumber.m
//  SaneScanner
//
//  Created by rominet on 06/05/15.
//  Copyright (c) 2015 Syan. All rights reserved.
//

#import "SYSaneOptionNumber.h"
#import "SYSaneHelper.h"
#import "SYSaneDevice.h"

@interface SYSaneOptionNumber ()
@end

@implementation SYSaneOptionNumber

- (instancetype)initWithCOpt:(const SANE_Option_Descriptor*)opt index:(int)index device:(SYSaneDevice *)device
{
    if (opt->type != SANE_TYPE_INT && opt->type != SANE_TYPE_FIXED)
        return nil;
    
    self = [super initWithCOpt:opt index:index device:device];
    if (self)
    {
        if (self.constraintType == SANE_CONSTRAINT_WORD_LIST)
        {
            NSMutableArray *values = [NSMutableArray array];
            for(uint i = 0; i < opt->constraint.word_list[0]; ++i)
            {
                NSNumber *value;
                if (self.type == SANE_TYPE_INT)
                    value = @(opt->constraint.word_list[i+1]);
                if (self.type == SANE_TYPE_FIXED)
                    value = @(SANE_UNFIX(opt->constraint.word_list[i+1]));
                
                [values addObject:value];
            }
            self.constraintValues = [values copy];
        }
        
        if (self.constraintType == SANE_CONSTRAINT_RANGE)
        {
            if (self.type == SANE_TYPE_INT)
            {
                self.minValue  = @(opt->constraint.range->min);
                self.maxValue  = @(opt->constraint.range->max);
                self.stepValue = @(opt->constraint.range->quant);
            }
            if (self.type == SANE_TYPE_FIXED)
            {
                self.minValue  = @(SANE_UNFIX(opt->constraint.range->min));
                self.maxValue  = @(SANE_UNFIX(opt->constraint.range->max));
                self.stepValue = @(SANE_UNFIX(opt->constraint.range->quant));
            }
            
            if (opt->constraint.range->quant == 0)
                self.stepValue = nil;
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
            if (self.type == SANE_TYPE_FIXED && !self.stepValue)
                return (self.minValue.doubleValue == self.maxValue.doubleValue);
            else if (self.type == SANE_TYPE_INT && !self.stepValue)
                return (self.minValue.intValue == self.maxValue.intValue);
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
        {
            if (self.type == SANE_TYPE_INT)
                self.value = value;
            
            if (self.type == SANE_TYPE_FIXED)
                self.value = @(SANE_UNFIX([value intValue]));
        }
        
        if(block)
            block(error);
    }];
}

- (NSString *)stringForValue:(id)value withUnit:(BOOL)withUnit
{
    NSString *unitString = @"";
    if (self.unit != SANE_UNIT_NONE)
        unitString = [NSString stringWithFormat:@" %@", NSStringFromSANE_Unit(self.unit)];
    
    if (self.type == SANE_TYPE_INT)
        return [NSString stringWithFormat:@"%d%@",        [value intValue], unitString];
    else
        return [NSString stringWithFormat:@"%.02lf%@", [value doubleValue], unitString];
}

- (NSString *)valueStringWithUnit:(BOOL)withUnit
{
    if (self.capInactive)
        return @"";
    
    return [self stringForValue:self.value withUnit:withUnit];
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
    if (!self.stepValue)
        return nil;
    
    if (self.type == SANE_TYPE_INT)
    {
        NSMutableArray *values = [NSMutableArray array];
        int value = self.minValue.intValue;
        while (value <= self.maxValue.intValue)
        {
            [values addObject:@(value)];
            value += self.stepValue.intValue;
        }
        
        return [values copy];
    }
    
    if (self.type == SANE_TYPE_FIXED)
    {
        NSMutableArray *values = [NSMutableArray array];
        double value = self.minValue.doubleValue;
        while (value <= self.maxValue.doubleValue)
        {
            [values addObject:@(value)];
            value += self.stepValue.doubleValue;
        }
        
        return [values copy];
    }
    
    return nil;
}

- (NSNumber *)bestValueForPreview
{
    SYSaneStandardOption std = SYSaneStandardOptionFromNSString(self.name);
    SYOptionValue value = SYBestValueForPreviewValueForOption(std);
    
    if (value == SYOptionValueAuto)
        return nil;
    
    if (self.constraintType == SANE_CONSTRAINT_RANGE)
        return (value == SYOptionValueMax ? self.maxValue : self.minValue);
    
    if (self.constraintType == SANE_CONSTRAINT_WORD_LIST)
    {
        NSArray <NSNumber *> *sortedValues = [self.constraintValues sortedArrayUsingSelector:@selector(compare:)];
        return (value == SYOptionValueMax ? sortedValues.lastObject : sortedValues.firstObject);
    }
    
    return self.value;
}

- (NSString *)descriptionConstraint
{
    if(self.constraintType == SANE_CONSTRAINT_RANGE) {
        if (self.stepValue)
            return [NSString stringWithFormat:@"Constrained to range from %@ to %@ with step of %@",
                    [self stringForValue:self.minValue withUnit:YES],
                    [self stringForValue:self.maxValue withUnit:YES],
                    [self stringForValue:self.stepValue withUnit:YES]];
        else
            return [NSString stringWithFormat:@"Constrained to range from %@ to %@",
                    [self stringForValue:self.minValue withUnit:YES],
                    [self stringForValue:self.maxValue withUnit:YES]];
    }
    else if (self.constraintType == SANE_CONSTRAINT_WORD_LIST) {
        return [NSString stringWithFormat:@"Constrained to list: %@",
                [self.constraintValues componentsJoinedByString:@", "]];
    }
    return @"not constrained";
}

@end
