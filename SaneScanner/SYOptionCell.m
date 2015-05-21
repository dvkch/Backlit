//
//  SYOptionCell.m
//  SaneScanner
//
//  Created by rominet on 06/05/15.
//  Copyright (c) 2015 Syan. All rights reserved.
//

#import "SYOptionCell.h"
#import "SYSaneOptionDescriptor.h"

@interface SYOptionCell ()
@property (nonatomic, strong) UILabel *labelTitle;
@property (nonatomic, strong) UILabel *labelValue;
@end

@implementation SYOptionCell

- (instancetype)initWithStyle:(UITableViewCellStyle)style reuseIdentifier:(NSString *)reuseIdentifier
{
    self = [super initWithStyle:style reuseIdentifier:reuseIdentifier];
    if(self)
    {
        self.labelTitle = [[UILabel alloc] init];
        [self.contentView addSubview:self.labelTitle];
        
        self.labelValue = [[UILabel alloc] init];
        [self.labelValue setTextAlignment:NSTextAlignmentRight];
        [self.contentView addSubview:self.labelValue];
    }
    return self;
}

- (void)setOption:(SYSaneOptionDescriptor *)option
{
    self->_option = option;
    [self.labelTitle setText:self.option.title];
    //[self.labelValue setText:NSStringFromSANE_Value_Type(self.option.type)];
    
    if(self.option.type == SANE_TYPE_BOOL) {
        [self.labelValue setText:[(NSNumber *)option.value stringValue]];
    }
    else if(self.option.type == SANE_TYPE_INT) {
        [self.labelValue setText:[(NSNumber *)option.value stringValue]];
    }
    else if(self.option.type == SANE_TYPE_FIXED) {
        [self.labelValue setText:[(NSNumber *)option.value stringValue]];
    }
    else if(self.option.type == SANE_TYPE_STRING) {
        [self.labelValue setText:[(NSString *)option.value description]];
    }
}

- (void)layoutSubviews
{
    [super layoutSubviews];
    
    CGFloat m = 10;
    CGFloat w = self.contentView.frame.size.width  - 3. * m;
    CGFloat h = self.contentView.frame.size.height - 2. * m;
    
    [self.labelTitle setFrame:CGRectMake(m,                 m, w * 2./3., h)];
    [self.labelValue setFrame:CGRectMake(2 * m + 2./3. * w, m, w * 1./3., h)];
}

@end
