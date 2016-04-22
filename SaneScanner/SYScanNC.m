//
//  SYScanNC.m
//  SaneScanner
//
//  Created by Stan Chevallier on 21/04/2016.
//  Copyright © 2016 Syan. All rights reserved.
//

#import "SYScanNC.h"

@interface SYScanNC ()

@end

@implementation SYScanNC

@dynamic toolbar;

- (instancetype)initWithNavigationBarClass:(Class)navigationBarClass toolbarClass:(Class)toolbarClass
{
    self = [super initWithNavigationBarClass:navigationBarClass toolbarClass:[SYToolbar class]];
    return self;
}

- (instancetype)init
{
    self = [super initWithNavigationBarClass:nil toolbarClass:[SYToolbar class]];
    return self;
}

@end
