//
//  SYOptionCell.m
//  SaneScanner
//
//  Created by rominet on 06/05/15.
//  Copyright (c) 2015 Syan. All rights reserved.
//

#import "SYOptionCell.h"
#import "SYSaneOption.h"
#import "SYSaneOptionBool.h"
#import "SYSaneOptionNumber.h"
#import "SYSaneOptionString.h"
#import "SYSaneOptionButton.h"
#import "SYSaneOptionGroup.h"
#import <Masonry.h>

@interface SYOptionCell ()
@property (nonatomic, strong) UILabel *labelTitle;
@property (nonatomic, strong) UILabel *labelValue;
@property (nonatomic, strong) UILabel *labelDescr;
@end

@implementation SYOptionCell

- (instancetype)initWithStyle:(UITableViewCellStyle)style reuseIdentifier:(NSString *)reuseIdentifier
{
    self = [super initWithStyle:style reuseIdentifier:reuseIdentifier];
    if(self)
    {
        [self.contentView setClipsToBounds:YES];
        
        self.labelTitle = [[UILabel alloc] init];
        [self.contentView addSubview:self.labelTitle];
        
        self.labelValue = [[UILabel alloc] init];
        [self.labelValue setTextAlignment:NSTextAlignmentRight];
        [self.contentView addSubview:self.labelValue];
        
        self.labelDescr = [[UILabel alloc] init];
        [self.labelDescr setNumberOfLines:0];
        [self.labelDescr setLineBreakMode:NSLineBreakByWordWrapping];
        [self.labelDescr setTextColor:[UIColor lightGrayColor]];
        [self.labelDescr setFont:[UIFont systemFontOfSize:self.labelDescr.font.pointSize - 2]];
        [self.contentView addSubview:self.labelDescr];
        
        [self setNeedsUpdateConstraints];
    }
    return self;
}

- (void)setOption:(SYSaneOption *)option
{
    self->_option = option;
    
    [self setBackgroundColor:(!option.disabledOrReadOnly ?
                              [UIColor whiteColor] :
                              [UIColor colorWithWhite:0.98 alpha:1.])];
    
    UIColor *textColor = (!option.disabledOrReadOnly ?
                          [UIColor darkTextColor] :
                          [UIColor lightGrayColor]);
    
    [self.labelTitle setTextColor:textColor];
    [self.labelValue setTextColor:textColor];
    [self.labelDescr setTextColor:textColor];
    
    [self.labelTitle setText:self.option.title];
    [self.labelValue setText:[self.option valueStringWithUnit:YES]];
    [self.labelDescr setText:self.option.desc];
}

- (void)setShowDescription:(BOOL)showDescription
{
    self->_showDescription = showDescription;
    [self setNeedsUpdateConstraints];
}

- (void)updateConstraints
{
    [super updateConstraints];
    
    [self.labelTitle mas_remakeConstraints:^(MASConstraintMaker *make) {
        make.top.equalTo(@10);
        make.left.equalTo(@10);
        make.right.equalTo(self.labelValue.mas_left).offset(-10);
    }];
    
    [self.labelValue mas_remakeConstraints:^(MASConstraintMaker *make) {
        make.top.equalTo(@10);
        make.right.equalTo(@(-10));
    }];
    
    [self.labelDescr mas_remakeConstraints:^(MASConstraintMaker *make) {
        make.top.equalTo(self.labelTitle.mas_bottom).offset(self.showDescription ? 6 : 0);
        make.left.equalTo(@10);
        make.right.equalTo(@(-10));
        if (!self.showDescription)
            make.height.equalTo(@0);
        make.bottom.equalTo(@(-10)).priorityLow();
    }];
}

static SYOptionCell *sizingCell;

+ (CGFloat)cellHeightForOption:(SYSaneOption *)option showDescription:(BOOL)showDescription width:(CGFloat)width
{
    if (!sizingCell)
    {
        sizingCell = [[self alloc] initWithStyle:UITableViewCellStyleDefault reuseIdentifier:@"sizingCellSYOptionCell"];
    }
    
    [sizingCell setOption:option];
    [sizingCell setShowDescription:showDescription];
    [sizingCell setFrame:CGRectMake(0, 0, width, 1000.)];
    [sizingCell setNeedsUpdateConstraints];
    [sizingCell updateConstraintsIfNeeded];
    [sizingCell setNeedsLayout];
    [sizingCell layoutIfNeeded];
    [sizingCell.contentView layoutIfNeeded];
    CGSize size = [sizingCell.contentView systemLayoutSizeFittingSize:CGSizeMake(width, 1000.)
                                        withHorizontalFittingPriority:UILayoutPriorityRequired
                                              verticalFittingPriority:UILayoutPriorityFittingSizeLevel];
    return size.height + 1;
}

@end
