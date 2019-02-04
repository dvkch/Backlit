//
//  SYAppDelegate.m
//  SaneScanner
//
//  Created by rominet on 06/05/15.
//  Copyright (c) 2015 Syan. All rights reserved.
//

#import "SYAppDelegate.h"
#import "SYTools.h"
#import "SYGalleryManager.h"
#import "SVProgressHUD.h"
#import <UIImage+SYKit.h>
#import "NSArray+SY.h"
#import "SYGalleryController.h"
#import "SYGalleryThumbsView.h"
#import "MHUICustomization+SY.h"
#import <SDImageCache.h>
#import <SYWindow.h>
#import "MHGalleryItem+SY.h"
#import <SaneSwift/SaneSwift-umbrella.h>
#import "SaneScanner-Swift.h"

// TODO: send in chronological order, older to newer

@interface SYAppDelegate () <SYGalleryManagerDelegate, UISplitViewControllerDelegate>
@property (nonatomic, strong) SplitVC *splitViewController;
@property (nonatomic, strong) ScanNC *scanNavigationController;
@property (nonatomic, strong) SYGalleryController *galleryViewController;
@property (nonatomic, strong) EmptyGalleryVC *emptyVC;
@end

@implementation SYAppDelegate

+ (instancetype)obtain
{
    return (SYAppDelegate *)[[UIApplication sharedApplication] delegate];
}

- (BOOL)application:(UIApplication *)application didFinishLaunchingWithOptions:(NSDictionary *)launchOptions
{
    // log
    NSLog($$("Document path: %@"), [SYTools documentsPath]);
    
    // create test images if needed
    //[SYTools createTestImages:200];
    
    // creating navigation controller
    DevicesVC *vc = [[DevicesVC alloc] init];
    self.scanNavigationController = [[ScanNC alloc] init];
    [self.scanNavigationController setToolbarHidden:YES];
    // TODO: [self.scanNavigationController.customToolbar setHeight:64.];
    [self.scanNavigationController.customToolbar setPadding:0.];
    [self.scanNavigationController.customToolbar setTranslucent:NO];
    [self.scanNavigationController setViewControllers:@[vc]];
    
    // gallery view controller
    self.galleryViewController =
    [SYGalleryController galleryWithPresentationStyle:MHGalleryViewModeOverView
                                      UICustomization:[MHUICustomization sy_defaultTheme]];
    [self.galleryViewController.UICustomization setHideDoneButton:YES];
    [self.galleryViewController.UICustomization
     setMHGalleryBackgroundColor:[UIColor groupTableViewBackgroundColor]
     forViewMode:MHGalleryViewModeImageViewerNavigationBarHidden];
    
    // empty gallery view controller
    self.emptyVC = [[EmptyGalleryVC alloc] init];
    
    // creating split controller
    self.splitViewController = [[SplitVC alloc] init];
    [self.splitViewController setViewControllers:@[self.scanNavigationController, self.emptyVC]];
    [self.splitViewController setDelegate:self];
    [self.splitViewController setPreferredDisplayMode:UISplitViewControllerDisplayModeAllVisible];
    
    // creating window
    self.window = [SYWindow mainWindowWithRootViewController:self.splitViewController];
    
    // customize HUD
    [SVProgressHUD setDefaultMaskType:SVProgressHUDMaskTypeBlack];
    
    // auto manage toolbar visibility
    [[SYGalleryManager shared] addDelegate:self];
    
    // Snapshots
    {
        if ([[NSProcessInfo processInfo].arguments containsObject:$$("DOING_SNAPSHOT")])
        {
            self->_snapshotType = SYSnapshotTypeOther;
            [Sane.shared.configuration clearHosts];
            [Sane.shared.configuration addHost:$$("192.168.69.42")];
            [SVProgressHUD show];
        }
        
        if ([[NSProcessInfo processInfo].arguments containsObject:$$("SNAPSHOT_PREVIEW")])
            self->_snapshotType = SYSnapshotTypeDevicePreview;
        
        if ([[NSProcessInfo processInfo].arguments containsObject:$$("SNAPSHOT_OPTIONS")])
            self->_snapshotType = SYSnapshotTypeDeviceOptions;
        
        if ([[NSProcessInfo processInfo].arguments containsObject:$$("SNAPSHOT_OPTION_POPUP")])
            self->_snapshotType = SYSnapshotTypeDeviceOptionPopup;
        
        NSString *pathPrefix = $$("SNAPSHOT_TEST_IMAGE_PATH=");
        for (NSString *argument in [NSProcessInfo processInfo].arguments)
        {
            if ([argument containsString:pathPrefix])
            {
                self->_snapshotTestScanImagePath = [argument substringFromIndex:pathPrefix.length];
                break;
            }
        }
    }
    
    return YES;
}

