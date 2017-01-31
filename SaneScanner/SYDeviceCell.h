//
//  SYDeviceCell.h
//  SaneScanner
//
//  Created by Stanislas Chevallier on 31/01/2017.
//  Copyright © 2017 Syan. All rights reserved.
//

#import <UIKit/UIKit.h>

@class SYSaneDevice;

@interface SYDeviceCell : UITableViewCell

@property (nonatomic, strong) SYSaneDevice *device;

@end
