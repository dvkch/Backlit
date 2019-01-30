//
//  UIImage+SY.h
//  SaneScanner
//
//  Created by Stan Chevallier on 04/03/2016.
//  Copyright © 2016 Syan. All rights reserved.
//

#import <UIKit/UIKit.h>

@interface UIImage (SY)

+ (UIImage *)sy_addIconWithColor:(UIColor *)color;
+ (UIImage *)sy_addIconWithColor:(UIColor *)color size:(CGFloat)size borderWidth:(CGFloat)borderWidth;

@end
