//
//  UIActivityViewController+SY.m
//  SaneScanner
//
//  Created by Stanislas Chevallier on 19/04/2017.
//  Copyright © 2017 Syan. All rights reserved.
//

#import "UIActivityViewController+SY.h"
#import "SYPDFActivity.h"
#import "SVProgressHUD+SY.h"

@implementation UIActivityViewController (SY)

+ (void)sy_showForUrls:(NSArray <NSURL *> *)urls
     fromBarButtonItem:(UIBarButtonItem *)barButtonItem
          presentingVC:(UIViewController *)presentingVC
            completion:(void(^)(void))completion
{
    SYPDFActivity *pdfActivity = [SYPDFActivity new];
    
    UIActivityViewController *activityViewController =
    [[UIActivityViewController alloc] initWithActivityItems:urls
                                      applicationActivities:@[pdfActivity]];
    
    [activityViewController setCompletionWithItemsHandler:^(UIActivityType activityType, BOOL completed, NSArray *returnedItems, NSError *activityError) {
        if (activityError)
            [SVProgressHUD showErrorWithStatus:activityError.sy_alertMessage];
        
        if (completion)
            completion();
    }];
    
    UIPopoverPresentationController *popover = activityViewController.popoverPresentationController;
    [popover setBarButtonItem:barButtonItem];
    
    pdfActivity.presentingViewController = presentingVC;
    [pdfActivity configureWithOriginalActivityViewController:activityViewController];

    [presentingVC presentViewController:activityViewController animated:YES completion:nil];
}

+ (void)sy_showForUrls:(NSArray <NSURL *> *)urls
  bottomInPresentingVC:(UIViewController *)presentingVC
            completion:(void(^)(void))completion
{
    SYPDFActivity *pdfActivity = [SYPDFActivity new];
    
    UIActivityViewController *activityViewController =
    [[UIActivityViewController alloc] initWithActivityItems:urls
                                      applicationActivities:@[pdfActivity]];
    
    [activityViewController setCompletionWithItemsHandler:^(UIActivityType activityType, BOOL completed, NSArray *returnedItems, NSError *activityError) {
        if (activityError)
            [SVProgressHUD showErrorWithStatus:activityError.sy_alertMessage];
        
        if (completion)
            completion();
    }];
    
    UIPopoverPresentationController *popover = activityViewController.popoverPresentationController;
    [popover setPermittedArrowDirections:0];
    [popover setSourceView:presentingVC.view];
    [popover setSourceRect:CGRectMake(presentingVC.view.bounds.size.width / 2.,
                                      presentingVC.view.bounds.size.height,
                                      1, 1)];
    
    pdfActivity.presentingViewController = presentingVC;
    [pdfActivity configureWithOriginalActivityViewController:activityViewController];
    
    [presentingVC presentViewController:activityViewController animated:YES completion:nil];
}

@end
