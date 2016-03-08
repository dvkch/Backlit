//
//  SYPreviewCell.h
//  SaneScanner
//
//  Created by Stan Chevallier on 08/03/2016.
//  Copyright © 2016 Syan. All rights reserved.
//

#import <UIKit/UIKit.h>

@class SYSaneDevice;

@interface SYPreviewCell : UITableViewCell

@property (nonatomic, strong) SYSaneDevice *device;

+ (CGFloat)cellHeightForDevice:(SYSaneDevice *)device
                         width:(CGFloat)width;

@end
