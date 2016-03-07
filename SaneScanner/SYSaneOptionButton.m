//
//  SYSaneOptionButton.m
//  SaneScanner
//
//  Created by rominet on 06/05/15.
//  Copyright (c) 2015 Syan. All rights reserved.
//

#import "SYSaneOptionButton.h"
#import "SYSaneHelper.h"

@interface SYSaneOptionButton ()
@end

@implementation SYSaneOptionButton

- (void)press:(void (^)(NSString *))block
{
    [[SYSaneHelper shared] setValue:nil
                        orAutoValue:YES
                          forOption:self
                    thenReloadValue:YES
                              block:^(BOOL reloadAllOptions, NSString *error)
    {
        block(error);
    }];
}

- (NSString *)valueStringWithUnit:(BOOL)withUnit
{
    return @"";
}

- (void)refreshValue:(void(^)(NSString *error))block
{
    if (block)
        block(nil);
}

@end