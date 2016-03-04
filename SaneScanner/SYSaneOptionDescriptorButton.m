//
//  SYSaneOptionDescriptorButton.m
//  SaneScanner
//
//  Created by rominet on 06/05/15.
//  Copyright (c) 2015 Syan. All rights reserved.
//

#import "SYSaneOptionDescriptorButton.h"
#import "SYSaneHelper.h"

@interface SYSaneOptionDescriptorButton ()
@end

@implementation SYSaneOptionDescriptorButton

- (void)press:(void (^)(NSString *))block
{
    [[SYSaneHelper shared] setValue:nil
                        orAutoValue:YES
                          forOption:self
                           onDevice:self.device
                              block:^(BOOL reloadAllOptions, NSString *error)
    {
        block(error);
    }];
}

- (void)updateValue:(void(^)(NSString *error))block
{
    if (block)
        block(nil);
}

@end