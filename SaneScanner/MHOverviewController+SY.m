//
//  MHOverviewController+SY.m
//  SaneScanner
//
//  Created by Stan Chevallier on 21/03/2016.
//  Copyright © 2016 Syan. All rights reserved.
//

#import "MHOverviewController+SY.h"
#import "SYGalleryController.h"
#import <NSObject+SYKit.h>
#import "SYAppDelegate.h"

@implementation MHOverviewController (SY)

+ (void)sy_fix
{
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        [self sy_swizzleSelector:@selector(viewWillTransitionToSize:withTransitionCoordinator:)
                    withSelector:@selector(sy_viewWillTransitionToSize:withTransitionCoordinator:)];
        [self sy_swizzleSelector:@selector(viewWillAppear:)
                    withSelector:@selector(sy_viewWillAppear:)];
    });
}

- (void)sy_viewWillAppear:(BOOL)animated
{
    [self sy_viewWillAppear:animated];
    [self updateCellSizesToWidth:self.view.bounds.size.width yo:@"will appear"];
}

- (void)sy_viewWillTransitionToSize:(CGSize)size
          withTransitionCoordinator:(id<UIViewControllerTransitionCoordinator>)coordinator
{
    [self sy_viewWillTransitionToSize:size withTransitionCoordinator:coordinator];
    [coordinator animateAlongsideTransition:^(id<UIViewControllerTransitionCoordinatorContext>  _Nonnull context) {
        [self updateCellSizesToWidth:size.width yo:@"will size"];
    } completion:nil];
}

- (void)updateCellSizesToWidth:(CGFloat)width yo:(NSString *)yo
{
    SYGalleryController *galleryController = (SYGalleryController *)self.navigationController;
    
    CGFloat maxSize = floor(320/3);
    NSUInteger numberOfItemsPerRow = floor(width / maxSize);
    CGFloat availableWidth = width - (numberOfItemsPerRow - 1) * galleryController.thumbsMargin;
    CGFloat cellSize = floor(availableWidth / numberOfItemsPerRow);
    
    [galleryController setThumbSize:CGSizeMake(cellSize, cellSize)];
    
    if ([[SYAppDelegate obtain] valueForKey:@"galleryViewController"] == self.navigationController)
        NSLog(@"%.02lf -> %.lf (%@)", width, cellSize, yo);
}

@end
