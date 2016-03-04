//
//  SYSaneOptionDescriptorBool.m
//  SaneScanner
//
//  Created by rominet on 06/05/15.
//  Copyright (c) 2015 Syan. All rights reserved.
//

#import "SYSaneOptionDescriptorBool.h"
#import "SYSaneHelper.h"

@interface SYSaneOptionDescriptorBool ()
@end

@implementation SYSaneOptionDescriptorBool

- (instancetype)initWithCOpt:(const SANE_Option_Descriptor*)opt index:(int)index device:(SYSaneDevice *)device
{
    self = [super initWithCOpt:opt index:index device:device];
    return self;
}

- (void)updateValue:(void(^)(NSString *error))block
{
    [[SYSaneHelper shared] getValueForOption:self onDevice:self.device block:^(id value, NSString *error) {
        if(error) {
            if(block)
                block(error);
        }
        else {
            self.value = [value boolValue];
        }
    }];
}

@end
