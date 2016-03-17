//
//  SYToolbar.h
//  SaneScanner
//
//  Created by Stan Chevallier on 15/03/2016.
//  Copyright © 2016 Syan. All rights reserved.
//

#import <UIKit/UIKit.h>

@interface SYToolbar : UIToolbar

@property (nonatomic) CGFloat padding;
@property (nonatomic) CGFloat height;

+ (CGFloat)systemPadding;

@end
