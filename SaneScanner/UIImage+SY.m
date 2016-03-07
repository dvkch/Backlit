//
//  UIImage+SY.m
//  SaneScanner
//
//  Created by Stan Chevallier on 04/03/2016.
//  Copyright © 2016 Syan. All rights reserved.
//

#import "UIImage+SY.h"
#import "SYSaneScanParameters.h"

NSData *NSDataRGB888FromSaneFrame(NSData *data, SYSaneScanParameters *parameters, NSString **error);
NSData *NSDataRGB888FromBlack1(NSData *data, SYSaneScanParameters *parameters);
NSData *NSDataRGB888FromGrey8(NSData *data, SYSaneScanParameters *parameters);

@implementation UIImage (SY)

+ (UIImage *)imageFromRGBData:(NSData *)data
               saneParameters:(SYSaneScanParameters *)parameters
                        error:(NSString **)error
{
    NSData *imageBytes = data; //NSDataRGB888FromSaneFrame(data, parameters, error);
    if (!imageBytes) {
        return nil;
    }
    
    CGDataProviderRef provider = CGDataProviderCreateWithData(NULL, [imageBytes bytes], [imageBytes length], NULL);
    
    CGColorSpaceRef colorSpaceRef;
    
    switch (parameters.currentlyAcquiredChannel)
    {
        case SANE_FRAME_RGB:
            colorSpaceRef = CGColorSpaceCreateDeviceRGB();
            break;
        case SANE_FRAME_GRAY:
            colorSpaceRef = CGColorSpaceCreateDeviceGray();
            break;
        default:
            if (error)
                *error = [NSString stringWithFormat:@"Unsupported frame type %@",
                          NSStringFromSANE_Frame(parameters.currentlyAcquiredChannel)];
            break;
    }
    
    if (!colorSpaceRef) {
        if (error)
            *error = @"Error allocating color space";
        
        CGDataProviderRelease(provider);
        return nil;
    }
    
    CGColorRenderingIntent renderingIntent = kCGRenderingIntentDefault;
    
    CGImageRef iref = CGImageCreate(parameters.width,
                                    parameters.height,
                                    parameters.depth,
                                    parameters.depth * parameters.numberOfChannels,
                                    parameters.bytesPerLine,
                                    colorSpaceRef,
                                    (kCGBitmapByteOrderDefault | kCGImageAlphaNone),
                                    provider,	// data provider
                                    NULL,		// decode
                                    YES,			// should interpolate
                                    renderingIntent);
    
    uint32_t* pixels = (uint32_t*)malloc(parameters.width * parameters.height * 4);
    
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
                                                 8,
                                                 parameters.width * 4,
                                                 CGColorSpaceCreateDeviceRGB(),
                                                 (kCGBitmapByteOrderDefault | kCGImageAlphaNoneSkipLast));
    
    if(context == NULL) {
        NSLog(@"Error context not created");
        free(pixels);
        CGColorSpaceRelease(colorSpaceRef);
        CGImageRelease(iref);
        CGDataProviderRelease(provider);
        return nil;
    }
    
    UIImage *image = nil;
    CGContextDrawImage(context, CGRectMake(0.0f, 0.0f, parameters.width, parameters.height), iref);
    
    CGImageRef imageRef = CGBitmapContextCreateImage(context);
    
    image = [UIImage imageWithCGImage:imageRef
                                scale:1.
                          orientation:UIImageOrientationUp];
    
    CGImageRelease(imageRef);
    CGContextRelease(context);
    CGColorSpaceRelease(colorSpaceRef);
    CGImageRelease(iref);
    CGDataProviderRelease(provider);
    
    //if(pixels) {
    //    free(pixels);
    //}
    return image;
}

@end

NSData *NSDataRGB888FromSaneFrame(NSData *data, SYSaneScanParameters *parameters, NSString **error)
{
    NSData *imageBytes;
    switch (parameters.currentlyAcquiredChannel)
    {
        case SANE_FRAME_RGB:
            imageBytes = data;
            break;
        case SANE_FRAME_GRAY:
            if (parameters.depth == 1)
                imageBytes = NSDataRGB888FromBlack1(data, parameters);
            else
                imageBytes = NSDataRGB888FromGrey8(data, parameters);
            break;
        default:
            if (error)
                *error = [NSString stringWithFormat:@"Unsupported frame type %@",
                          NSStringFromSANE_Frame(parameters.currentlyAcquiredChannel)];
            break;
    }
    return imageBytes;
}

NSData *NSDataRGB888FromBlack1(NSData *data, SYSaneScanParameters *parameters)
{
    NSMutableData *imageBytes = [NSMutableData dataWithCapacity:parameters.width*parameters.height*3];
    
    u_int8_t *bufferIn  = malloc(1);
    u_int8_t *bufferOut = malloc(3);
    
    for (NSUInteger i = 0; i < data.length; ++i)
    {
        [data getBytes:bufferIn range:NSMakeRange(i, 1)];
        
        for (NSUInteger j = 0; j < 8; ++j)
        {
            BOOL value = bufferIn[0] & (1 << j);
            bufferOut[0] = (!value) * 0xFF;
            bufferOut[1] = (!value) * 0xFF;
            bufferOut[2] = (!value) * 0xFF;
            [imageBytes appendBytes:bufferOut length:3];
        }
    }
    
    parameters.depth = 8;
    parameters.currentlyAcquiredChannel = SANE_FRAME_RGB;
    parameters.bytesPerLine = parameters.numberOfChannels * parameters.width;
    
    return [imageBytes copy];
}

NSData *NSDataRGB888FromGrey8(NSData *data, SYSaneScanParameters *parameters)
{
    NSMutableData *imageBytes = [NSMutableData dataWithCapacity:parameters.width*parameters.height*3];
    
    u_int8_t *bufferIn  = malloc(1);
    u_int8_t *bufferOut = malloc(3);
    
    for (NSUInteger i = 0; i < data.length; ++i)
    {
        [data getBytes:bufferIn range:NSMakeRange(i, 1)];
        
        bufferOut[0] = bufferIn[0];
        bufferOut[1] = bufferIn[0];
        bufferOut[2] = bufferIn[0];
        [imageBytes appendBytes:bufferOut length:3];
    }
    
    parameters.depth = 8;
    parameters.currentlyAcquiredChannel = SANE_FRAME_RGB;
    parameters.bytesPerLine = parameters.numberOfChannels * parameters.width;
    
    return [imageBytes copy];
}
