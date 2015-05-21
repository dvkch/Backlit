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
#import <BlocksKit/UIAlertView+BlocksKit.h>
#import "SSPullToRefresh.h"
#import "SYDeviceVC.h"
#import "MBProgressHUD+SYAdditions.h"

@interface SYDevicesVC () <UITableViewDataSource, UITableViewDelegate, SSPullToRefreshViewDelegate>
@property (nonatomic, strong) UITableView *tableView;
@property (nonatomic, strong) UIBarButtonItem *buttonAddHost;
@property (nonatomic, strong) SSPullToRefreshView *pullToRefreshView;
@end

@implementation SYDevicesVC

- (void)loadView
{
    [super loadView];
    
    self.tableView = [[UITableView alloc] initWithFrame:self.view.bounds style:UITableViewStyleGrouped];
    [self.tableView setAutoresizingMask:(UIViewAutoresizingFlexibleHeight | UIViewAutoresizingFlexibleWidth)];
    [self.tableView setDataSource:self];
    [self.tableView setDelegate:self];
    [self.view addSubview:self.tableView];
    
    self.buttonAddHost = [[UIBarButtonItem alloc] initWithBarButtonSystemItem:UIBarButtonSystemItemAdd
                                                                       target:self
                                                                       action:@selector(buttonAddHostTap:)];
    [self.buttonAddHost setTintColor:[UIColor darkGrayColor]];
    
    [self.navigationItem setRightBarButtonItems:@[self.buttonAddHost]];
}

- (void)viewWillAppear:(BOOL)animated
{
    [super viewWillAppear:animated];
    [self setTitle:@"SaneScanner"];
    
    __weak SYDevicesVC *wSelf = self;
    [[SYSaneHelper shared] setStartingDevicesUpdateBlock:^{
        [wSelf.pullToRefreshView startLoadingAndExpand:YES animated:YES];
    }];
    [[SYSaneHelper shared] setEndedDevicesUpdateBlock:^{
        [wSelf.tableView reloadData];
        [wSelf.pullToRefreshView finishLoading];
    }];

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

#pragma mark - IBActions

- (void)buttonAddHostTap:(id)sender
{
    UIAlertView *av = [[UIAlertView alloc] initWithTitle:@"Add a host"
                                                 message:@"Enter the hostname or IP address for the new host"
                                                delegate:nil
                                       cancelButtonTitle:nil
                                       otherButtonTitles:nil];
    
    [av setAlertViewStyle:UIAlertViewStylePlainTextInput];
    [av bk_setCancelButtonWithTitle:@"Cancel" handler:nil];
    [av bk_addButtonWithTitle:@"Add" handler:^{
        NSString *host = [[av textFieldAtIndex:0] text];
        [[SYSaneHelper shared] addHost:host];
        [self.tableView reloadData];
    }];
    
    [av show];
}

#pragma mark - UITableViewDataSource

- (NSInteger)numberOfSectionsInTableView:(UITableView *)tableView
{
    return 2;
}

- (NSInteger)tableView:(UITableView *)tableView numberOfRowsInSection:(NSInteger)section
{
    return section == 0 ? [[SYSaneHelper shared] hostsCount] : [[SYSaneHelper shared] devicesCount];
}

- (UITableViewCell *)tableView:(UITableView *)tableView cellForRowAtIndexPath:(NSIndexPath *)indexPath
{
    UITableViewCell *cell = [tableView dequeueReusableCellWithIdentifier:@"cell"];
    if(!cell)
        cell = [[UITableViewCell alloc] initWithStyle:UITableViewCellStyleDefault reuseIdentifier:@"cell"];
    
    
    NSString *text;
    if(indexPath.section == 0)
        text = [[SYSaneHelper shared] hostAtIndex:indexPath.row];
    else
    {
        text = [[[SYSaneHelper shared] deviceAtIndex:indexPath.row] model];
        [cell setAccessoryType:UITableViewCellAccessoryDisclosureIndicator];
    }
    
    [cell.textLabel setText:text];
    return cell;
}

- (NSString *)tableView:(UITableView *)tableView titleForHeaderInSection:(NSInteger)section
{
    return section == 0 ? @"Hosts" : @"Devices";
}

- (void)tableView:(UITableView *)tableView commitEditingStyle:(UITableViewCellEditingStyle)editingStyle forRowAtIndexPath:(NSIndexPath *)indexPath
{
    if(indexPath.section != 0)
        return;
    
    [[SYSaneHelper shared] removeHostAtIndex:indexPath.row];
    
    [self.tableView beginUpdates];
    [self.tableView deleteRowsAtIndexPaths:@[indexPath] withRowAnimation:UITableViewRowAnimationBottom];
    [self.tableView endUpdates];
}

#pragma mark - UITableViewDelegate

- (UITableViewCellEditingStyle)tableView:(UITableView *)tableView editingStyleForRowAtIndexPath:(NSIndexPath *)indexPath
{
    return indexPath.section == 0 ? UITableViewCellEditingStyleDelete : UITableViewCellEditingStyleNone;
}

- (NSString *)tableView:(UITableView *)tableView titleForDeleteConfirmationButtonForRowAtIndexPath:(NSIndexPath *)indexPath
{
    return @"Remove";
}

- (void)tableView:(UITableView *)tableView didSelectRowAtIndexPath:(NSIndexPath *)indexPath
{
    [tableView deselectRowAtIndexPath:indexPath animated:YES];
    if(indexPath.section == 0)
        return;
    
    SYSaneDevice *device = [[SYSaneHelper shared] deviceAtIndex:indexPath.row];
    if(!device)
        return;
    
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
    [[SYSaneHelper shared] updateDevices];
}

@end
