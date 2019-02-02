//
//  UITableViewCell+SYKit.m
//  SYKit
//
//  Created by Stan Chevallier on 12/04/2016.
//  Copyright © 2016 Syan. All rights reserved.
//

#import "UITableViewCell+SYKit.h"

@implementation UITableViewCell (SYKit)

- (CGFloat)sy_cellHeightForWidth:(CGFloat)width NS_AVAILABLE_IOS(6_0);
{
    [self setTranslatesAutoresizingMaskIntoConstraints:NO];
    
    [self setFrame:CGRectMake(0, 0, width, 8000.)];
    [self setNeedsUpdateConstraints];
    [self updateConstraintsIfNeeded];
    [self setNeedsLayout];
    [self layoutIfNeeded];
    
    NSLayoutConstraint *widthConstraint = [NSLayoutConstraint constraintWithItem:self.contentView
                                                                       attribute:NSLayoutAttributeWidth
                                                                       relatedBy:NSLayoutRelationEqual
                                                                          toItem:nil
                                                                       attribute:NSLayoutAttributeNotAnAttribute
                                                                      multiplier:1.
                                                                        constant:width];
    [self.contentView addConstraint:widthConstraint];
    [self.contentView layoutIfNeeded];
    
    CGSize size = [self.contentView systemLayoutSizeFittingSize:CGSizeMake(width, 8000)];
    [self.contentView removeConstraint:widthConstraint];
    
    return ceil(size.height);
}

@end
