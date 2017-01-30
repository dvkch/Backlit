//
//  SYOverviewController.m
//  SaneScanner
//
//  Created by Stanislas Chevallier on 27/01/2017.
//  Copyright © 2017 Syan. All rights reserved.
//

#import "SYOverviewController.h"
#import <SVProgressHUD.h>
#import "SYGalleryManager.h"
#import "DLAVAlertView+SY.h"

@interface SYOverviewController ()

@end

@implementation SYOverviewController

- (void)viewDidLoad
{
    [super viewDidLoad];
    
    // add trash icon
    NSMutableArray <UIBarButtonItem *> *toolbarItems = [self.toolbarItems mutableCopy];
    UIBarButtonItem *trashBarButton = [[UIBarButtonItem alloc] initWithBarButtonSystemItem:UIBarButtonSystemItemTrash target:self action:@selector(trashPressed)];
    [toolbarItems insertObject:trashBarButton atIndex:0];
    self.toolbarItems = toolbarItems;
}

- (void)trashPressed
{
    if (!self.collectionView.indexPathsForSelectedItems.count)
        return;
    
    NSString *title   = $("DIALOG TITLE DELETE SCAN");
    NSString *message = $("DIALOG MESSAGE DELETE SCAN");
    
    if (self.collectionView.indexPathsForSelectedItems.count > 1)
    {
        title   = $("DIALOG TITLE DELETE SCANS");
        message = [NSString stringWithFormat:$("DIALOG MESSAGE DELETE SCANS %d"),
                   (int)self.collectionView.indexPathsForSelectedItems.count];
    }
    
    [[[DLAVAlertView alloc] initWithTitle:title
                                  message:message
                                 delegate:nil
                        cancelButtonTitle:$("ACTION CANCEL")
                        otherButtonTitles:$("ACTION DELETE"), nil]
     showWithCompletion:^(DLAVAlertView *alertView, NSInteger buttonIndex)
    {
        if (alertView.cancelButtonIndex == buttonIndex)
            return;
        
        [SVProgressHUD show];
        
        for (NSIndexPath *indexPath in self.collectionView.indexPathsForSelectedItems)
            [[SYGalleryManager shared] deleteItem:[self itemForIndex:indexPath.row]];
        
        [SVProgressHUD dismiss];
        
        [self setEditing:NO];
    }];
}

@end
