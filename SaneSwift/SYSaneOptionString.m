//
//  SYSaneOptionString.m
//  SaneScanner
//
//  Created by rominet on 06/05/15.
//  Copyright (c) 2015 Syan. All rights reserved.
//

#import "SYSaneOptionString.h"
#import <SaneSwift/SaneSwift-Swift.h>

@interface SYSaneOptionString ()
@end

@implementation SYSaneOptionString

- (instancetype)initWithCOpt:(const SANE_Option_Descriptor*)opt index:(int)index device:(Device *)device
{
    if (opt->type != SANE_TYPE_STRING)
        return nil;
    
    self = [super initWithCOpt:opt index:index device:device];
    if (self)
    {
        if (self.constraintType == SANE_CONSTRAINT_STRING_LIST)
        {
            NSMutableArray <NSString *> *values = [NSMutableArray array];
            uint i = 0;
            while (opt->constraint.string_list[i])
            {
                NSString *value = [NSString stringWithCString:opt->constraint.string_list[i]
                                                     encoding:NSUTF8StringEncoding];

                value = [Sane.shared translationFor:value];
                [values addObject:value];
                
                ++i;
            }
            self.constraintValues = [values copy];
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
        case SANE_CONSTRAINT_STRING_LIST:
            return (self.constraintValues.count <= 1);
        case SANE_CONSTRAINT_RANGE:
        case SANE_CONSTRAINT_WORD_LIST:
            return YES; // should never happen, let's at least prevent setting the value
    }
}

- (void)refreshValue:(void (^)(NSError *))block
{
    if (self.capInactive)
    {
        if (block)
            block(nil);
        return;
    }
    
    [Sane.shared valueForOption:self completion:^(id _Nullable value, NSError * _Nullable error) {
        if (!error)
            self.value = value;
        
        if (block)
            block(error);
    }];
}

- (NSString *)stringForValue:(id)value withUnit:(BOOL)withUnit
{
    NSString *stringValue = [value description];
    stringValue = [Sane.shared translationFor:stringValue];
    
    NSMutableArray <NSString *> *parts = [NSMutableArray array];
    
    if (stringValue)                 [parts addObject:stringValue];
    if (self.unit != SANE_UNIT_NONE) [parts addObject:NSStringFromSANE_Unit(self.unit)];
    
    return [parts componentsJoinedByString:@" "];
}

- (NSString *)valueStringWithUnit:(BOOL)withUnit
{
    if (self.capInactive)
        return @"";
    
    return [self stringForValue:self.value withUnit:withUnit];
}

- (NSArray <NSNumber *> *)constraintValuesWithUnit:(BOOL)withUnit
{
    NSMutableArray *values = [NSMutableArray array];
    for (NSString *value in self.constraintValues)
        [values addObject:[self stringForValue:value withUnit:withUnit]];
    
    return [values copy];
}

- (NSString *)descriptionConstraint
{
    // TODO: translate
    if (self.constraintType == SANE_CONSTRAINT_STRING_LIST) {
        return [NSString stringWithFormat:@"OPTION CONSTRAINED LIST %@",
                [self.constraintValues componentsJoinedByString:@", "]];
    }
    return @"OPTION CONSTRAINED NOT CONSTRAINED";
}

@end
