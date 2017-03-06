//
//  SYPDFMaker.m
//  SaneScanner
//
//  Created by Stanislas Chevallier on 06/03/2017.
//  Copyright © 2017 Syan. All rights reserved.
//

#import "SYPDFMaker.h"
#import <CoreGraphics/CoreGraphics.h>
#import <UIKit/UIKit.h>
#import <UIImage+SYKit.h>

@implementation SYPDFMaker

+ (BOOL)createPDFAtURL:(NSURL *)url
      fromImagesAtURLs:(NSArray <NSURL *> *)imageURLs
           aspectRatio:(double)aspectRatio
           jpegQuality:(double)jpegQuality
         fixedPageSize:(BOOL)fixedPageSize
{
    CGSize greatestSize = CGSizeZero;
    CGFloat minRatio = 1.;
    CGFloat maxRatio = 1.;
    for (NSURL *imageURL in imageURLs)
    {
        CGSize size = [UIImage sy_sizeOfImageAtURL:imageURL];
        if (CGSizeEqualToSize(CGSizeZero, size))
            continue;
        
        greatestSize.width  = MAX(greatestSize.width,  size.width );
        greatestSize.height = MAX(greatestSize.height, size.height);
        minRatio = MIN(minRatio, size.width / size.height);
        maxRatio = MAX(maxRatio, size.width / size.height);
    }
    
    if (CGSizeEqualToSize(CGSizeZero, greatestSize))
        return NO;
    
    CGSize pdfSize = greatestSize;
    if (aspectRatio != 0.)
    {
        // We create a size that has the wanted ratio, and the same area (w*h) than greatestSize. This
        // should prevent loosing too much data, or upscaling images too much.
        pdfSize.width  = sqrt(aspectRatio * greatestSize.width * greatestSize.height);
        pdfSize.height = pdfSize.width / aspectRatio;
    }
    
    UIGraphicsBeginPDFContextToFile(url.path, (CGRect){CGPointZero, pdfSize}, nil);
    
    for (NSURL *imageURL in imageURLs)
    {
        CGDataProviderRef dataProvider = CGDataProviderCreateWithURL((__bridge CFURLRef)imageURL);
        CGImageRef cgImage = NULL;
        BOOL sourceIsPNG = NO;
        
        if (!cgImage)
            cgImage = CGImageCreateWithJPEGDataProvider(dataProvider, NULL, true, kCGRenderingIntentDefault);
        
        if (!cgImage)
        {
            cgImage = CGImageCreateWithPNGDataProvider(dataProvider, NULL, true, kCGRenderingIntentDefault);
            sourceIsPNG = YES;
        }
        
        if (!cgImage)
        {
            CGDataProviderRelease(dataProvider);
            continue;
        }
        
        // encode PNG data to JPEG if asked to
        if (jpegQuality > 0 && jpegQuality < 1. && sourceIsPNG)
        {
            UIImage *image = [UIImage imageWithCGImage:cgImage];
            NSData *imageData = UIImageJPEGRepresentation(image, jpegQuality);
            
            CGImageRelease(cgImage);
            CGDataProviderRelease(dataProvider);
            
            dataProvider = CGDataProviderCreateWithCFData((__bridge CFDataRef)imageData);
            cgImage = CGImageCreateWithJPEGDataProvider(dataProvider, NULL, true, kCGRenderingIntentDefault);
        }
        
        // determines size that fits inside the PDF bounds, using as much space as possible, while
        // keeping the original image ratio
        CGSize sizeThatFits;
        {
            CGSize size = [UIImage sy_sizeOfImageAtURL:imageURL];
            
            if (size.width / size.height > pdfSize.width / pdfSize.height)
                sizeThatFits = CGSizeMake(pdfSize.width, size.height * pdfSize.width / size.width);
            else
                sizeThatFits = CGSizeMake(size.width * pdfSize.height / size.height, pdfSize.height);
        }
        
        if (fixedPageSize)
        {
            // add image centered inside the page, page has the same size as the document
            UIGraphicsBeginPDFPage();
            
            CGRect rect = CGRectZero;
            rect.origin.x = (pdfSize.width  - sizeThatFits.width ) / 2.;
            rect.origin.y = (pdfSize.height - sizeThatFits.height) / 2.;
            rect.size = sizeThatFits;
            
            [[UIImage imageWithCGImage:cgImage] drawInRect:rect];
        }
        else
        {
            // add image in a page of the same size as the image
            CGRect rect = (CGRect){CGPointZero, sizeThatFits};
            UIGraphicsBeginPDFPageWithInfo(rect, nil);
            [[UIImage imageWithCGImage:cgImage] drawInRect:rect];
        }
        
        CGDataProviderRelease(dataProvider);
        CGImageRelease(cgImage);
    }
    
    UIGraphicsEndPDFContext();
    return YES;
}

@end
