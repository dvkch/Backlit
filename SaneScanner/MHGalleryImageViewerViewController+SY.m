//
//  MHGalleryImageViewerViewController+SY.m
//  SaneScanner
//
//  Created by Stan Chevallier on 18/03/2016.
//  Copyright © 2016 Syan. All rights reserved.
//

#import "MHGalleryImageViewerViewController+SY.h"
#import <NSObject+SYKit.h>

@implementation MHGalleryImageViewerViewController (SY)

+ (void)sy_fix
{
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        [self sy_swizzleSelector:@selector(updateToolBarForItem:)
                    withSelector:@selector(sy_updateToolBarForItem:)];
        [self sy_swizzleSelector:@selector(reloadData)
                    withSelector:@selector(sy_reloadData)];
    });
}

- (void)sy_updateToolBarForItem:(MHGalleryItem *)item
{
    [self sy_updateToolBarForItem:item];
    
    if (self.UICustomization.customBarButtonItem)
    {
        NSMutableArray <UIBarButtonItem *> *items = [self.toolbar.items mutableCopy];
        [items removeLastObject];
        [items addObject:self.UICustomization.customBarButtonItem];
        self.toolbar.items = [items copy];
    }
}

- (void)sy_reloadData
{
    [self sy_reloadData];
    __weak MHGalleryImageViewerViewController *wSelf = self;
    
    /*
    NSUInteger numberOfItems = [self.galleryViewController.dataSource numberOfItemsInGallery:self.galleryViewController];
    if (numberOfItems && self.pageIndex >= numberOfItems)
    {
        MHGalleryItem *item = [self.galleryViewController.dataSource itemForIndex:self.pageIndex-1];
        
        MHImageViewController *imageViewController = [MHImageViewController imageViewControllerForMHMediaItem:item viewController:self];
        imageViewController.pageIndex = self.pageIndex;
        NSArray <UIViewController *> *previousViewControllers = self.pageViewController.viewControllers;
        [self.pageViewController setViewControllers:@[imageViewController]
                                          direction:UIPageViewControllerNavigationDirectionForward
                                           animated:YES
                                         completion:^(BOOL finished)
        {
            [wSelf.pageViewController.delegate pageViewController:wSelf.pageViewController
                                               didFinishAnimating:YES
                                          previousViewControllers:previousViewControllers
                                              transitionCompleted:YES];
        }];
     
        //[self updateTitleLabelForIndex:self.pageIndex];
        //[self updateDescriptionLabelForIndex:self.pageIndex];
        //[self updateToolBarForItem:item];
        //[self updateTitleForIndex:self.pageIndex];
    }
     */
}

@end
