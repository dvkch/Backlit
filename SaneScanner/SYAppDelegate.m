//
//  SYAppDelegate.m
//  SaneScanner
//
//  Created by rominet on 06/05/15.
//  Copyright (c) 2015 Syan. All rights reserved.
//

#import "SYAppDelegate.h"
#import "SYTools.h"
#import "SYDevicesVC.h"
#import "SYDeviceVC.h"
#import "SYSaneHelper.h"
#import "SYToolbar.h"
#import "SYGalleryManager.h"
#import "SVProgressHUD.h"
#import <UIImage+SYKit.h>
#import "SYEmptyGalleryVC.h"
#import "SYSplitVC.h"
#import "NSArray+SY.h"
#import "SYGalleryController.h"
#import "UIImage+SY.h"
#import "SYGalleryThumbsView.h"
#import "SYScanNC.h"
#import "MHUICustomization+SY.h"
#import <SDImageCache.h>
#import <SYWindow.h>

@interface SYAppDelegate () <SYGalleryManagerDelegate, UISplitViewControllerDelegate>
@property (nonatomic, strong) SYSplitVC *splitViewController;
@property (nonatomic, strong) SYScanNC *scanNavigationController;
@property (nonatomic, strong) SYGalleryController *galleryViewController;
@property (nonatomic, strong) SYEmptyGalleryVC *emptyVC;
@end

@implementation SYAppDelegate

+ (instancetype)obtain
{
    return (SYAppDelegate *)[[UIApplication sharedApplication] delegate];
}

- (BOOL)application:(UIApplication *)application didFinishLaunchingWithOptions:(NSDictionary *)launchOptions
{
    // log
    NSLog(@"%@", [SYTools documentsPath]);
    
    // create test images if needed
    //[SYTools createTestImages:400];
    
    // creating navigation controller
    SYDevicesVC *vc = [[SYDevicesVC alloc] init];
    self.scanNavigationController = [[SYScanNC alloc] init];
    [self.scanNavigationController setToolbarHidden:YES];
    [self.scanNavigationController.toolbar setHeight:64.];
    [self.scanNavigationController.toolbar setPadding:0.];
    [self.scanNavigationController.toolbar setTranslucent:NO];
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
    self.emptyVC = [[SYEmptyGalleryVC alloc] init];
    
    // creating split controller
    self.splitViewController = [[SYSplitVC alloc] init];
    [self.splitViewController setViewControllers:@[self.scanNavigationController, self.emptyVC]];
    [self.splitViewController setDelegate:self];
    [self.splitViewController setPreferredDisplayMode:UISplitViewControllerDisplayModeAllVisible];
    
    // creating window
    self.window = [SYWindow mainWindowWithRootViewController:self.splitViewController];
    
    // populate list for the first time
    [[SYSaneHelper shared] updateDevices:^(NSString *error) {
        [SVProgressHUD showErrorWithStatus:error];
    }];
    
    // customize HUD
    [SVProgressHUD setDefaultMaskType:SVProgressHUDMaskTypeBlack];
    
    // auto manage toolbar visibility
    [[SYGalleryManager shared] addDelegate:self];
    
    return YES;
}

- (void)splitVCtraitCollectionWillChangeTo:(UITraitCollection *)traitCollection
{
    BOOL constrainedW = (traitCollection.horizontalSizeClass == UIUserInterfaceSizeClassCompact);
    BOOL constrainedH = (traitCollection.verticalSizeClass   == UIUserInterfaceSizeClassCompact);
    
    [self.scanNavigationController.toolbar setHeight:(constrainedH ? 34 : 64)];
    
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

#pragma mark - Application

- (void)applicationWillResignActive:(UIApplication *)application {
    // Sent when the application is about to move from active to inactive state. This can occur for certain types of temporary interruptions (such as an incoming phone call or SMS message) or when the user quits the application and it begins the transition to the background state.
    // Use this method to pause ongoing tasks, disable timers, and throttle down OpenGL ES frame rates. Games should use this method to pause the game.
}

- (void)applicationDidEnterBackground:(UIApplication *)application {
    // Use this method to release shared resources, save user data, invalidate timers, and store enough application state information to restore your application to its current state in case it is terminated later.
    // If your application supports background execution, this method is called instead of applicationWillTerminate: when the user quits.
}

- (void)applicationWillEnterForeground:(UIApplication *)application {
    // Called as part of the transition from the background to the inactive state; here you can undo many of the changes made on entering the background.
}

- (void)applicationDidBecomeActive:(UIApplication *)application {
    // Restart any tasks that were paused (or not yet started) while the application was inactive. If the application was previously in the background, optionally refresh the user interface.
}

- (void)applicationWillTerminate:(UIApplication *)application {
    // Called when the application is about to terminate. Save data if appropriate. See also applicationDidEnterBackground:.
}

@end
