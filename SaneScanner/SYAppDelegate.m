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
#import "MHGalleryController+SY.h"
#import "UIImage+SY.h"
#import "SYGalleryThumbsView.h"
#include <stdlib.h>

@interface SYAppDelegate () <SYGalleryManagerDelegate, UISplitViewControllerDelegate, UINavigationControllerDelegate>
@property (nonatomic, strong) SYSplitVC *splitViewController;
@property (nonatomic, strong) UINavigationController *scanNavigationController;
@property (nonatomic, strong) MHGalleryController *galleryViewController;
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
    
    // init swizzlings
    [MHGalleryController sy_fix];
    
    // creating navigation controller
    SYDevicesVC *vc = [[SYDevicesVC alloc] init];
    self.scanNavigationController =
    [[UINavigationController alloc] initWithNavigationBarClass:nil toolbarClass:[SYToolbar class]];
    [self.scanNavigationController setToolbarHidden:YES];
    [(SYToolbar *)self.scanNavigationController.toolbar setHeight:64.];
    [(SYToolbar *)self.scanNavigationController.toolbar setPadding:0.];
    [self.scanNavigationController.toolbar setTranslucent:NO];
    [self.scanNavigationController setDelegate:self];
    [self.scanNavigationController setViewControllers:@[vc]];
    
    // gallery view controller
    self.galleryViewController =
    [MHGalleryController galleryWithPresentationStyle:MHGalleryViewModeImageViewerNavigationBarShown];
    [self.galleryViewController setHideDoneButton:YES];
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
    self.window = [[UIWindow alloc] init];
    [self.window makeKeyAndVisible];
    [self.window setRootViewController:self.splitViewController];
    [self.window setBackgroundColor:[UIColor whiteColor]];
    [self.window.layer setMasksToBounds:YES];
    [self.window.layer setOpaque:NO];
    
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

- (void)updateToolbarAndGalleryForTraitCollection:(UITraitCollection *)traitCollection
{
    UITraitCollection *traits = traitCollection ?: self.splitViewController.traitCollection;
    NSArray <MHGalleryItem *> *galleryItems = [[SYGalleryManager shared] galleryItems];

    BOOL constrainedW = (traits.horizontalSizeClass == UIUserInterfaceSizeClassCompact);
    BOOL hasImages    = (galleryItems.count > 0);
    [self.scanNavigationController setToolbarHidden:(!constrainedW || !hasImages) animated:YES];
    
    [self.galleryViewController setGalleryItems:galleryItems];
    
    UIViewController *detailsViewController = [self.splitViewController.viewControllers nullableObjectAtIndex:1];
    
    if (!constrainedW)
        [self.scanNavigationController dismissViewControllerAnimated:NO completion:nil];
    
    if (!hasImages && detailsViewController == self.galleryViewController)
        [self.splitViewController setViewControllers:@[self.scanNavigationController, self.emptyVC]];
    
    if (hasImages && detailsViewController != self.galleryViewController)
        [self.splitViewController setViewControllers:@[self.scanNavigationController, self.galleryViewController]];
    
    if (traits.verticalSizeClass == UIUserInterfaceSizeClassCompact)
        [(SYToolbar *)self.scanNavigationController.toolbar setHeight:34];
    else
        [(SYToolbar *)self.scanNavigationController.toolbar setHeight:64];
}

#pragma mark - GalleryManager

- (void)gallerymanager:(SYGalleryManager *)gallerymanager
 didUpdateGalleryItems:(NSArray<MHGalleryItem *> *)items
               newItem:(MHGalleryItem *)newItem
           removedItem:(MHGalleryItem *)removedItem
{
    [self updateToolbarAndGalleryForTraitCollection:nil];
}

- (BOOL)splitViewController:(UISplitViewController *)splitViewController
collapseSecondaryViewController:(UIViewController *)secondaryViewController
  ontoPrimaryViewController:(UIViewController *)primaryViewController
{
    return (splitViewController.traitCollection.horizontalSizeClass == UIUserInterfaceSizeClassCompact);
}

#pragma mark - NavigationController

- (void)navigationController:(UINavigationController *)navigationController
      willShowViewController:(UIViewController *)viewController
                    animated:(BOOL)animated
{
    if (navigationController != self.scanNavigationController)
        return;
    
    if ([viewController isKindOfClass:[SYDevicesVC class]] || [viewController isKindOfClass:[SYDeviceVC class]])
    {
        SYGalleryThumbsView *thumbsView = viewController.toolbarItems.firstObject.customView;
        if ([thumbsView isKindOfClass:[SYGalleryThumbsView class]])
        {
            NSLog(@"do shit");
        }
    }
}


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
