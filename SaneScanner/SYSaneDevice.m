//
//  SYSaneDevice.m
//  SaneScanner
//
//  Created by rominet on 06/05/15.
//  Copyright (c) 2015 Syan. All rights reserved.
//

#import "SYSaneDevice.h"

@implementation SYSaneDevice

- (instancetype)initWithCDevice:(const SANE_Device *)device
{
    if(!device)
        return nil;
    
    self = [super init];
    if (self)
    {
        self.name   = [NSString stringWithCString:(device->name   ?: "") encoding:NSUTF8StringEncoding];
        self.model  = [NSString stringWithCString:(device->model  ?: "") encoding:NSUTF8StringEncoding];
        self.vendor = [NSString stringWithCString:(device->vendor ?: "") encoding:NSUTF8StringEncoding];
        self.type   = [NSString stringWithCString:(device->type   ?: "") encoding:NSUTF8StringEncoding];
    }
    return self;
}

- (NSString *)humanName
{
    return [NSString stringWithFormat:@"%@ %@", self.vendor, self.model];
}

@end
