//
//  UIImage+SY.m
//  SaneScanner
//
//  Created by Stan Chevallier on 04/03/2016.
//  Copyright © 2016 Syan. All rights reserved.
//

#import "UIImage+SY.h"
#import "SYSaneScanParameters.h"

@implementation UIImage (SY)

+ (UIImage *)imageFromRGBData:(NSData *)data saneParameters:(SYSaneScanParameters *)parameters
{
    CGDataProviderRef provider = CGDataProviderCreateWithData(NULL, [data bytes], [data length], NULL);
    
    CGColorSpaceRef colorSpaceRef = CGColorSpaceCreateDeviceRGB();
    if(colorSpaceRef == NULL) {
        NSLog(@"Error allocating color space");
        CGDataProviderRelease(provider);
        return nil;
    }
    CGColorRenderingIntent renderingIntent = kCGRenderingIntentDefault;
    
    CGImageRef iref = CGImageCreate(parameters.width,
                                    parameters.height,
                                    parameters.depth,
                                    parameters.depth * 3,
                                    parameters.bytesPerLine,
                                    colorSpaceRef,
                                    (kCGBitmapByteOrderDefault | kCGImageAlphaNone),
                                    provider,	// data provider
                                    NULL,		// decode
                                    YES,			// should interpolate
                                    renderingIntent);
    
    uint32_t* pixels = (uint32_t*)malloc([data length]*4/3);
    
    if(pixels == NULL) {
        NSLog(@"Error: Memory not allocated for bitmap");
        CGDataProviderRelease(provider);
        CGColorSpaceRelease(colorSpaceRef);
        CGImageRelease(iref);
        return nil;
    }
    
    CGContextRef context = CGBitmapContextCreate(pixels,
                                                 parameters.width,
                                                 parameters.height,
                                                 parameters.depth,
                                                 parameters.width * 4,
                                                 colorSpaceRef,
                                                 (kCGBitmapByteOrderDefault | kCGImageAlphaNoneSkipLast));
    
    if(context == NULL) {
        NSLog(@"Error context not created");
        //if (pixels)
            free(pixels);
    }
    
    UIImage *image = nil;
    if(context) {
        
        CGContextDrawImage(context, CGRectMake(0.0f, 0.0f, parameters.width, parameters.height), iref);
        
        CGImageRef imageRef = CGBitmapContextCreateImage(context);
        
        // Support both iPad 3.2 and iPhone 4 Retina displays with the correct scale
        if([UIImage respondsToSelector:@selector(imageWithCGImage:scale:orientation:)]) {
            float scale = [[UIScreen mainScreen] scale];
            image = [UIImage imageWithCGImage:imageRef scale:scale orientation:UIImageOrientationUp];
        } else {
            image = [UIImage imageWithCGImage:imageRef];
        }
        
        CGImageRelease(imageRef);	
        CGContextRelease(context);	
    }
    
    CGColorSpaceRelease(colorSpaceRef);
    CGImageRelease(iref);
    CGDataProviderRelease(provider);
    
    if(pixels) {
        free(pixels);
    }	
    return image;
}

@end
