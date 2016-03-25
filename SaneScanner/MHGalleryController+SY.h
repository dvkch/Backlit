//
//  MHGalleryController+SY.h
//  SaneScanner
//
//  Created by Stan Chevallier on 17/03/2016.
//  Copyright © 2016 Syan. All rights reserved.
//

#import <MHGalleryController.h>

@interface MHGalleryController (SY) <MHGalleryDelegate>

@property (nonatomic, assign) CGSize thumbSize;
@property (nonatomic, assign) CGFloat thumbsMargin;
@property (nonatomic, assign) BOOL hideDoneButton;
@property (nonatomic, strong) UIColor *titleGradientTintColor;

+ (void)sy_fix;

@end
