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
#import "SYSaneHelper.h"
#import "SYToolbar.h"
#import "SYGalleryManager.h"
#import <SVProgressHUD.h>

@interface SYAppDelegate () <SYGalleryManagerDelegate>
@property (nonatomic, strong) UINavigationController *navigationController;
@end

@implementation SYAppDelegate

+ (instancetype)obtain
{
    return (SYAppDelegate *)[[UIApplication sharedApplication] delegate];
}

- (BOOL)application:(UIApplication *)application didFinishLaunchingWithOptions:(NSDictionary *)launchOptions
{
    // needed for Sane-net config file
    [[NSFileManager defaultManager] changeCurrentDirectoryPath:[SYTools appSupportPath]];

    // creating navigation controller
    SYDevicesVC *vc = [[SYDevicesVC alloc] init];
    self.navigationController =
    [[UINavigationController alloc] initWithNavigationBarClass:nil toolbarClass:[SYToolbar class]];
    [self.navigationController setToolbarHidden:YES];
    [(SYToolbar *)self.navigationController.toolbar setHeight:64.];
    [(SYToolbar *)self.navigationController.toolbar setPadding:0.];
    [self.navigationController.toolbar setTranslucent:NO];
    [self.navigationController setViewControllers:@[vc]];
    
    // creating window
    self.window = [[UIWindow alloc] init];
    [self.window makeKeyAndVisible];
    [self.window setFrame:[[UIScreen mainScreen] bounds]];
    [self.window setRootViewController:self.navigationController];
    [self.window setBackgroundColor:[UIColor whiteColor]];
    [self.window.layer setMasksToBounds:YES];
    [self.window.layer setOpaque:NO];
    
    // populate list for the first time
    [[SYSaneHelper shared] updateDevices];
    
    // customize HUD
    [SVProgressHUD setDefaultMaskType:SVProgressHUDMaskTypeBlack];
    
    // auto manage toolbar visibility
    [[SYGalleryManager shared] addDelegate:self];
#warning need to restart app to see device ?!
    return YES;
}

#warning better delegate
- (void)gallerymanager:(SYGalleryManager *)gallerymanager didUpdateImageList:(NSArray<NSString *> *)imageList
{
    [self.navigationController setToolbarHidden:(imageList.count == 0) animated:YES];
}

- (void)gallerymanager:(SYGalleryManager *)gallerymanager didAddImage:(NSString *)imageName
{
    [self.navigationController setToolbarHidden:(gallerymanager.imageNames.count == 0) animated:YES];
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
