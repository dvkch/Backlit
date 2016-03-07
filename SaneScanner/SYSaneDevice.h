//
//  SYSaneDevice.h
//  SaneScanner
//
//  Created by rominet on 06/05/15.
//  Copyright (c) 2015 Syan. All rights reserved.
//

#import <Foundation/Foundation.h>
#import "sane.h"
#import "SYSaneOptionGroup.h"

@interface SYSaneDevice : NSObject

@property (nonatomic, strong) NSString *name;
@property (nonatomic, strong) NSString *type;
@property (nonatomic, strong) NSString *vendor;
@property (nonatomic, strong) NSString *model;
@property (nonatomic, strong) NSArray <SYSaneOptionGroup *> *groupedOptions;

- (instancetype)initWithCDevice:(const SANE_Device *)device;

- (NSString *)humanName;

@end
