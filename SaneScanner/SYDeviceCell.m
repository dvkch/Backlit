//
//  SYDeviceCell.m
//  SaneScanner
//
//  Created by Stanislas Chevallier on 31/01/2017.
//  Copyright © 2017 Syan. All rights reserved.
//

#import "SYDeviceCell.h"
#import "SYSaneDevice.h"

@implementation SYDeviceCell

- (instancetype)initWithStyle:(UITableViewCellStyle)style reuseIdentifier:(NSString *)reuseIdentifier
{
    self = [super initWithStyle:UITableViewCellStyleSubtitle reuseIdentifier:reuseIdentifier];
    if (self)
    {
        [self setAccessoryType:UITableViewCellAccessoryDisclosureIndicator];
    }
    return self;
}

- (void)setDevice:(SYSaneDevice *)device
{
    self->_device = device;
    [self.textLabel setText:device.model];
    
    NSMutableArray <NSString *> *parts = [NSMutableArray array];
    if (device.host) [parts addObject:device.host];
    if (device.type) [parts addObject:device.type];
    [self.detailTextLabel setText:[parts componentsJoinedByString:$$(" – ")]];
}

@end
