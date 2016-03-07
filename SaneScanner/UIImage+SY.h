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

@end
