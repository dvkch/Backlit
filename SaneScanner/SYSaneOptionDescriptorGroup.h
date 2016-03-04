//
//  SYSaneOptionDescriptorGroup.h
//  SaneScanner
//
//  Created by rominet on 06/05/15.
//  Copyright (c) 2015 Syan. All rights reserved.
//

#import "SYSaneOptionDescriptor.h"

@interface SYSaneOptionDescriptorGroup : SYSaneOptionDescriptor

@property (nonatomic, strong) NSArray <SYSaneOptionDescriptor *> *items;

- (BOOL)containsOnlyAdvancedOptions;

@end
