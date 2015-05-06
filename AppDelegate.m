//
//  AppDelegate.m
//  SANE
//
//  Created by rominet on 06/05/15.
//  Copyright (c) 2015 Syan. All rights reserved.
//

#import "AppDelegate.h"
#import "sane.h"

@interface AppDelegate ()

@end

@implementation AppDelegate

+ (NSString *) applicationDocumentsDirectory
{
    NSArray *paths = NSSearchPathForDirectoriesInDomains(NSDocumentDirectory, NSUserDomainMask, YES);
    NSString *basePath = ([paths count] > 0) ? [paths objectAtIndex:0] : nil;
    return basePath;
}

- (BOOL)application:(UIApplication *)application didFinishLaunchingWithOptions:(NSDictionary *)launchOptions
{
    NSString *docs = [[self class] applicationDocumentsDirectory];
    [[NSFileManager defaultManager] changeCurrentDirectoryPath:docs];
    [@"connect_timeout = 30\n192.168.1.42" writeToFile:[docs stringByAppendingPathComponent:@"net.conf"] atomically:YES encoding:NSUTF8StringEncoding error:NULL];
    
    NSLog(@"%@", [[NSFileManager defaultManager] currentDirectoryPath]);
    
    // Override point for customization after application launch.
    dispatch_async(dispatch_get_main_queue(), ^
    {
        SANE_Int version;
        SANE_Status s = sane_init(&version, NULL);
        if(s == SANE_STATUS_GOOD)
            NSLog(@"yo! %d", version);
        
        const SANE_Device **devices = NULL;
        SANE_Handle handle;
        //sane_open("net:192.168.1.42", &handle);
        sane_get_devices(&devices, SANE_FALSE);
        uint i = 0;
        while(devices[i]) {
            const SANE_Device *d = devices[i];
            NSLog(@"Device %d: %s %s %s (%s)", i, d->name, d->vendor, d->model, d->type);
            ++i;
        }
        
        
        
        sane_open("net:192.168.1.42", &handle);

        if (s == SANE_STATUS_GOOD)
            NSLog(@"opened");
        else
            NSLog(@"erro %s", sane_strstatus(s));

        NSLog(@"%d devices", i);
    });
    
    return YES;
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
