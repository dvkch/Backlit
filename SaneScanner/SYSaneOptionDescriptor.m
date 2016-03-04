//
//  SYSaneOptionDescriptor.m
//  SaneScanner
//
//  Created by rominet on 06/05/15.
//  Copyright (c) 2015 Syan. All rights reserved.
//

#import "SYSaneOptionDescriptor.h"
#import "SYSaneOptionDescriptorBool.h"
#import "SYSaneOptionDescriptorInt.h"
#import "SYSaneOptionDescriptorFloat.h"
#import "SYSaneOptionDescriptorString.h"
#import "SYSaneOptionDescriptorButton.h"
#import "SYSaneOptionDescriptorGroup.h"
#import "SYSaneHelper.h"

@interface SYSaneOptionDescriptor ()
@property (nonatomic, assign) SANE_Int cap;
@end

@implementation SYSaneOptionDescriptor

+ (instancetype)initWithCOpt:(const SANE_Option_Descriptor*)opt index:(int)index device:(SYSaneDevice *)device
{
    switch (opt->type) {
        case SANE_TYPE_BOOL:    return [[SYSaneOptionDescriptorBool   alloc] initWithCOpt:opt index:index device:device];
        case SANE_TYPE_INT:     return [[SYSaneOptionDescriptorInt    alloc] initWithCOpt:opt index:index device:device];
        case SANE_TYPE_FIXED:   return [[SYSaneOptionDescriptorFloat  alloc] initWithCOpt:opt index:index device:device];
        case SANE_TYPE_STRING:  return [[SYSaneOptionDescriptorString alloc] initWithCOpt:opt index:index device:device];
        case SANE_TYPE_BUTTON:  return [[SYSaneOptionDescriptorButton alloc] initWithCOpt:opt index:index device:device];
        case SANE_TYPE_GROUP:   return [[SYSaneOptionDescriptorGroup  alloc] initWithCOpt:opt index:index device:device];
    }
}

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
        self.cap                    = opt->cap;
        self.capReadable            = opt->cap & SANE_CAP_SOFT_DETECT;
        self.capSetAuto             = opt->cap & SANE_CAP_AUTOMATIC;
        self.capEmulated            = opt->cap & SANE_CAP_EMULATED;
        self.capInactive            = opt->cap & SANE_CAP_INACTIVE;
        self.capAdvanced            = opt->cap & SANE_CAP_ADVANCED;
        self.capSettableViaSoftware = opt->cap & SANE_CAP_SOFT_SELECT;
        self.capSettableViaHardware = opt->cap & SANE_CAP_HARD_SELECT;
        self.constraintType         = opt->constraint_type;
    }
    return self;
}

- (BOOL)readOnlyOrSingleOption
{
    return (!self.capSettableViaSoftware);
}

- (void)updateValue:(void(^)(NSString *error))block
{
    [NSException raise:@"Not implemented" format:@""];
}

- (NSString *)descriptionConstraint
{
    if(self.constraintType == SANE_CONSTRAINT_RANGE) {
        return @"Constrained to range";
    }
    else if (self.constraintType == SANE_CONSTRAINT_STRING_LIST) {
        return @"Constrained to string list";
    }
    else if (self.constraintType == SANE_CONSTRAINT_WORD_LIST) {
        return @"Constrained to value list";
    }
    return @"not constrained";
}

- (NSString *)descriptionCapabilities
{
    NSMutableArray <NSString *> *descriptions = [NSMutableArray array];
    
    if (self.capReadable)
        [descriptions addObject:@"readable"];
    else
        [descriptions addObject:@"not readable"];
    
    if (self.cap & SANE_CAP_SOFT_SELECT)
        [descriptions addObject:@"settable via software"];
    
    if (self.cap & SANE_CAP_HARD_SELECT)
        [descriptions addObject:@"settable via hardware"];
    
    if (!(self.cap & SANE_CAP_SOFT_SELECT) && !(self.cap & SANE_CAP_HARD_SELECT))
        [descriptions addObject:@"not settable"];
    
    if (self.capSetAuto)
        [descriptions addObject:@"has auto value"];
    
    if (self.capInactive)
        [descriptions addObject:@"inactive"];
    
    if (self.capAdvanced)
        [descriptions addObject:@"advanced"];
    
    if (self.capEmulated)
        [descriptions addObject:@"emulated"];
    
    return [descriptions componentsJoinedByString:@", "];
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
    return [NSString stringWithFormat:@"<%@: %p, %d, %@, %@, %@>",
            [self class],
            self,
            (int)self.index,
            self.title,
            NSStringFromSANE_Value_Type(self.type),
            NSStringFromSANE_Unit(self.unit)];
}

+ (NSArray *)groupedElements:(NSArray *)elements
{
    NSMutableArray *groups = [NSMutableArray array];
    NSMutableArray *groupElements = nil;
    SYSaneOptionDescriptorGroup *group = nil;
    
    for(SYSaneOptionDescriptor *item in elements)
    {
        if(item.type == SANE_TYPE_GROUP)
        {
            if(group)
            {
                group.items = [groupElements copy];
                [groups addObject:group];
            }
            
            group = (SYSaneOptionDescriptorGroup *)item;
            groupElements = [NSMutableArray array];
            continue;
        }
        [groupElements addObject:item];
    }

    group.items = [groupElements copy];
    if (group)
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
