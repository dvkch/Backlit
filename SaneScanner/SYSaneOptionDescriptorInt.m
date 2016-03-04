//
//  SYSaneOptionDescriptorInt.m
//  SaneScanner
//
//  Created by rominet on 06/05/15.
//  Copyright (c) 2015 Syan. All rights reserved.
//

#import "SYSaneOptionDescriptorInt.h"
#import "SYSaneHelper.h"

@interface SYSaneOptionDescriptorInt ()
@end

@implementation SYSaneOptionDescriptorInt

- (instancetype)initWithCOpt:(const SANE_Option_Descriptor*)opt index:(int)index device:(SYSaneDevice *)device
{
    self = [super initWithCOpt:opt index:index device:device];
    if (self)
    {
        if (self.constraintType == SANE_CONSTRAINT_WORD_LIST)
        {
            NSMutableArray *values = [NSMutableArray array];
            for(uint i = 0; i < opt->constraint.word_list[0]; ++i)
                [values addObject:@(opt->constraint.word_list[i+1])];
            self.constraintValues = [values copy];
        }
        
        if (self.constraintType == SANE_CONSTRAINT_RANGE)
        {
            self.minValue  = opt->constraint.range->min;
            self.maxValue  = opt->constraint.range->max;
            self.stepValue = opt->constraint.range->quant;
        }
    }
    return self;
}

- (BOOL)readOnlyOrSingleOption
{
    if (!self.capSettableViaSoftware)
        return YES;
    
    switch (self.constraintType) {
        case SANE_CONSTRAINT_NONE:
            return NO;
        case SANE_CONSTRAINT_RANGE:
            if (self.stepValue == 0)
                return (self.minValue == self.maxValue);
            else
                return (self.rangeValues.count <= 1);
        case SANE_CONSTRAINT_WORD_LIST:
            return (self.constraintValues.count <= 1);
        case SANE_CONSTRAINT_STRING_LIST:
            return YES; // should never happen, let's at least prevent setting the value
    }
}

- (void)updateValue:(void (^)(NSString *))block
{
    [[SYSaneHelper shared] getValueForOption:self onDevice:self.device block:^(id value, NSString *error) {
        if(error) {
            if(block)
                block(error);
        }
        else {
            self.value = [value intValue];
        }
    }];
}

- (NSArray<NSNumber *> *)rangeValues
{
    if (self.stepValue <= 0)
        return nil;
    
    NSMutableArray *values = [NSMutableArray array];
    int value = self.minValue;
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
            return [NSString stringWithFormat:@"Constrained to range from %d to %d with step of %d",
                    self.minValue, self.maxValue, self.stepValue];
        else
            return [NSString stringWithFormat:@"Constrained to range from %d to %d",
                    self.minValue, self.maxValue];
    }
    else if (self.constraintType == SANE_CONSTRAINT_WORD_LIST) {
        return [NSString stringWithFormat:@"Constrained to list: %@",
                [self.constraintValues componentsJoinedByString:@", "]];
    }
    return @"not constrained";
}

@end
