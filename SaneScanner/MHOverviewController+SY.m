//
//  MHOverviewController+SY.m
//  SaneScanner
//
//  Created by Stan Chevallier on 21/03/2016.
//  Copyright © 2016 Syan. All rights reserved.
//

#import "MHOverviewController+SY.h"
#import "MHGalleryController+SY.h"
#import <NSObject+SYKit.h>

@implementation MHOverviewController (SY)

+ (void)sy_fix
{
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        [self sy_swizzleSelector:@selector(viewWillTransitionToSize:withTransitionCoordinator:)
                    withSelector:@selector(sy_viewWillTransitionToSize:withTransitionCoordinator:)];
    });
}

- (void)sy_viewWillTransitionToSize:(CGSize)size
          withTransitionCoordinator:(id<UIViewControllerTransitionCoordinator>)coordinator
{
    [self sy_viewWillTransitionToSize:size withTransitionCoordinator:coordinator];
    [coordinator animateAlongsideTransition:^(id<UIViewControllerTransitionCoordinatorContext>  _Nonnull context) {
        [self updateCellSizesToWidth:size.width];
    } completion:nil];
}

- (void)updateCellSizesToWidth
{
    [self updateCellSizesToWidth:self.view.bounds.size.width];
}

- (void)updateCellSizesToWidth:(CGFloat)width
{
    MHGalleryController *galleryController = (MHGalleryController *)self.navigationController;
    
    CGFloat maxSize = floor(320/3);
    NSUInteger numberOfItemsPerRow = floor(width / maxSize);
    CGFloat availableWidth = width - (numberOfItemsPerRow - 1) * galleryController.thumbsMargin;
    CGFloat cellSize = floor(availableWidth / numberOfItemsPerRow);
    
    [galleryController setThumbSize:CGSizeMake(cellSize, cellSize)];
}

@end
