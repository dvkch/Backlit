//
//  SYSaneOptionDescriptor.m
//  SaneScanner
//
//  Created by rominet on 06/05/15.
//  Copyright (c) 2015 Syan. All rights reserved.
//

#import "SYSaneOptionDescriptor.h"
#import "SYSaneHelper.h"

@interface SYSaneOptionDescriptor ()
@end

@implementation SYSaneOptionDescriptor

- (instancetype)initWithCOpt:(const SANE_Option_Descriptor*)opt index:(int)index device:(SYSaneDevice *)device
{
    if(!opt)
        return nil;
    
    self = [super init];
    if (self)
    {
        self.index  = index;
        self.device = device;
        self.name   = [NSString stringWithCString:(opt->name  ?: "") encoding:NSUTF8StringEncoding];
        self.title  = [NSString stringWithCString:(opt->title ?: "") encoding:NSUTF8StringEncoding];
        self.desc   = [NSString stringWithCString:(opt->desc  ?: "") encoding:NSUTF8StringEncoding];
        self.type   = opt->type;
        self.unit   = opt->unit;
        self.size   = opt->size;
        if(opt->cap & SANE_CAP_SOFT_SELECT) self.capSet = SYSaneOptionsSet_ViaSoftware;
        if(opt->cap & SANE_CAP_HARD_SELECT) self.capSet = SYSaneOptionsSet_ViaHardware;
        self.capReadable        = opt->cap & SANE_CAP_SOFT_DETECT;
        self.capSetAuto         = opt->cap & SANE_CAP_AUTOMATIC;
        self.capEmulated        = opt->cap & SANE_CAP_EMULATED;
        self.capInactive        = opt->cap & SANE_CAP_INACTIVE;
        self.capAdvanced        = opt->cap & SANE_CAP_ADVANCED;
        self.constraintType     = opt->constraint_type;
        
        if (self.constraintType == SANE_CONSTRAINT_STRING_LIST)
        {
            NSMutableArray *values = [NSMutableArray array];
            uint i = 0;
            while (opt->constraint.string_list[i]) {
                [values addObject:[NSString stringWithCString:opt->constraint.string_list[i] encoding:NSUTF8StringEncoding]];
                ++i;
            }
            self.constraintStringValues = [values copy];
        }
        
        if (self.constraintType == SANE_CONSTRAINT_WORD_LIST)
        {
            NSMutableArray *values = [NSMutableArray array];
            for(uint i = 0; i < opt->constraint.word_list[0]; ++i)
                [values addObject:@(opt->constraint.word_list[i+1])];
            self.constraintIntValues = [values copy];
        }
        
        if (self.constraintType == SANE_CONSTRAINT_RANGE)
        {
            self.constraintMin = @(opt->constraint.range->min);
            self.constraintMax = @(opt->constraint.range->max);
            int step = opt->constraint.range->quant;
            self.constraintStep = step == 0 ? nil : @(step);
        }
    }
    return self;
}

- (NSString *)descriptionConstraint
{
    if(self.constraintType == SANE_CONSTRAINT_RANGE) {
        return [NSString stringWithFormat:@"constrained to range from %@ to %@, %@step%@%@",
                self.constraintMin,
                self.constraintMax,
                self.constraintStep ? @"" : @"no ",
                self.constraintStep ? @" of " : @"",
                self.constraintStep ?: @""];
    }
    else if (self.constraintType == SANE_CONSTRAINT_STRING_LIST) {
        return [NSString stringWithFormat:@"possible values [%@]",
                [self.constraintStringValues componentsJoinedByString:@", "]];
    }
    else if (self.constraintType == SANE_CONSTRAINT_WORD_LIST) {
        return [NSString stringWithFormat:@"possible values [%@]",
                [self.constraintIntValues componentsJoinedByString:@", "]];
    }
    return @"not constrained";
}

- (NSString *)descriptionCapabilities
{
    return [NSString stringWithFormat:@"%@readable, %@settable%@%@%@, %@active%@%@%@%@",
            self.capReadable ? @"" : @"not ",
            self.capSet == SYSaneOptionsSet_None ? @"not " : @"",
            self.capSet == SYSaneOptionsSet_None ? @"" : @" via ",
            self.capSet == SYSaneOptionsSet_ViaHardware ? @"hardware" : (self.capSet == SYSaneOptionsSet_ViaSoftware ? @"software" : @""),
            self.capSetAuto ? @" or auto" : @"",
            self.capInactive ? @"in" : @"",
            self.capEmulated ? @", " : @"",
            self.capEmulated ? @"emulated" : @"",
            self.capAdvanced ? @", " : @"",
            self.capAdvanced ? @"advanced" : @""];
}

- (NSString *)descriptionHuman
{
    return [NSString stringWithFormat:@"<#%d, %@, %@, %@, %@, %@>",
            (int)self.index,
            self.title,
            NSStringFromSANE_Value_Type(self.type),
            NSStringFromSANE_Unit(self.unit),
            self.descriptionCapabilities,
            self.descriptionConstraint];
}

- (NSString *)description
{
    return [NSString stringWithFormat:@"<%@: %p, %d, %@, %@, %@%@%@>",
            [self class],
            self,
            (int)self.index,
            self.title,
            NSStringFromSANE_Value_Type(self.type),
            NSStringFromSANE_Unit(self.unit),
            self.groupChildren ? @", " : @"",
            self.groupChildren ?: @""];
}

- (void)updateValue:(void(^)(NSString *error))block
{
    [[SYSaneHelper shared] getValueForOption:self onDevice:self.device block:^(id value, NSString *error) {
        if(error) {
            if(block)
                block(error);
        }
        else {
            self.value = value;
        }
    }];
}

+ (NSArray *)groupedElements:(NSArray *)elements
{
    NSMutableArray *groups = [NSMutableArray array];
    NSMutableArray *groupElements = nil;
    SYSaneOptionDescriptor *group = nil;
    
    for(SYSaneOptionDescriptor *item in elements)
    {
        if(item.type == SANE_TYPE_GROUP)
        {
            if(group)
            {
                group.groupChildren = [groupElements copy];
                [groups addObject:group];
            }
            
            group = item;
            groupElements = [NSMutableArray array];
            continue;
        }
        [groupElements addObject:item];
    }

    group.groupChildren = [groupElements copy];
    [groups addObject:group];
    
    return [groups copy];
}

@end

NSString *NSStringFromSANE_Value_Type(SANE_Value_Type type)
{
    NSString *s = @"unknown";
    switch (type) {
        case SANE_TYPE_BOOL:    s = @"bool";    break;
        case SANE_TYPE_BUTTON:  s = @"button";  break;
        case SANE_TYPE_FIXED:   s = @"fixed";   break;
        case SANE_TYPE_GROUP:   s = @"group";   break;
        case SANE_TYPE_INT:     s = @"int";     break;
        case SANE_TYPE_STRING:  s = @"string";  break;
    }
    return s;
}

NSString *NSStringFromSANE_Unit(SANE_Unit unit)
{
    NSString *s = @"unknown";
    switch (unit) {
        case SANE_UNIT_NONE:        s = @"none"; break;
        case SANE_UNIT_PIXEL:       s = @"px";   break;
        case SANE_UNIT_BIT:         s = @"bits"; break;
        case SANE_UNIT_MM:          s = @"mm";   break;
        case SANE_UNIT_DPI:         s = @"dpi";  break;
        case SANE_UNIT_PERCENT:     s = @"%";    break;
        case SANE_UNIT_MICROSECOND: s = @"ms";   break;
    }
    return s;
}
