//
//  SYGalleryController.m
//  SaneScanner
//
//  Created by Stan Chevallier on 17/03/2016.
//  Copyright © 2016 Syan. All rights reserved.
//

#import "SYGalleryController.h"
#import <MHBarButtonItem.h>
#import <NSObject+SYKit.h>
#import <UIImage+SYKit.h>
#import "DLAVAlertView+SY.h"
#import "SYGalleryManager.h"
#import "UIColor+SY.h"
#import "UIViewController+SYKit.h"

@implementation SYGalleryController

- (instancetype)initWithPresentationStyle:(MHGalleryViewMode)presentationStyle UICustomization:(MHUICustomization *)UICustomization
{
    self = [super initWithPresentationStyle:presentationStyle UICustomization:UICustomization];
    if (self)
    {
        [self.transitionCustomization setDismissWithScrollGestureOnFirstAndLastImage:NO];
        [self setGalleryDelegate:self];
        
        [[SYGalleryManager shared] addDelegate:self];
    }
    return self;
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

#pragma mark - SYGalleryManager

- (void)gallerymanager:(SYGalleryManager *)gallerymanager
 didUpdateGalleryItems:(NSArray<MHGalleryItem *> *)items
               newItem:(MHGalleryItem *)newItem
           removedItem:(MHGalleryItem *)removedItem
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
        
        if (newItem)
            index = [items indexOfObject:newItem];
        
        if (removedItem)
        {
            index = [self.galleryItems indexOfObject:removedItem];
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

