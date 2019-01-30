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

+ (UIImage *)sy_imageFromRGBData:(NSData *)data
                       orFileURL:(NSURL *)fileURL
                  saneParameters:(SYSaneScanParameters *)parameters
                           error:(NSError **)error;

+ (UIImage *)sy_imageFromIncompleteRGBData:(NSData *)data
                                 orFileURL:(NSURL *)fileURL
                            saneParameters:(SYSaneScanParameters *)parameters
                                     error:(NSError **)error;

@end
