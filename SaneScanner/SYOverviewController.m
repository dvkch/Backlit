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
#import "SYTools.h"
#import "SYPDFMaker.h"
#import "UIActivityViewController+SY.h"

@interface SYOverviewController ()

@end

@implementation SYOverviewController

- (void)viewDidLoad
{
    [super viewDidLoad];
    
    // add trash and PDF buttons
    UIBarButtonItem *trashBarButton = [[UIBarButtonItem alloc] initWithBarButtonSystemItem:UIBarButtonSystemItemTrash target:self action:@selector(buttonTrashTap:)];
    UIBarButtonItem   *pdfBarButton = [[UIBarButtonItem alloc] initWithTitle:$$("PDF") style:UIBarButtonItemStylePlain target:self action:@selector(buttonPDFTap:)];
    UIBarButtonItem *shareBarButton = [[UIBarButtonItem alloc] initWithBarButtonSystemItem:UIBarButtonSystemItemAction target:self action:@selector(buttonShareTap:)];
    UIBarButtonItem *spaceBarButton = [[UIBarButtonItem alloc] initWithBarButtonSystemItem:UIBarButtonSystemItemFlexibleSpace target:nil action:nil];
    
    self.toolbarItems = @[trashBarButton, spaceBarButton, pdfBarButton, spaceBarButton, shareBarButton];
}

- (void)buttonTrashTap:(id)sender
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

- (void)buttonPDFTap:(UIBarButtonItem *)barButtonItem
{
    if (!self.collectionView.indexPathsForSelectedItems.count)
        return;
    
    [SVProgressHUD show];
    
    // needed to let SVProgressHUD appear, even if PDF is generated on main thread
    dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(0.1 * NSEC_PER_SEC)), dispatch_get_main_queue(), ^{
        [self sharePDFForSelectedItems:barButtonItem];
    });
}

- (NSArray <NSURL *> *)selectedURLs
{
    NSSortDescriptor *sortDescriptor = [NSSortDescriptor sortDescriptorWithKey:$$("item") ascending:NO];
    NSArray <NSIndexPath *> *sortedIndexPaths = [self.collectionView.indexPathsForSelectedItems sortedArrayUsingDescriptors:@[sortDescriptor]];
    NSMutableArray <NSURL *> *selectedItemsURLs = [NSMutableArray array];
    
    for (NSIndexPath *indexPath in sortedIndexPaths)
        [selectedItemsURLs addObject:[self itemForIndex:indexPath.item].URL];
    
    return [selectedItemsURLs copy];
}

- (void)sharePDFForSelectedItems:(UIBarButtonItem *)barButtonItem
{
    NSArray <NSURL *> *selectedItemsURLs = [self selectedURLs];
    
    if (!selectedItemsURLs.count)
        return;
    
    NSString *tempPath = [[SYGalleryManager shared] tempPDFPath];
    
    BOOL result = [SYPDFMaker createPDFAtURL:[NSURL fileURLWithPath:tempPath]
                            fromImagesAtURLs:selectedItemsURLs
                                 aspectRatio:210./297.
                                 jpegQuality:JPEG_COMP
                               fixedPageSize:YES];
    
    if (!result || ![[NSFileManager defaultManager] fileExistsAtPath:tempPath])
    {
        [SVProgressHUD showErrorWithStatus:$("ERROR MESSAGE PDF NOT CREATED")];
        return;
    }
    
    [SVProgressHUD dismiss];
    
    [UIActivityViewController sy_showForUrls:@[[NSURL fileURLWithPath:tempPath]]
                        fromBarButtonItem:barButtonItem
                                presentingVC:self
                                  completion:^
    {
        // is called when the interaction with the PDF is done. It's either been copied, imported,
        // displayed, shared or printed, but we can dispose of it
        [[SYGalleryManager shared] deleteTempPDFs];
    }];
}

- (void)buttonShareTap:(id)sender
{
    NSArray <NSURL *> *selectedItemsURLs = [self selectedURLs];
    
    if (!selectedItemsURLs.count)
        return;
    
    [UIActivityViewController sy_showForUrls:selectedItemsURLs
                           fromBarButtonItem:sender
                                presentingVC:self
                                  completion:nil];
}

@end
