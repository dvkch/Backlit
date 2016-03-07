//
//  SYSaneOptionGroup.m
//  SaneScanner
//
//  Created by rominet on 06/05/15.
//  Copyright (c) 2015 Syan. All rights reserved.
//

#import "SYSaneOptionGroup.h"
#import "SYSaneHelper.h"

@interface SYSaneOptionGroup ()
@end

@implementation SYSaneOptionGroup

- (void)refreshValue:(void(^)(NSString *error))block
{
    if (block)
        block(nil);
}

- (BOOL)containsOnlyAdvancedOptions
{
    for (SYSaneOption *option in self.items)
        if (!option.capAdvanced)
            return NO;
    return YES;
}

- (NSString *)valueStringWithUnit:(BOOL)withUnit
{
    return @"";
}

- (NSString *)description
{
    return [NSString stringWithFormat:@"<%@: %p, %d, %@, group containing: \n%@>",
            [self class],
            self,
            (int)self.index,
            self.title,
            [self.items componentsJoinedByString:@"\n"]];
}

@end