//
//  MHGalleryController+SY.m
//  SaneScanner
//
//  Created by Stan Chevallier on 17/03/2016.
//  Copyright © 2016 Syan. All rights reserved.
//

#import "MHGalleryController+SY.h"
#import "MHGalleryImageViewerViewController+SY.h"
#import "MHOverviewController+SY.h"
#import <NSObject+SYKit.h>
#import <UIImage+SYKit.h>
#import <objc/runtime.h>
#import "DLAVAlertView+SY.h"
#import "SYGalleryManager.h"
#import "UIColor+SY.h"

@interface MHGalleryController (SY_Private)
@property (nonatomic, strong) UIBarButtonItem *doneButtonOverViewVC;
@property (nonatomic, strong) UIBarButtonItem *doneButtonImageViewerVC;
@end

@implementation MHGalleryController (SY)

@dynamic hideDoneButton;
@dynamic thumbSize;
@dynamic titleGradientTintColor;

+ (void)sy_fix
{
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        [self sy_swizzleSelector:@selector(initWithPresentationStyle:)
                    withSelector:@selector(sy_initWithPresentationStyle:)];
        [self sy_swizzleSelector:@selector(reloadData)
                    withSelector:@selector(sy_reloadData)];
        [self sy_swizzleSelector:@selector(setGalleryItems:)
                    withSelector:@selector(sy_setGalleryItems:)];
    });
    
    [MHGalleryImageViewerViewController sy_fix];
    [MHOverviewController sy_fix];
}

- (id)sy_initWithPresentationStyle:(MHGalleryViewMode)presentationStyle;
{
    MHGalleryController *controller = [self sy_initWithPresentationStyle:presentationStyle];
    if (controller)
    {
        [self.transitionCustomization setDismissWithScrollGestureOnFirstAndLastImage:NO];
        
        [self.UICustomization setMHGalleryBackgroundColor:[UIColor groupTableViewBackgroundColor]
                                              forViewMode:MHGalleryViewModeImageViewerNavigationBarShown];
        [self.UICustomization setShowMHShareViewInsteadOfActivityViewController:NO];
        [self.UICustomization setShowOverView:YES];
        [self.UICustomization setUseCustomBackButtonImageOnImageViewer:YES];
        [self.UICustomization setBackButtonState:MHBackButtonStateWithBackArrow];
        [self.UICustomization setCustomBarButtonItem:
         [[UIBarButtonItem alloc] initWithBarButtonSystemItem:UIBarButtonSystemItemTrash
                                                       target:self action:@selector(deleteImage:)]];
        
        [self setTitleGradientTintColor:[UIColor colorWithWhite:0.9 alpha:1.]];
        [self setTitleGradientTintColor:[UIColor vividBlueColor]];
        [self setThumbSize:CGSizeMake(150, 150)];
        [self setThumbsMargin:4.];
        
        [self.overViewViewController view];
        [self.overViewViewController setTitle:@"All scans"];
        [self.overViewViewController updateCellSizesToWidth];
    }
    return controller;
}

- (void)sy_reloadData
{
    // views haven't been loaded yet
    if (!self.imageViewerViewController.toolbar)
        return;
    
    [self sy_reloadData];
}

- (void)sy_setGalleryItems:(NSArray<MHGalleryItem *> *)galleryItems
{
    [self setDataSource:self];
    
    if (!galleryItems.count)
        [self sy_setGalleryItems:@[[MHGalleryItem itemWithImage:[UIImage sy_imageWithColor:[UIColor clearColor]]]]];
    else
        [self sy_setGalleryItems:galleryItems];
    
    [self reloadData];
}

- (CGSize)thumbSize {
    return [(NSValue *)objc_getAssociatedObject(self, @selector(thumbSize)) CGSizeValue];
}

- (void)setThumbSize:(CGSize)thumbSize {
    objc_setAssociatedObject(self, @selector(thumbSize),
                             [NSValue valueWithCGSize:thumbSize],
                             OBJC_ASSOCIATION_RETAIN_NONATOMIC);
    [self.UICustomization.overViewCollectionViewLayoutPortrait  setItemSize:thumbSize];
    [self.UICustomization.overViewCollectionViewLayoutLandscape setItemSize:thumbSize];
    [self.overViewViewController.collectionView reloadData];
    [self.overViewViewController willRotateToInterfaceOrientation:self.overViewViewController.interfaceOrientation
                                                         duration:0];
}

- (CGFloat)thumbsMargin
{
    return [objc_getAssociatedObject(self, @selector(thumbsMargin)) doubleValue];
}

