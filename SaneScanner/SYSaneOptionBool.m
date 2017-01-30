//
//  SYSaneOptionBool.m
//  SaneScanner
//
//  Created by rominet on 06/05/15.
//  Copyright (c) 2015 Syan. All rights reserved.
//

#import "SYSaneOptionBool.h"
#import "SYSaneHelper.h"

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
    
    [[SYSaneHelper shared] getValueForOption:self block:^(id value, NSError *error) {
        if (!error)
            self.value = [value boolValue];
        
        if(block)
            block(error);
    }];
}

- (NSString *)stringForValue:(id)value withUnit:(BOOL)withUnit
{
    return [value boolValue] ? $("OPTION BOOL ON") : $("OPTION BOOL OFF");
}

- (NSString *)valueStringWithUnit:(BOOL)withUnit
{
    if (self.capInactive)
        return $$("");
    
    return [self stringForValue:@(self.value) withUnit:withUnit];
}

@end
