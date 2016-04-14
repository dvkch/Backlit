//
//  SYPreviewCell.m
//  SaneScanner
//
//  Created by Stan Chevallier on 08/03/2016.
//  Copyright © 2016 Syan. All rights reserved.
//

#import "SYPreviewCell.h"
#import "SYSanePreviewView.h"
#import "SYSaneDevice.h"
#import "SYSaneOptionNumber.h"
#import <Masonry.h>

@interface SYPreviewCell ()
@property (nonatomic, strong) SYSanePreviewView *previewView;
@end

@implementation SYPreviewCell

- (instancetype)initWithStyle:(UITableViewCellStyle)style reuseIdentifier:(NSString *)reuseIdentifier
{
    self = [super initWithStyle:style reuseIdentifier:reuseIdentifier];
    if (self)
    {
        // prevents constraint errors when cell width is still 0
        [self setTranslatesAutoresizingMaskIntoConstraints:NO];
    
        [self setBackgroundColor:[UIColor clearColor]];
        [self setSelectionStyle:UITableViewCellSelectionStyleNone];
        
        self.previewView = [[SYSanePreviewView alloc] init];
        [self.contentView addSubview:self.previewView];
        
        [self.previewView mas_makeConstraints:^(MASConstraintMaker *make) {
            make.edges.equalTo(@0);
        }];
    }
    return self;
}

- (void)setDevice:(SYSaneDevice *)device
{
    self->_device = device;
    [self.previewView setDevice:device];
}

+ (CGFloat)cellHeightForDevice:(SYSaneDevice *)device
                         width:(CGFloat)width
                     maxHeight:(CGFloat)maxHeight
{
    CGFloat bestHeight = width / device.previewImageRatio;
    return MIN(MAX(bestHeight, width), maxHeight);
}

@end
