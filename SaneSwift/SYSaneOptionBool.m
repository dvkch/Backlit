//
//  SYSaneOptionBool.m
//  SaneScanner
//
//  Created by rominet on 06/05/15.
//  Copyright (c) 2015 Syan. All rights reserved.
//

#import "SYSaneOptionBool.h"
#import <SaneSwift/SaneSwift-Swift.h>

@interface SYSaneOptionBool ()
@end

@implementation SYSaneOptionBool

- (instancetype)initWithCOpt:(const SANE_Option_Descriptor*)opt index:(int)index device:(SYSaneDevice *)device
{
    if (opt->type != SANE_TYPE_BOOL)
        return nil;
    
    self = [super initWithCOpt:opt index:index device:device];
    return self;
}

- (void)refreshValue:(void(^)(NSError *error))block
{
    if (self.capInactive)
    {
        if (block)
            block(nil);
        return;
    }
    
    [Sane.shared valueForOption:self completion:^(id _Nullable value, NSError * _Nullable error) {
        if (!error)
            self.value = [value boolValue];
        
        if (block)
            block(error);
    }];
}

- (NSString *)stringForValue:(id)value withUnit:(BOOL)withUnit
{
    return [Sane.shared translationFor:[value boolValue] ? @"On" : @"Off"];
}

- (NSString *)valueStringWithUnit:(BOOL)withUnit
{
    if (self.capInactive)
        return @"";
    
    return [self stringForValue:@(self.value) withUnit:withUnit];
}

@end
