//
//  SYSaneOption.m
//  SaneScanner
//
//  Created by rominet on 06/05/15.
//  Copyright (c) 2015 Syan. All rights reserved.
//

#import "SYSaneOption.h"
#import "SYSaneOptionBool.h"
#import "SYSaneOptionNumber.h"
#import "SYSaneOptionString.h"
#import "SYSaneOptionButton.h"
#import "SYSaneOptionGroup.h"
#import "SYSaneHelper.h"

@interface SYSaneOption ()
@property (nonatomic, assign) SANE_Int cap;
@end

@implementation SYSaneOption

+ (instancetype)initWithCOpt:(const SANE_Option_Descriptor*)opt index:(int)index device:(SYSaneDevice *)device
{
    switch (opt->type) {
        case SANE_TYPE_BOOL:    return [[SYSaneOptionBool   alloc] initWithCOpt:opt index:index device:device];
        case SANE_TYPE_INT:     return [[SYSaneOptionNumber alloc] initWithCOpt:opt index:index device:device];
        case SANE_TYPE_FIXED:   return [[SYSaneOptionNumber alloc] initWithCOpt:opt index:index device:device];
        case SANE_TYPE_STRING:  return [[SYSaneOptionString alloc] initWithCOpt:opt index:index device:device];
        case SANE_TYPE_BUTTON:  return [[SYSaneOptionButton alloc] initWithCOpt:opt index:index device:device];
        case SANE_TYPE_GROUP:   return [[SYSaneOptionGroup  alloc] initWithCOpt:opt index:index device:device];
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

- (BOOL)disabledOrReadOnly
{
    return (!self.capSettableViaSoftware || self.capInactive);
}

- (BOOL)readOnlyOrSingleOption
{
    return (!self.capSettableViaSoftware || self.capInactive);
}

- (NSString *)valueStringWithUnit:(BOOL)withUnit
{
    [NSException raise:$$("Not implemented") format:$("")];
    return nil;
}

- (NSString *)stringForValue:(id)value withUnit:(BOOL)withUnit;
{
    [NSException raise:$$("Not implemented") format:$("")];
    return nil;
}

- (void)refreshValue:(void(^)(NSError *error))block
{
    [NSException raise:$$("Not implemented") format:$("")];
}

- (NSString *)descriptionConstraint
{
    if(self.constraintType == SANE_CONSTRAINT_RANGE) {
        return $("OPTION CONSTRAINED RANGE");
    }
    else if (self.constraintType == SANE_CONSTRAINT_STRING_LIST) {
        return $("OPTION CONSTRAINED LIST");
    }
    else if (self.constraintType == SANE_CONSTRAINT_WORD_LIST) {
        return $("OPTION CONSTRAINED LIST");
    }
    return $("OPTION CONSTRAINED NOT CONSTRAINED");
}

- (NSString *)debugDescriptionCapabilities
{
    NSMutableArray <NSString *> *descriptions = [NSMutableArray array];
    
    if (self.capReadable)
        [descriptions addObject:$$("readable")];
    else
        [descriptions addObject:$$("not readable")];
    
    if (self.cap & SANE_CAP_SOFT_SELECT)
        [descriptions addObject:$$("settable via software")];
    
    if (self.cap & SANE_CAP_HARD_SELECT)
        [descriptions addObject:$$("settable via hardware")];
    
    if (!(self.cap & SANE_CAP_SOFT_SELECT) && !(self.cap & SANE_CAP_HARD_SELECT))
        [descriptions addObject:$$("not settable")];
    
    if (self.capSetAuto)
        [descriptions addObject:$$("has auto value")];
    
    if (self.capInactive)
        [descriptions addObject:$$("inactive")];
    
    if (self.capAdvanced)
        [descriptions addObject:$$("advanced")];
    
    if (self.capEmulated)
        [descriptions addObject:$$("emulated")];
    
    return [descriptions componentsJoinedByString:$$(", ")];
}

- (NSString *)debugDescriptionHuman
{
    return [NSString stringWithFormat:$$("<#%d, %@, %@, %@, %@, %@>"),
            (int)self.index,
            self.title,
            NSStringFromSANE_Value_Type(self.type),
            NSStringFromSANE_Unit(self.unit),
            self.debugDescriptionCapabilities,
            self.descriptionConstraint];
}

- (NSString *)description
{
    return [NSString stringWithFormat:$$("<%@: %p, %d, %@, %@, %@>"),
            [self class],
            self,
            (int)self.index,
            self.title,
            NSStringFromSANE_Value_Type(self.type),
            NSStringFromSANE_Unit(self.unit)];
}

+ (NSArray<SYSaneOptionGroup *> *)groupedElements:(NSArray<SYSaneOption *> *)elements
                                removeEmptyGroups:(BOOL)removeEmptyGroups
{
    NSMutableArray <SYSaneOptionGroup *> *groups = [NSMutableArray array];
    NSMutableArray <SYSaneOption *> *groupElements = nil;
    SYSaneOptionGroup *group = nil;
    
    for(SYSaneOption *item in elements)
    {
        if(item.type == SANE_TYPE_GROUP)
        {
            if(group)
            {
                group.items = [groupElements copy];
                [groups addObject:group];
            }
            
            group = (SYSaneOptionGroup *)item;
            groupElements = [NSMutableArray array];
            continue;
        }
        [groupElements addObject:item];
    }

    group.items = [groupElements copy];
    if (group)
        [groups addObject:group];
    
    if (removeEmptyGroups)
    {
        NSMutableArray <SYSaneOptionGroup *> *emptyGroups = [NSMutableArray array];
        for (SYSaneOptionGroup *group in groups)
            if (group.items.count == 0)
                [emptyGroups addObject:group];
        
        [groups removeObjectsInArray:emptyGroups];
    }
    
    return [groups copy];
}

@end

NSString *NSStringFromSANE_Value_Type(SANE_Value_Type type)
{
    NSString *s = $$("unknown");
    switch (type) {
        case SANE_TYPE_BOOL:    s = $$("bool");    break;
        case SANE_TYPE_BUTTON:  s = $$("button");  break;
        case SANE_TYPE_FIXED:   s = $$("fixed");   break;
        case SANE_TYPE_GROUP:   s = $$("group");   break;
        case SANE_TYPE_INT:     s = $$("int");     break;
        case SANE_TYPE_STRING:  s = $$("string");  break;
    }
    return s;
}

NSString *NSStringFromSANE_Unit(SANE_Unit unit)
{
    NSString *s = $$("unknown");
    switch (unit) {
        case SANE_UNIT_NONE:        s = $$("none"); break;
        case SANE_UNIT_PIXEL:       s = $$("px");   break;
        case SANE_UNIT_BIT:         s = $$("bits"); break;
        case SANE_UNIT_MM:          s = $$("mm");   break;
        case SANE_UNIT_DPI:         s = $$("dpi");  break;
        case SANE_UNIT_PERCENT:     s = $$("%");    break;
        case SANE_UNIT_MICROSECOND: s = $$("ms");   break;
    }
    return s;
}
