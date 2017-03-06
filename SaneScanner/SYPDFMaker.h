//
//  SYPDFMaker.h
//  SaneScanner
//
//  Created by Stanislas Chevallier on 06/03/2017.
//  Copyright © 2017 Syan. All rights reserved.
//

#import <Foundation/Foundation.h>

@interface SYPDFMaker : NSObject

+ (BOOL)createPDFAtURL:(NSURL *)url
      fromImagesAtURLs:(NSArray <NSURL *> *)imageURLs
           aspectRatio:(double)aspectRatio
           jpegQuality:(double)jpegQuality
         fixedPageSize:(BOOL)fixedPageSize;

@end
