//
//  SYGalleryController.m
//  SaneScanner
//
//  Created by Stan Chevallier on 17/03/2016.
//  Copyright © 2016 Syan. All rights reserved.
//

#import "SYGalleryController.h"
#import <MHVideoPhotoGallery/MHVideoPhotoGallery-umbrella.h>
#import <NSObject+SYKit.h>
#import <SYKit-Swift.h>
#import "DLAVAlertView+SY.h"
#import "UIColor+SY.h"
#import "UIViewController+SYKit.h"
#import "SYOverviewController.h"
#import "UIActivityViewController+SY.h"
#import "SaneScanner-Swift.h"

@interface SYGalleryController () <GalleryManagerDelegate>
@end

@implementation SYGalleryController

- (instancetype)initWithPresentationStyle:(MHGalleryViewMode)presentationStyle UICustomization:(MHUICustomization *)UICustomization
{
    self = [super initWithPresentationStyle:presentationStyle UICustomization:UICustomization];
    if (self)
    {
        self.overViewViewController = SYOverviewController.new;
        
        if (presentationStyle != MHGalleryViewModeOverView) {
            self.viewControllers = @[self.overViewViewController,self.imageViewerViewController];
        }else{
            self.viewControllers = @[self.overViewViewController];
        }
        
        [self.transitionCustomization setDismissWithScrollGestureOnFirstAndLastImage:NO];
        [self setGalleryDelegate:self];
        
        [GalleryManager.shared addDelegate:self];
    }
    return self;
}

- (void)deleteImage:(id)sender
{
    if (self.imageViewerViewController.userScrolls)
        return;
    
    NSInteger index = self.imageViewerViewController.pageIndex;
    MHGalleryItem *item = [self.dataSource itemForIndex:index];
    
    [[[DLAVAlertView alloc] initWithTitle:$("DIALOG TITLE DELETE SCAN")
                                  message:$("DIALOG MESSAGE DELETE SCAN")
                                 delegate:nil
                        cancelButtonTitle:$("ACTION CANCEL")
                        otherButtonTitles:$("ACTION DELETE"), nil]
     showWithCompletion:^(DLAVAlertView *alertView, NSInteger buttonIndex)
    {
        if (alertView.cancelButtonIndex == buttonIndex)
            return;
        
        if ([self.dataSource numberOfItemsInGallery:self] == 1)
        {
            [self dismissViewControllerAnimated:YES
                               dismissImageView:self.imageViewerViewController.dismissFromImageView
                                     completion:nil];
        }
        
        [GalleryManager.shared deleteItem:item];
    }];
}

- (void)shareImage:(id)sender
{
    if (self.imageViewerViewController.userScrolls)
        return;
    
    NSInteger index = self.imageViewerViewController.pageIndex;
    MHGalleryItem *item = [self.dataSource itemForIndex:index];
    
    [UIActivityViewController sy_showForUrls:@[item.URL]
                           fromBarButtonItem:sender
                                presentingVC:self
                                  completion:nil];
}

- (NSArray<MHBarButtonItem *>*)customizeableToolBarItems:(NSArray<MHBarButtonItem *>*)toolBarItems
                                         forGalleryItem:(MHGalleryItem*)galleryItem
{
    if (galleryItem.galleryType == MHGalleryTypeVideo)
        return toolBarItems;
    
    MHBarButtonItem *deleteButton =
    [[MHBarButtonItem alloc] initWithBarButtonSystemItem:UIBarButtonSystemItemTrash
                                                  target:self action:@selector(deleteImage:)];
    [deleteButton setType:MHBarButtonItemTypeCustom];
    
    MHBarButtonItem *dateButton =
    [[MHBarButtonItem alloc] initWithTitle:[GalleryManager.shared dateStringFor:galleryItem]
                                     style:UIBarButtonItemStylePlain target:nil action:nil];
    [dateButton setType:MHBarButtonItemTypeCustom];
    
    MHBarButtonItem *shareButton =
    [[MHBarButtonItem alloc] initWithBarButtonSystemItem:UIBarButtonSystemItemAction
                                                  target:self action:@selector(shareImage:)];
    [shareButton setType:MHBarButtonItemTypeShare];
    
    MHBarButtonItem *flexibleSpace =
    [[MHBarButtonItem alloc] initWithBarButtonSystemItem:UIBarButtonSystemItemFlexibleSpace
                                                  target:nil action:nil];
    [flexibleSpace setType:MHBarButtonItemTypeFlexible];
    
    return @[deleteButton, flexibleSpace, dateButton, flexibleSpace, shareButton];
}

#pragma mark - SYGalleryManager

- (void)galleryManager:(GalleryManager *)manager
             didUpdate:(NSArray<MHGalleryItem *> *)items
              newItems:(NSArray<MHGalleryItem *> *)newItems
          removedItems:(NSArray<MHGalleryItem *> *)removedItems
{
    if (items.count == 0 && [self sy_isModal])
    {
        [self dismissViewControllerAnimated:YES completion:nil];
        return;
    }
    
    if (![self isShowingOverview])
    {
        NSUInteger currentIndex = self.imageViewerViewController.pageIndex;
        NSUInteger index = NSNotFound;
        BOOL adding = YES;
        
        if (newItems.firstObject)
            index = [items indexOfObject:newItems.firstObject];
        
        if (removedItems.firstObject)
        {
            index = [self.galleryItems indexOfObject:removedItems.firstObject];
            adding = NO;
        }
        
        if (index == currentIndex)
        {
            CATransition *transition = [CATransition animation];
            transition.duration = 0.3f;
            transition.timingFunction = [CAMediaTimingFunction functionWithName:kCAMediaTimingFunctionEaseInEaseOut];
            transition.type = kCATransitionPush;
            transition.subtype = kCATransitionFromRight;
            
            if (adding || index == self.galleryItems.count - 1)
                transition.subtype = kCATransitionFromLeft;
            
            [self.imageViewerViewController.pageViewController.view.layer addAnimation:transition forKey:kCATransition];
        }
    }
    
    [self setGalleryItems:items];
}

@end

