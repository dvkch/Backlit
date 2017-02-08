//
//  SYOptionCell.h
//  SaneScanner
//
//  Created by rominet on 06/05/15.
//  Copyright (c) 2015 Syan. All rights reserved.
//

#import <UIKit/UIKit.h>

@class SYSaneOption;

@interface SYOptionCell : UITableViewCell

@property (nonatomic, strong) SYSaneOption *option;
@property (nonatomic, strong) NSString *prefKey;
@property (nonatomic, assign) BOOL showDescription;

- (void)updateWithLeftText:(NSString *)leftText rightText:(NSString *)rightText;

+ (CGFloat)cellHeightForOption:(SYSaneOption *)option showDescription:(BOOL)showDescription width:(CGFloat)width;
+ (CGFloat)cellHeightForPrefKey:(NSString *)prefKey showDescription:(BOOL)showDescription width:(CGFloat)width;
+ (CGFloat)cellHeightForLeftText:(NSString *)leftText rightText:(NSString *)rightText width:(CGFloat)width;

@end
