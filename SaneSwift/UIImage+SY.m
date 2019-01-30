//
//  UIImage+SY.m
//  SaneScanner
//
//  Created by Stan Chevallier on 04/03/2016.
//  Copyright © 2016 Syan. All rights reserved.
//

#import "UIImage+SY.h"
// TODO: #import <UIImage+SYKit.h>
#import "SYSaneScanParameters.h"
#import <ImageIO/ImageIO.h>
#import "NSError+SY.h"

@implementation UIImage (SY)

+ (UIImage *)sy_imageFromRGBData:(NSData *)imageBytes
                       orFileURL:(NSURL *)fileURL
                  saneParameters:(SYSaneScanParameters *)parameters
                           error:(NSError **)error
{
    if (parameters.currentlyAcquiredChannel != SANE_FRAME_RGB &&
        parameters.currentlyAcquiredChannel != SANE_FRAME_GRAY)
    {
        if (error)
            *error = [NSError ss_errorWithCode:SSErrorCode_UnsupportedChannels];
        return nil;
    }
    
    if (!imageBytes && ![[NSFileManager defaultManager] fileExistsAtPath:fileURL.path]) {
        if (error) *error = [NSError ss_errorWithCode:SSErrorCode_NoImageData];
        return nil;
    }
    
    CGColorSpaceRef colorSpaceRef = NULL;
    
    if (parameters.currentlyAcquiredChannel == SANE_FRAME_RGB)
        colorSpaceRef = CGColorSpaceCreateDeviceRGB();
    
    if (parameters.currentlyAcquiredChannel == SANE_FRAME_GRAY)
        colorSpaceRef = CGColorSpaceCreateDeviceGray();
    
    if (!colorSpaceRef) {
        if (error) *error = [NSError ss_errorWithCode:SSErrorCode_CannotGenerateImage];
        return nil;
    }
    
    
    CGDataProviderRef provider = NULL;
    if (fileURL)
        provider = CGDataProviderCreateWithURL((CFURLRef)fileURL);
    else
        provider = CGDataProviderCreateWithData(NULL,
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
        if (error) *error = [NSError ss_errorWithCode:SSErrorCode_CannotGenerateImage];
        CGDataProviderRelease(provider);
        CGColorSpaceRelease(colorSpaceRef);
        return nil;
    }
    
    /*
    UIImage *img = [UIImage imageWithCGImage:sourceImageRef
                                         scale:1.
                                   orientation:UIImageOrientationUp];
    return img;
     */
    
    CGBitmapInfo destBitmapInfo = (kCGBitmapByteOrderDefault | kCGImageAlphaNoneSkipLast);
    int destNumberOfComponents = 4;
    
    if (parameters.currentlyAcquiredChannel == SANE_FRAME_GRAY)
    {
        destBitmapInfo = (kCGBitmapByteOrderDefault | kCGImageAlphaNone);
        destNumberOfComponents = 1;
    }
    
    CGContextRef context =
    CGBitmapContextCreate(NULL, // let iOS deal with allocating the memory
                          parameters.width,
                          parameters.height,
                          8,
                          parameters.width * destNumberOfComponents,
                          colorSpaceRef,
                          destBitmapInfo);
    
    if (!context) {
        if (error) *error = [NSError ss_errorWithCode:SSErrorCode_CannotGenerateImage];
        CGDataProviderRelease(provider);
        CGColorSpaceRelease(colorSpaceRef);
        CGImageRelease(sourceImageRef);
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
    
    return image;
}

+ (UIImage *)sy_imageFromIncompleteRGBData:(NSData *)data
                                 orFileURL:(NSURL *)fileURL
                            saneParameters:(SYSaneScanParameters *)parameters
                                     error:(NSError **)error
{
    SYSaneScanParameters *incompleteParams = [SYSaneScanParameters parametersForIncompleteDataOfLength:data.length
                                                                                    completeParameters:parameters];
    
    if (!incompleteParams.fileSize)
        return nil;
    
    UIImage *image = [self sy_imageFromRGBData:data
                                     orFileURL:fileURL
                                saneParameters:incompleteParams
                                         error:error];
    
    if (image)
    {
        CGRect fullRect = CGRectMake(0, 0, parameters.width, parameters.height);
        UIGraphicsBeginImageContextWithOptions(fullRect.size, YES, 1.0);
        [[UIColor whiteColor] setFill];
        [[UIBezierPath bezierPathWithRect:fullRect] fill];
        [image drawInRect:(CGRect){CGPointZero, image.size}];
        image = UIGraphicsGetImageFromCurrentImageContext();
        UIGraphicsEndImageContext();
    }
    
    return image;
}
/*
// TODO: work on big images
+ (void)sy_convertTempImage
{
    SYSaneScanParameters *params = [[SYSaneScanParameters alloc] init];
    params.acquiringLastChannel = YES;
    params.currentlyAcquiredChannel = SANE_FRAME_RGB;
    params.width = 10208;
    params.height = 14032;
    params.bytesPerLine = params.width * 3;
    params.depth = 8;
    
    
    NSString *sourceFileName = [[SYTools documentsPath] stringByAppendingPathComponent:$$("scan.tmp")];
    NSString *destinationFileName = [[SYTools documentsPath] stringByAppendingPathComponent:$$("scan.jpg")];
    
    NSDate *date = [NSDate date];
    
    UIImage *img = [self sy_imageFromRGBData:nil orFileURL:[NSURL fileURLWithPath:sourceFileName] saneParameters:params error:NULL];
    NSData *data = UIImageJPEGRepresentation(img, JPEG_COMP);
    [data writeToFile:destinationFileName atomically:YES];
    
    NSLog($$("-> %@ in %.03lfs"), img, [[NSDate date] timeIntervalSinceDate:date]);
}
*/
@end

