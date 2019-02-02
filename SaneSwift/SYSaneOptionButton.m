//
//  SYSaneOptionButton.m
//  SaneScanner
//
//  Created by rominet on 06/05/15.
//  Copyright (c) 2015 Syan. All rights reserved.
//

#import "SYSaneOptionButton.h"
#import <SaneSwift/SaneSwift-Swift.h>

@interface SYSaneOptionButton ()
@end

@implementation SYSaneOptionButton

- (instancetype)initWithCOpt:(const SANE_Option_Descriptor *)opt index:(int)index device:(SYSaneDevice *)device
{
    if (opt->type != SANE_TYPE_BUTTON)
        return nil;
    
    self = [super initWithCOpt:opt index:index device:device];
    return self;
}

- (void)press:(void (^)(BOOL reloadAllOptions, NSError *))block
{
    [Sane.shared setValueForOptionWithValue:@YES auto:NO option:self completion:^(BOOL reloadAllOptions, NSError *error) {
        block(reloadAllOptions, error);
    }];
}

- (NSString *)valueStringWithUnit:(BOOL)withUnit
{
    return @"";
}

- (void)refreshValue:(void(^)(NSError *error))block
{
    if (block)
        block(nil);
}

@end
