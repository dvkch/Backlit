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
    
    [SVProgressHUD show];
    
    for (NSIndexPath *indexPath in self.collectionView.indexPathsForSelectedItems)
        [[SYGalleryManager shared] deleteItem:[self itemForIndex:indexPath.row]];
    
    [SVProgressHUD dismiss];
}

@end
