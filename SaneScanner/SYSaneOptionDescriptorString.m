//
//  SYSaneOptionDescriptorString.m
//  SaneScanner
//
//  Created by rominet on 06/05/15.
//  Copyright (c) 2015 Syan. All rights reserved.
//

#import "SYSaneOptionDescriptorString.h"
#import "SYSaneHelper.h"

@interface SYSaneOptionDescriptorString ()
@end

@implementation SYSaneOptionDescriptorString

- (instancetype)initWithCOpt:(const SANE_Option_Descriptor*)opt index:(int)index device:(SYSaneDevice *)device
{
    self = [super initWithCOpt:opt index:index device:device];
    if (self)
    {
        if (self.constraintType == SANE_CONSTRAINT_STRING_LIST)
        {
            NSMutableArray <NSString *> *values = [NSMutableArray array];
            uint i = 0;
            while (opt->constraint.string_list[i]) {
                NSString *value = [NSString stringWithCString:opt->constraint.string_list[i]
                                                     encoding:NSUTF8StringEncoding];
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
    if (!self.capSettableViaSoftware)
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

- (void)updateValue:(void (^)(NSString *))block
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

- (NSString *)descriptionConstraint
{
    if (self.constraintType == SANE_CONSTRAINT_STRING_LIST) {
        return [NSString stringWithFormat:@"Constrained to list: %@",
                [self.constraintValues componentsJoinedByString:@", "]];
    }
    return @"not constrained";
}

@end
