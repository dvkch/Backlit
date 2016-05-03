//
//  UIImage+SY.m
//  SaneScanner
//
//  Created by Stan Chevallier on 04/03/2016.
//  Copyright © 2016 Syan. All rights reserved.
//

#import "UIImage+SY.h"
#import "SYSaneScanParameters.h"
#import <ImageIO/ImageIO.h>

@implementation UIImage (SY)

+ (UIImage *)imageFromRGBData:(NSData *)imageBytes
               saneParameters:(SYSaneScanParameters *)parameters
                        error:(NSString **)error
{
    if (parameters.currentlyAcquiredChannel != SANE_FRAME_RGB &&
        parameters.currentlyAcquiredChannel != SANE_FRAME_GRAY)
    {
        if (error)
            *error = [NSString stringWithFormat:@"Unsupported frame type %@",
                      NSStringFromSANE_Frame(parameters.currentlyAcquiredChannel)];
        return nil;
    }
    
    if (!imageBytes) {
        if (error) *error = @"No image data";
        return nil;
    }
    
    CGColorSpaceRef colorSpaceRef = NULL;
    
    if (parameters.currentlyAcquiredChannel == SANE_FRAME_RGB)
        colorSpaceRef = CGColorSpaceCreateDeviceRGB();
    
    if (parameters.currentlyAcquiredChannel == SANE_FRAME_GRAY)
        colorSpaceRef = CGColorSpaceCreateDeviceGray();
    
    if (!colorSpaceRef) {
        if (error) *error = @"Error allocating color space";
        return nil;
    }
    
    CGDataProviderRef provider =
    CGDataProviderCreateWithData(NULL,
                                 imageBytes.bytes,
                                 imageBytes.length,
                                 NULL);
    
    CGImageRef sourceImageRef =
    CGImageCreate(parameters.width,
                  parameters.height,
                  parameters.depth,
                  parameters.depth * parameters.numberOfChannels,
                  parameters.bytesPerLine,
                  colorSpaceRef,
                  (kCGBitmapByteOrderDefault | kCGImageAlphaNone),
                  provider,   // data provider
                  NULL,       // decode
                  NO,         // should interpolate
                  kCGRenderingIntentDefault);
    
    if (!sourceImageRef) {
        if (error) *error = @"Error allocating source image ref";
        CGDataProviderRelease(provider);
        CGColorSpaceRelease(colorSpaceRef);
        return nil;
    }
    
    CGBitmapInfo destBitmapInfo = (kCGBitmapByteOrderDefault | kCGImageAlphaNoneSkipLast);
    int destNumberOfComponents = 4;
    
    if (parameters.currentlyAcquiredChannel == SANE_FRAME_GRAY)
    {
        destBitmapInfo = (kCGBitmapByteOrderDefault | kCGImageAlphaNone);
        destNumberOfComponents = 1;
    }
    
    void* pixels = malloc(parameters.width * parameters.height * destNumberOfComponents);
    
    if (!pixels) {
        if (error) *error = @"Error allocating memory for bitmap";
        CGDataProviderRelease(provider);
        CGColorSpaceRelease(colorSpaceRef);
        CGImageRelease(sourceImageRef);
        return nil;
    }

    CGContextRef context =
    CGBitmapContextCreate(pixels,
                          parameters.width,
                          parameters.height,
                          8,
                          parameters.width * destNumberOfComponents,
                          colorSpaceRef,
                          destBitmapInfo);
    
    if (!context) {
        if (error) *error = @"Error creating image context";
        CGDataProviderRelease(provider);
        CGColorSpaceRelease(colorSpaceRef);
        CGImageRelease(sourceImageRef);
        free(pixels);
        return nil;
    }
    
    CGContextDrawImage(context, (CGRect){CGPointZero, parameters.size}, sourceImageRef);
    
    CGImageRef destImageRef = CGBitmapContextCreateImage(context);
    
    UIImage *image = [UIImage imageWithCGImage:destImageRef
                                         scale:1.
                                   orientation:UIImageOrientationUp];
    
    CGDataProviderRelease(provider);
    CGColorSpaceRelease(colorSpaceRef);
    CGImageRelease(sourceImageRef);
    CGImageRelease(destImageRef);
    CGContextRelease(context);
    free(pixels);
    
    return image;
}

+ (UIImage *)imageFromIncompleteRGBData:(NSData *)data saneParameters:(SYSaneScanParameters *)parameters error:(NSString **)error
{
    SYSaneScanParameters *incompleteParams = [SYSaneScanParameters parametersForIncompleteDataOfLength:data.length
                                                                                    completeParameters:parameters];
    
    if (!incompleteParams.fileSize)
        return nil;
    
    UIImage *image = [self imageFromRGBData:data saneParameters:incompleteParams error:error];
    
    if (image)
    {
        CGRect fullRect = CGRectMake(0, 0, parameters.width, parameters.height);
        UIGraphicsBeginImageContextWithOptions(fullRect.size, NO, 1.0);
        [[UIColor whiteColor] setFill];
        [[UIBezierPath bezierPathWithRect:fullRect] fill];
        [image drawInRect:(CGRect){CGPointZero, image.size}];
        image = UIGraphicsGetImageFromCurrentImageContext();
        UIGraphicsEndImageContext();
    }
    
    return image;
}

+ (UIImage *)addIconWithColor:(UIColor *)color
{
    // iOS defaults @2x for UIBarButtonItemSystemAdd
    return [self addIconWithColor:color size:35. borderWidth:3.];
}

+ (UIImage *)addIconWithColor:(UIColor *)color size:(CGFloat)size borderWidth:(CGFloat)borderWidth
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

