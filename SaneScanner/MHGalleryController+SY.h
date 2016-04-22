//
//  MHGalleryController+SY.h
//  SaneScanner
//
//  Created by Stan Chevallier on 17/03/2016.
//  Copyright © 2016 Syan. All rights reserved.
//

#import <MHGalleryController.h>
#import "SYGalleryManager.h"

@interface MHGalleryController (SY) <MHGalleryDelegate, SYGalleryManagerDelegate>

@property (nonatomic, assign) CGSize thumbSize;
@property (nonatomic, assign) CGFloat thumbsMargin;

+ (void)sy_fix;

@end
