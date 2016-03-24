//
//  SYDevicesVC.m
//  SaneScanner
//
//  Created by rominet on 06/05/15.
//  Copyright (c) 2015 Syan. All rights reserved.
//

#import "SYDevicesVC.h"
#import "SYTools.h"
#import "SYSaneHelper.h"
#import "SYSaneDevice.h"
#import <DLAVAlertView.h>
#import "SSPullToRefresh.h"
#import "SYDeviceVC.h"
#import "SYAddCell.h"
#import "SVProgressHUD.h"
#import "SYPrefVC.h"
#import "SYGalleryThumbsView.h"
#import <Masonry.h>
#import "SYAppDelegate.h"
#import "SYGalleryManager.h"
#import "MHGalleryController+SY.h"

@interface SYDevicesVC () <UITableViewDataSource, UITableViewDelegate, SSPullToRefreshViewDelegate, SYSaneHelperDelegate, SYGalleryManagerDelegate>
@property (nonatomic, strong) UITableView *tableView;
@property (nonatomic, strong) SSPullToRefreshView *pullToRefreshView;
@property (nonatomic, strong) SYGalleryThumbsView *thumbsView;
@property (nonatomic, strong) MHGalleryController *galleryVC;
@end

@implementation SYDevicesVC

- (void)viewDidLoad
{
    [super viewDidLoad];
    
    self.tableView = [[UITableView alloc] initWithFrame:self.view.bounds style:UITableViewStyleGrouped];
    [self.tableView setDataSource:self];
    [self.tableView setDelegate:self];
    [self.tableView registerClass:[UITableViewCell class] forCellReuseIdentifier:@"UITableViewCell"];
    [self.tableView registerClass:[SYAddCell class]       forCellReuseIdentifier:@"SYAddCell"];
    [self.view addSubview:self.tableView];
    
    self.thumbsView = [SYGalleryThumbsView showInToolbarOfController:self tintColor:nil];
    
    [self.navigationItem setRightBarButtonItem:
     [SYPrefVC barButtonItemWithTarget:self action:@selector(buttonSettingsTap:)]];
    
    [[SYSaneHelper shared] setDelegate:self];
    [[SYGalleryManager shared] addDelegate:self];
    
    [self.tableView mas_makeConstraints:^(MASConstraintMaker *make) {
        make.edges.equalTo(@0);
    }];
}

- (void)viewWillAppear:(BOOL)animated
{
    [super viewWillAppear:animated];
    [self setTitle:@"SaneScanner"];
    [self.tableView reloadData];
}

- (void)viewWillDisappear:(BOOL)animated
{
    [super viewWillDisappear:animated];
    [self setTitle:@""];
}

- (void)viewDidLayoutSubviews
{
    if(self.pullToRefreshView == nil)
    {
        self.pullToRefreshView = [[SSPullToRefreshView alloc] initWithScrollView:self.tableView
                                                                        delegate:self];
        if([[SYSaneHelper shared] isUpdatingDevices])
        {
            [self.pullToRefreshView startLoadingAndExpand:YES animated:NO];
        }
    }
}

- (void)presentViewController:(UIViewController *)viewController animated:(BOOL)flag completion:(void (^)(void))completion
{
    [super presentViewController:viewController animated:flag completion:completion];
    if ([viewController isKindOfClass:[MHGalleryController class]])
        self.galleryVC = (MHGalleryController *)viewController;
}

#pragma mark - IBActions

- (void)buttonSettingsTap:(id)sender
{
    [SYPrefVC showOnVC:self closeBlock:nil];
}

#pragma mark - GalleryManager

- (void)gallerymanager:(SYGalleryManager *)gallerymanager
 didUpdateGalleryItems:(NSArray<MHGalleryItem *> *)items
               newItem:(MHGalleryItem *)newItem
           removedItem:(MHGalleryItem *)removedItem
{
    [self.galleryVC setGalleryItems:items];
}

#pragma mark - SYSaneHelperDelegate

- (void)saneHelperDidStartUpdatingDevices:(SYSaneHelper *)saneHelper
{
    [self.pullToRefreshView startLoadingAndExpand:YES animated:YES];
}

- (void)saneHelperDidEndUpdatingDevices:(SYSaneHelper *)saneHelper
{
    [self.tableView reloadData];
    [self.pullToRefreshView finishLoading];
}

- (void)saneHelper:(SYSaneHelper *)saneHelper
needsAuthForDevice:(NSString *)device
       outUsername:(NSString **)username
       outPassword:(NSString **)password
{
    dispatch_semaphore_t semaphore = dispatch_semaphore_create(0);
    
    __block NSString *outUsername;
    __block NSString *outPassword;
    
    dispatch_async(dispatch_get_main_queue(), ^{
        NSString *message = [NSString stringWithFormat:@"Please enter the username and password for %@", device];
        DLAVAlertView *alertView = [[DLAVAlertView alloc] initWithTitle:@"Authentication needed"
                                                                message:message
                                                               delegate:nil
                                                      cancelButtonTitle:@"Cancel"
                                                      otherButtonTitles:@"Continue", nil];
        [alertView setAlertViewStyle:DLAVAlertViewStyleLoginAndPasswordInput];
        [[alertView textFieldAtIndex:0] setBorderStyle:UITextBorderStyleNone];
        [[alertView textFieldAtIndex:1] setBorderStyle:UITextBorderStyleNone];
        [alertView showWithCompletion:^(DLAVAlertView *alertView, NSInteger buttonIndex) {
            if (buttonIndex != alertView.cancelButtonIndex)
            {
                outUsername = [alertView textFieldAtIndex:0].text;
                outPassword = [alertView textFieldAtIndex:1].text;
            }
            dispatch_semaphore_signal(semaphore);
        }];
    });
    
    dispatch_semaphore_wait(semaphore, DISPATCH_TIME_FOREVER);
    
    if (username)
        *username = outUsername;
    
    if (password)
        *password = outPassword;
}

