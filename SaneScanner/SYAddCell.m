//
//  SYAddCell.m
//  SaneScanner
//
//  Created by Stan Chevallier on 09/03/2016.
//  Copyright © 2016 Syan. All rights reserved.
//

#import "SYAddCell.h"
#import "UIImage+SY.h"

static CGFloat const kPlusIconSize = 12.;
static CGFloat const kPlusIconBorderWidth = 2.;

@implementation SYAddCell

- (void)setText:(NSString *)text
{
    self->_text = text;
    
    UIColor *addGreyColor = [UIColor colorWithRed:199./255. green:199./255. blue:204./255. alpha:1.];
    
    UIImageView *imageView = [[UIImageView alloc] initWithFrame:CGRectMake(0, 0, kPlusIconSize, kPlusIconSize)];
    [imageView setImage:[UIImage sy_addIconWithColor:addGreyColor
                                                size:kPlusIconSize
                                         borderWidth:kPlusIconBorderWidth]];
    [self setAccessoryView:imageView];
    
    self.textLabel.text = text;
}

@end
