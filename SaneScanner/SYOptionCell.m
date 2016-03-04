//
//  SYOptionCell.m
//  SaneScanner
//
//  Created by rominet on 06/05/15.
//  Copyright (c) 2015 Syan. All rights reserved.
//

#import "SYOptionCell.h"
#import "SYSaneOptionDescriptor.h"
#import "SYSaneOptionDescriptorBool.h"
#import "SYSaneOptionDescriptorInt.h"
#import "SYSaneOptionDescriptorFloat.h"
#import "SYSaneOptionDescriptorString.h"
#import "SYSaneOptionDescriptorButton.h"
#import "SYSaneOptionDescriptorGroup.h"

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
        [self.contentView setClipsToBounds:YES];
        [self.contentView.layer setMasksToBounds:YES];
        
        self.labelTitle = [[UILabel alloc] init];
        [self.labelTitle setClipsToBounds:YES];
        [self.contentView addSubview:self.labelTitle];
        
        self.labelValue = [[UILabel alloc] init];
        [self.labelValue setTextAlignment:NSTextAlignmentRight];
        [self.labelValue setClipsToBounds:YES];
        [self.contentView addSubview:self.labelValue];
    }
    return self;
}

- (void)setOption:(SYSaneOptionDescriptor *)option
{
    self->_option = option;
    [self.labelTitle setText:self.option.title];
    [self.labelValue setText:@""];
    //[self.labelValue setText:NSStringFromSANE_Value_Type(self.option.type)];
    
    NSString *valueString = nil;
    if(self.option.type == SANE_TYPE_BOOL) {
        SYSaneOptionDescriptorBool *castedOption = (SYSaneOptionDescriptorBool *)option;
        valueString = (castedOption.value ? @"On" : @"Off");
    }
    else if(self.option.type == SANE_TYPE_INT) {
        SYSaneOptionDescriptorInt *castedOption = (SYSaneOptionDescriptorInt *)option;
        valueString = [NSString stringWithFormat:@"%d", castedOption.value];
    }
    else if(self.option.type == SANE_TYPE_FIXED) {
        SYSaneOptionDescriptorFloat *castedOption = (SYSaneOptionDescriptorFloat *)option;
        valueString = [NSString stringWithFormat:@"%.02lf", castedOption.value];
    }
    else if(self.option.type == SANE_TYPE_STRING) {
        SYSaneOptionDescriptorString *castedOption = (SYSaneOptionDescriptorString *)option;
        valueString = castedOption.value;
    }
    
    if (self.option.unit != SANE_UNIT_NONE)
        valueString = [valueString stringByAppendingFormat:@" %@", NSStringFromSANE_Unit(self.option.unit)];
    
    [self.labelValue setText:valueString];
}

- (void)layoutSubviews
{
    [super layoutSubviews];
    
    CGFloat m = 10;
    CGFloat w = MAX(0, self.contentView.frame.size.width  - 3. * m);
    CGFloat h = MAX(0, self.contentView.frame.size.height - 2. * m);
    
    [self.labelTitle setFrame:CGRectMake(m,                 m, w * 2./3., h)];
    [self.labelValue setFrame:CGRectMake(2 * m + 2./3. * w, m, w * 1./3., h)];
}

@end
