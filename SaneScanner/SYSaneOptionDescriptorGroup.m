//
//  SYSaneOptionDescriptorGroup.m
//  SaneScanner
//
//  Created by rominet on 06/05/15.
//  Copyright (c) 2015 Syan. All rights reserved.
//

#import "SYSaneOptionDescriptorGroup.h"
#import "SYSaneHelper.h"

@interface SYSaneOptionDescriptorGroup ()
@end

@implementation SYSaneOptionDescriptorGroup

- (void)updateValue:(void(^)(NSString *error))block
{
    if (block)
        block(nil);
}

- (BOOL)containsOnlyAdvancedOptions
{
    for (SYSaneOptionDescriptor *option in self.items)
        if (!option.capAdvanced)
            return NO;
    return YES;
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