- (void)setThumbsMargin:(CGFloat)thumbsMargin
{
    objc_setAssociatedObject(self, @selector(thumbsMargin),
                             @(thumbsMargin),
                             OBJC_ASSOCIATION_RETAIN_NONATOMIC);
    [self.UICustomization.overViewCollectionViewLayoutPortrait  setMinimumInteritemSpacing:thumbsMargin];
    [self.UICustomization.overViewCollectionViewLayoutLandscape setMinimumInteritemSpacing:thumbsMargin];
    [self.overViewViewController.collectionView reloadData];
    [self.overViewViewController willRotateToInterfaceOrientation:self.overViewViewController.interfaceOrientation
                                                         duration:0];
}

- (BOOL)hideDoneButton {
    return [objc_getAssociatedObject(self, @selector(hideDoneButton)) boolValue];
}

- (void)setHideDoneButton:(BOOL)hideDoneButton {
    objc_setAssociatedObject(self, @selector(hideDoneButton), @(hideDoneButton), OBJC_ASSOCIATION_RETAIN_NONATOMIC);

    [self.overViewViewController    view];
    [self.imageViewerViewController view];
    
    self.doneButtonOverViewVC =
    (self.doneButtonOverViewVC ?: self.overViewViewController.navigationItem.rightBarButtonItem);
    
    self.doneButtonImageViewerVC =
    (self.doneButtonImageViewerVC ?: self.imageViewerViewController.navigationItem.rightBarButtonItem);
    
    [self.overViewViewController.navigationItem
     setRightBarButtonItem:(hideDoneButton ? nil : self.doneButtonOverViewVC)];
    
    [self.imageViewerViewController.navigationItem
     setRightBarButtonItem:(hideDoneButton ? nil : self.doneButtonImageViewerVC)];
}

- (UIBarButtonItem *)doneButtonOverViewVC {
    return objc_getAssociatedObject(self, @selector(doneButtonOverViewVC));
}

- (void)setDoneButtonOverViewVC:(UIBarButtonItem *)doneButtonOverViewVC {
    objc_setAssociatedObject(self, @selector(doneButtonOverViewVC),
                             doneButtonOverViewVC,
                             OBJC_ASSOCIATION_RETAIN_NONATOMIC);
}

- (UIBarButtonItem *)doneButtonImageViewerVC {
    return objc_getAssociatedObject(self, @selector(doneButtonImageViewerVC));
}

- (void)setDoneButtonImageViewerVC:(UIBarButtonItem *)doneButtonImageViewerVC {
    objc_setAssociatedObject(self, @selector(doneButtonImageViewerVC),
                             doneButtonImageViewerVC,
                             OBJC_ASSOCIATION_RETAIN_NONATOMIC);
}

- (UIColor *)titleGradientTintColor {
    return objc_getAssociatedObject(self, @selector(titleGradientTintColor));
}

- (void)setTitleGradientTintColor:(UIColor *)titleGradientTintColor {
    
    UIColor *tintColor = titleGradientTintColor ?: [UIColor blackColor];
    
    objc_setAssociatedObject(self, @selector(titleGradientTintColor),
                             tintColor,
                             OBJC_ASSOCIATION_RETAIN_NONATOMIC);
    
    [self.UICustomization setMHGradients:@[[tintColor colorWithAlphaComponent:0.85],
                                           [tintColor colorWithAlphaComponent:0.70],
                                           [tintColor colorWithAlphaComponent:0.0]]
                            forDirection:MHGradientDirectionTopToBottom];
    
    [self.UICustomization setMHGradients:@[[tintColor colorWithAlphaComponent:0.0],
                                           [tintColor colorWithAlphaComponent:0.70],
                                           [tintColor colorWithAlphaComponent:0.85]]
                            forDirection:MHGradientDirectionBottomToTop];
}

- (void)deleteImage:(id)sender
{
    if (self.imageViewerViewController.userScrolls)
        return;
    
    NSInteger index = self.imageViewerViewController.pageIndex;
    MHGalleryItem *item = [self.dataSource itemForIndex:index];
    
    [[[DLAVAlertView alloc] initWithTitle:@"Confirmation"
                                  message:@"Are you sure you want to delete this picture?"
                                 delegate:nil
                        cancelButtonTitle:@"Cancel"
                        otherButtonTitles:@"Delete", nil]
     showWithCompletion:^(DLAVAlertView *alertView, NSInteger buttonIndex) {
         if (alertView.cancelButtonIndex == buttonIndex)
             return;
         
         if ([self.dataSource numberOfItemsInGallery:self] == 1)
         {
             [self dismissViewControllerAnimated:YES
                                dismissImageView:self.imageViewerViewController.dismissFromImageView
                                      completion:nil];
         }
         
         [[SYGalleryManager shared] deleteItem:item];
     }];
}

@end
