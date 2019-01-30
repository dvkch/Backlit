//
//  UIImage+SY.m
//  SaneScanner
//
//  Created by Stan Chevallier on 04/03/2016.
//  Copyright © 2016 Syan. All rights reserved.
//

#import "UIImage+SY.h"
#import <UIImage+SYKit.h>
#import "SYSaneScanParameters.h"
#import <ImageIO/ImageIO.h>
#import "SYTools.h"

@implementation UIImage (SY)

+ (UIImage *)sy_addIconWithColor:(UIColor *)color
{
    // iOS defaults @2x for UIBarButtonItemSystemAdd
    return [self sy_addIconWithColor:color
                                size:35.
                         borderWidth:3.];
}

+ (UIImage *)sy_addIconWithColor:(UIColor *)color
                            size:(CGFloat)size
                     borderWidth:(CGFloat)borderWidth
{
    CGRect verticalRect   = CGRectMake((size - borderWidth)/2., 0, borderWidth, size);
    CGRect horizontalRect = CGRectMake(0, (size - borderWidth)/2., size, borderWidth);
    CGRect wholeRect      = CGRectMake(0, 0, size, size);
    
    UIGraphicsBeginImageContextWithOptions(wholeRect.size, NO, 0.0);
    
    [color setFill];
    
    UIBezierPath *path = [UIBezierPath bezierPathWithRect:verticalRect];
    [path appendPath:[UIBezierPath bezierPathWithRect:horizontalRect]];
    [path fill];
    
    UIImage *img = UIGraphicsGetImageFromCurrentImageContext();
    UIGraphicsEndImageContext();
    
    return img;
}

@end

