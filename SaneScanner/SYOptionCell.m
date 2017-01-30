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
#import "SYPreferences.h"
#import <Masonry.h>
#import <UITableViewCell+SYKit.h>

@interface SYOptionCell ()
@property (nonatomic, strong) UILabel *labelTitle;
@property (nonatomic, strong) UILabel *labelValue;
@property (nonatomic, strong) UILabel *labelDescr;
@property (nonatomic, strong) MASConstraint *constraingLabelDescrHeight;
@end

@implementation SYOptionCell

- (instancetype)initWithStyle:(UITableViewCellStyle)style reuseIdentifier:(NSString *)reuseIdentifier
{
    self = [super initWithStyle:style reuseIdentifier:reuseIdentifier];
    if(self)
    {
        // prevents constraint errors when cell width is still 0
        [self setTranslatesAutoresizingMaskIntoConstraints:NO];
        
        [self.contentView setClipsToBounds:YES];
        
        self.labelTitle = [[UILabel alloc] init];
        [self.labelTitle setBackgroundColor:[UIColor clearColor]];
        [self.contentView addSubview:self.labelTitle];
        
        self.labelValue = [[UILabel alloc] init];
        [self.labelValue setBackgroundColor:[UIColor clearColor]];
        [self.labelValue setTextAlignment:NSTextAlignmentRight];
        [self.contentView addSubview:self.labelValue];
        
        self.labelDescr = [[UILabel alloc] init];
        [self.labelDescr setBackgroundColor:[UIColor clearColor]];
        [self.labelDescr setNumberOfLines:0];
        [self.labelDescr setLineBreakMode:NSLineBreakByWordWrapping];
        [self.labelDescr setFont:[UIFont systemFontOfSize:self.labelDescr.font.pointSize - 2]];
        [self.contentView addSubview:self.labelDescr];
        
        [self.labelTitle mas_makeConstraints:^(MASConstraintMaker *make) {
            make.top.equalTo(@10);
            make.left.equalTo(@10);
            make.right.equalTo(self.labelValue.mas_left).offset(-10);
        }];
        
        [self.labelValue mas_makeConstraints:^(MASConstraintMaker *make) {
            make.top.equalTo(@10);
            make.right.equalTo(@(-10));
        }];
        
        [self.labelDescr mas_makeConstraints:^(MASConstraintMaker *make) {
            make.top.equalTo(self.labelTitle.mas_bottom).offset(self.showDescription ? 10 : 0);
            make.left.equalTo(@10);
            make.right.equalTo(@(-10));//.priorityLow();
            make.bottom.equalTo(@(-10)).priorityLow();
        }];
    }
    return self;
}

- (void)setPrefKey:(NSString *)prefKey
{
    self->_prefKey = prefKey;
    self->_option  = nil;
    [self updateTexts];
}

- (void)setOption:(SYSaneOption *)option
{
    self->_option  = option;
    self->_prefKey = nil;
    [self updateTexts];
}

- (void)updateTexts
{
    UIColor *backgroundColor = [UIColor whiteColor];
    UIColor *normalTextColor = [UIColor darkTextColor];
    UIColor *descTextColor   = [UIColor grayColor];
    
    if (self.option && self.option.disabledOrReadOnly)
    {
        backgroundColor = [UIColor colorWithWhite:0.98 alpha:1.];
        normalTextColor = [UIColor lightGrayColor];
        descTextColor   = [UIColor lightGrayColor];
    }
    
    [self setBackgroundColor:backgroundColor];
    [self.labelTitle setTextColor:normalTextColor];
    [self.labelValue setTextColor:normalTextColor];
    [self.labelDescr setTextColor:descTextColor];
    
    if (self.option)
    {
        [self.labelTitle setText:self.option.title];
        [self.labelValue setText:[self.option valueStringWithUnit:YES]];
        [self.labelDescr setText:self.option.desc];
    }
    else if (self.prefKey)
    {
        [self.labelTitle setText:[[SYPreferences shared] titleForKey:self.prefKey]];
        [self.labelDescr setText:[[SYPreferences shared] descriptionForKey:self.prefKey]];
        
        switch ([[SYPreferences shared] typeForKey:self.prefKey])
        {
            case SYPreferenceTypeBool:
                [self.labelValue setText:[[[SYPreferences shared] objectForKey:self.prefKey] boolValue] ? $("OPTION BOOL ON") : $("OPTION BOOL OFF")];
                break;
            case SYPreferenceTypeInt:
                [self.labelValue setText:[[[SYPreferences shared] objectForKey:self.prefKey] stringValue]];
                break;
            case SYPreferenceTypeString:
            case SYPreferenceTypeUnknown:
                [self.labelValue setText:[[SYPreferences shared] objectForKey:self.prefKey]];
                break;
        }
    }
}

- (void)setShowDescription:(BOOL)showDescription
{
    self->_showDescription = showDescription;
    [self setNeedsUpdateConstraints];
}

- (void)updateConstraints
{
    [super updateConstraints];
    
    if (self.showDescription && self.constraingLabelDescrHeight)
    {
        [self.constraingLabelDescrHeight uninstall];
        self.constraingLabelDescrHeight = nil;
    }
    
    if (!self.showDescription && !self.constraingLabelDescrHeight)
    {
        [self.labelDescr mas_makeConstraints:^(MASConstraintMaker *make) {
            self.constraingLabelDescrHeight = make.height.equalTo(@0);
        }];
    }
}

+ (CGFloat)cellHeightForOption:(SYSaneOption *)option showDescription:(BOOL)showDescription width:(CGFloat)width
{
    return [self sy_cellHeightForWidth:width configurationBlock:^(UITableViewCell *sizingCell) {
        [(SYOptionCell *)sizingCell setOption:option];
        [(SYOptionCell *)sizingCell setShowDescription:showDescription];
    }];
}

+ (CGFloat)cellHeightForPrefKey:(NSString *)prefKey showDescription:(BOOL)showDescription width:(CGFloat)width
{
    return [self sy_cellHeightForWidth:width configurationBlock:^(UITableViewCell *sizingCell) {
        [(SYOptionCell *)sizingCell setPrefKey:prefKey];
        [(SYOptionCell *)sizingCell setShowDescription:showDescription];
    }];
}

@end
