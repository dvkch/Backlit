//
//  SYSaneOptionDescriptorString.h
//  SaneScanner
//
//  Created by rominet on 06/05/15.
//  Copyright (c) 2015 Syan. All rights reserved.
//

#import "SYSaneOptionDescriptor.h"

@interface SYSaneOptionDescriptorString : SYSaneOptionDescriptor

@property (nonatomic, strong) NSArray <NSString *> *constraintValues;
@property (nonatomic, strong) NSString *value;

@end
