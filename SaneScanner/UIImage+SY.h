//
//  UIImage+SY.h
//  SaneScanner
//
//  Created by Stan Chevallier on 04/03/2016.
//  Copyright © 2016 Syan. All rights reserved.
//

#import <UIKit/UIKit.h>

@class SYSaneScanParameters;

@interface UIImage (SY)

+ (UIImage *)imageFromRGBData:(NSData *)data saneParameters:(SYSaneScanParameters *)parameters error:(NSString **)error;
+ (UIImage *)imageFromIncompleteRGBData:(NSData *)data saneParameters:(SYSaneScanParameters *)parameters error:(NSString **)error;

+ (UIImage *)addIconWithColor:(UIColor *)color;
+ (UIImage *)addIconWithColor:(UIColor *)color size:(CGFloat)size borderWidth:(CGFloat)borderWidth;

+ (CGSize)sy_sizeOfImageAtURL:(NSURL *)url;
+ (NSNumber *)sy_isImageFileValidAtURL:(NSURL *)url;
+ (UIImage *)sy_validImageWithContentsOfFile:(NSString *)path;

@end
