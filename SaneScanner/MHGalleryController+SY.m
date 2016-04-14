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
#import <MHBarButtonItem.h>
#import <NSObject+SYKit.h>
#import <UIImage+SYKit.h>
#import <objc/runtime.h>
#import "DLAVAlertView+SY.h"
#import "SYGalleryManager.h"
#import "UIColor+SY.h"

NSString *NSStringFromMHBarButtonItemType(MHBarButtonItemType type);

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
        
        [self setTitleGradientTintColor:[UIColor groupTableViewBackgroundColor]];
        [self setThumbSize:CGSizeMake(150, 150)];
        [self setThumbsMargin:4.];
        
        [self.overViewViewController view];
        [self.overViewViewController setTitle:@"All scans"];
        [self.overViewViewController updateCellSizesToWidth];
        [self setGalleryDelegate:self];
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
    self.overViewViewController.collectionView.collectionViewLayout = [self.overViewViewController layoutForOrientation:0];
    [self.overViewViewController.collectionView.collectionViewLayout invalidateLayout];
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
    self.overViewViewController.collectionView.collectionViewLayout = [self.overViewViewController layoutForOrientation:0];
    [self.overViewViewController.collectionView.collectionViewLayout invalidateLayout];
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
    
    //CGFloat alphas[3] = {0.85, 0.70, 0.0};
    CGFloat alphas[3] = {1.0, 0.85, 0.0};
    
    [self.UICustomization setMHGradients:@[[tintColor colorWithAlphaComponent:alphas[0]],
                                           [tintColor colorWithAlphaComponent:alphas[1]],
                                           [tintColor colorWithAlphaComponent:alphas[2]]]
                            forDirection:MHGradientDirectionTopToBottom];
    
    [self.UICustomization setMHGradients:@[[tintColor colorWithAlphaComponent:alphas[2]],
                                           [tintColor colorWithAlphaComponent:alphas[1]],
                                           [tintColor colorWithAlphaComponent:alphas[0]]]
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

-(NSArray<MHBarButtonItem *>*)customizeableToolBarItems:(NSArray<MHBarButtonItem *>*)toolBarItems
                                         forGalleryItem:(MHGalleryItem*)galleryItem
{
    if (galleryItem.galleryType == MHGalleryTypeVideo)
        return toolBarItems;
    
    MHBarButtonItem *deleteButton =
    [[MHBarButtonItem alloc] initWithBarButtonSystemItem:UIBarButtonSystemItemTrash
                                                  target:self action:@selector(deleteImage:)];
    [deleteButton setType:MHBarButtonItemTypeCustom];
    
    MHBarButtonItem *dateButton =
    [[MHBarButtonItem alloc] initWithTitle:[[SYGalleryManager shared] dateStringForItem:galleryItem]
                                     style:UIBarButtonItemStylePlain target:nil action:nil];
    [dateButton setType:MHBarButtonItemTypeCustom];
    
    MHBarButtonItem *shareButton;
    MHBarButtonItem *flexibleSpace;
    for (MHBarButtonItem *item in toolBarItems)
    {
        if (item.type == MHBarButtonItemTypeShare)
            shareButton = item;
        if (item.type == MHBarButtonItemTypeFlexible)
            flexibleSpace = item;
    }
    
    return @[shareButton, flexibleSpace, dateButton, flexibleSpace, deleteButton];
}

@end

NSString *NSStringFromMHBarButtonItemType(MHBarButtonItemType type)
{
    switch (type) {
        case MHBarButtonItemTypeLeft:       return @"MHBarButtonItemTypeLeft";
        case MHBarButtonItemTypeRigth:      return @"MHBarButtonItemTypeRigth";
        case MHBarButtonItemTypePlayPause:  return @"MHBarButtonItemTypePlayPause";
        case MHBarButtonItemTypeShare:      return @"MHBarButtonItemTypeShare";
        case MHBarButtonItemTypeFixed:      return @"MHBarButtonItemTypeFixed";
        case MHBarButtonItemTypeFlexible:   return @"MHBarButtonItemTypeFlexible";
        case MHBarButtonItemTypeCustom:     return @"MHBarButtonItemTypeCustom";
    }
    return @"unknown";
}