#pragma mark - UITableViewDataSource

- (NSInteger)numberOfSectionsInTableView:(UITableView *)tableView
{
    return 2;
}

- (NSInteger)tableView:(UITableView *)tableView numberOfRowsInSection:(NSInteger)section
{
    return section == 0 ? ([[SYSaneHelper shared] allHosts].count+1) : [[SYSaneHelper shared] allDevices].count;
}

- (UITableViewCell *)tableView:(UITableView *)tableView cellForRowAtIndexPath:(NSIndexPath *)indexPath
{
    if (indexPath.section == 0)
    {
        if (indexPath.row < [[SYSaneHelper shared] allHosts].count)
        {
            UITableViewCell *cell = [tableView dequeueReusableCellWithIdentifier:@"UITableViewCell"];
            [cell.textLabel setText:[[SYSaneHelper shared] allHosts][indexPath.row]];
            [cell setAccessoryType:UITableViewCellAccessoryNone];
            return cell;
        }
        else
        {
            SYAddCell *cell = (SYAddCell *)[tableView dequeueReusableCellWithIdentifier:@"SYAddCell"];
            [cell setText:@"Add new host"];
            return cell;
        }
    }
    else
    {
        UITableViewCell *cell = [tableView dequeueReusableCellWithIdentifier:@"UITableViewCell"];
        [cell.textLabel setText:[[SYSaneHelper shared] allDevices][indexPath.row].model];
        [cell setAccessoryType:UITableViewCellAccessoryDisclosureIndicator];
        return cell;
    }
}

- (NSString *)tableView:(UITableView *)tableView titleForHeaderInSection:(NSInteger)section
{
    return section == 0 ? @"Hosts" : @"Devices";
}

- (void)tableView:(UITableView *)tableView commitEditingStyle:(UITableViewCellEditingStyle)editingStyle forRowAtIndexPath:(NSIndexPath *)indexPath
{
    if (indexPath.section != 0)
        return;
    
    if (indexPath.row >= [[SYSaneHelper shared] allHosts].count)
        return;
    
    [[SYSaneHelper shared] removeHost:[[SYSaneHelper shared] allHosts][indexPath.row]];
    
    [self.tableView beginUpdates];
    [self.tableView deleteRowsAtIndexPaths:@[indexPath] withRowAnimation:UITableViewRowAnimationBottom];
    [self.tableView endUpdates];
}

#pragma mark - UITableViewDelegate

- (UITableViewCellEditingStyle)tableView:(UITableView *)tableView editingStyleForRowAtIndexPath:(NSIndexPath *)indexPath
{
    if (indexPath.section == 0)
    {
        if (indexPath.row < [[SYSaneHelper shared] allHosts].count)
            return UITableViewCellEditingStyleDelete;
        else
            return UITableViewCellEditingStyleNone;
    }
    else
        return UITableViewCellEditingStyleNone;
}

- (NSString *)tableView:(UITableView *)tableView titleForDeleteConfirmationButtonForRowAtIndexPath:(NSIndexPath *)indexPath
{
    return @"Remove";
}

- (void)tableView:(UITableView *)tableView didSelectRowAtIndexPath:(NSIndexPath *)indexPath
{
    [tableView deselectRowAtIndexPath:indexPath animated:YES];
    
    if (indexPath.section == 0 && indexPath.row >= [[SYSaneHelper shared] allHosts].count)
    {
        DLAVAlertView *av = [[DLAVAlertView alloc] initWithTitle:@"Add a host"
                                                         message:@"Enter the hostname or IP address for the new host"
                                                        delegate:nil
                                               cancelButtonTitle:@"Cancel"
                                               otherButtonTitles:@"Add", nil];
        
        [av setAlertViewStyle:DLAVAlertViewStylePlainTextInput];
        [[av textFieldAtIndex:0] setBorderStyle:UITextBorderStyleNone];
        [av showWithCompletion:^(DLAVAlertView *alertView, NSInteger buttonIndex) {
            if (buttonIndex == alertView.cancelButtonIndex)
                return;
            NSString *host = [[av textFieldAtIndex:0] text];
            [[SYSaneHelper shared] addHost:host];
            [self.tableView reloadData];
        }];
        
        return;
    }
    
    else if (indexPath.section == 0)
        return;
    
    SYSaneDevice *device = [[SYSaneHelper shared] allDevices][indexPath.row];
    
    [SVProgressHUD showWithStatus:@"Loading..."];
    [[SYSaneHelper shared] openDevice:device block:^(NSString *error) {
        [SVProgressHUD dismiss];
        if(error)
        {
            NSString *msg = [NSString stringWithFormat:@"Couldn't open device: %@", error];
            [[[UIAlertView alloc] initWithTitle:device.model
                                        message:msg
                                       delegate:nil
                              cancelButtonTitle:nil
                              otherButtonTitles:@"Close", nil] show];
        }
        else
        {
            SYDeviceVC *vc = [[SYDeviceVC alloc] init];
            [vc setDevice:device];
            [self.navigationController pushViewController:vc animated:YES];
        }
    }];
}

#pragma mark - SSPullToRefreshViewDelegate

- (void)pullToRefreshViewDidStartLoading:(SSPullToRefreshView *)view
{
    [[SYSaneHelper shared] updateDevices:^(NSString *error) {
        [SVProgressHUD showErrorWithStatus:error];
    }];
}

@end