- (void)applicationDidEnterBackground:(UIApplication *)application
{
    UIBackgroundTaskIdentifier taskID = [[UIApplication sharedApplication] beginBackgroundTaskWithExpirationHandler:^{}];
    
    [Sane.shared cancelCurrentScan];
    
    for (UIViewController *vc in self.splitViewController.viewControllers)
        if ([vc isKindOfClass:[ScanNC class]])
            [(ScanNC *)vc popToRootViewControllerAnimated:NO];
    
    // give time to the system to really close the deviceVC if
    // it's opened, close eventual scan alertView, and dealloc
    // the VC, which will in turn closing the device and make
    // sane exit gracefully
    dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(5. * NSEC_PER_SEC)), dispatch_get_main_queue(), ^{
        [[UIApplication sharedApplication] endBackgroundTask:taskID];
    });
}

- (void)applicationWillTerminate:(UIApplication *)application {
    // Called when the application is about to terminate. Save data if appropriate. See also applicationDidEnterBackground:.
}

#pragma mark - SplitVC

- (void)splitVCtraitCollectionWillChangeTo:(UITraitCollection *)traitCollection
{
    BOOL constrainedW = (traitCollection.horizontalSizeClass == UIUserInterfaceSizeClassCompact);
    BOOL constrainedH = (traitCollection.verticalSizeClass   == UIUserInterfaceSizeClassCompact);
    
    // TODO: [self.scanNavigationController.customToolbar setHeight:(constrainedH ? 34 : 64)];
    
    if (!constrainedW)
    {
        if ([self.scanNavigationController.presentedViewController isKindOfClass:[SYGalleryController class]])
        {
            SYGalleryController *currentGallery = (SYGalleryController *)self.scanNavigationController.presentedViewController;
            if ([currentGallery isShowingOverview])
                [self.galleryViewController openOverview];
            else
                [self.galleryViewController openImageViewForPage:currentGallery.imageViewerViewController.pageIndex];
        }
        
        [self.scanNavigationController dismissViewControllerAnimated:NO completion:nil];
    }
    
    [self.scanNavigationController setToolbarHidden:(!constrainedW || ![[SYGalleryManager shared] galleryItems].count) animated:YES];
}

#pragma mark - GalleryManager

- (void)gallerymanager:(SYGalleryManager *)gallerymanager
 didUpdateGalleryItems:(NSArray<MHGalleryItem *> *)items
               newItem:(MHGalleryItem *)newItem
           removedItem:(MHGalleryItem *)removedItem
{
    BOOL constrainedW = (self.splitViewController.traitCollection.horizontalSizeClass == UIUserInterfaceSizeClassCompact);
    
    [self.scanNavigationController setToolbarHidden:(!constrainedW || !items.count) animated:YES];

    UIViewController *detailsViewController = [self.splitViewController.viewControllers nullableObjectAtIndex:1];
    
    if (!items.count && detailsViewController != self.emptyVC)
        [self.splitViewController setViewControllers:@[self.scanNavigationController, self.emptyVC]];
    
    if ( items.count && detailsViewController != self.galleryViewController)
        [self.splitViewController setViewControllers:@[self.scanNavigationController, self.galleryViewController]];
}

#pragma mark - SplitViewController

- (BOOL)splitViewController:(UISplitViewController *)splitViewController
collapseSecondaryViewController:(UIViewController *)secondaryViewController
  ontoPrimaryViewController:(UIViewController *)primaryViewController
{
    return (splitViewController.traitCollection.horizontalSizeClass == UIUserInterfaceSizeClassCompact);
}

- (UIViewController *)splitViewController:(UISplitViewController *)splitViewController
separateSecondaryViewControllerFromPrimaryViewController:(UIViewController *)primaryViewController
{
    if ([[SYGalleryManager shared] galleryItems].count)
        return self.galleryViewController;
    else
        return self.emptyVC;
}

- (void)splitViewController:(UISplitViewController *)svc willChangeToDisplayMode:(UISplitViewControllerDisplayMode)displayMode
{
    [self.galleryViewController.overViewViewController viewWillAppear:NO];
}

@end
