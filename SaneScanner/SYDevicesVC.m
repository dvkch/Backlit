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
#import "SYDeviceVC.h"
#import "SYAddCell.h"
#import "SVProgressHUD.h"
#import "SYPrefVC.h"
#import "SYGalleryThumbsView.h"
#import <Masonry.h>
#import "SYAppDelegate.h"
#import "SYGalleryManager.h"
#import "SYGalleryController.h"
#import "SYRefreshControl.h"
#import "UIApplication+SY.h"
#import "UIViewController+SYKit.h"
#import "SYDeviceCell.h"

@interface SYDevicesVC () <UITableViewDataSource, UITableViewDelegate, SYSaneHelperDelegate>
@property (nonatomic, strong) UITableView *tableView;
@property (nonatomic, strong) SYGalleryThumbsView *thumbsView;
@end

@implementation SYDevicesVC

- (void)viewDidLoad
{
    [super viewDidLoad];
    [self.view setBackgroundColor:[UIColor groupTableViewBackgroundColor]];
    
    self.tableView = [[UITableView alloc] initWithFrame:self.view.bounds style:UITableViewStyleGrouped];
    [self.tableView setDataSource:self];
    [self.tableView setDelegate:self];
    [self.tableView registerClass:[UITableViewCell class] forCellReuseIdentifier:[UITableViewCell sy_className]];
    [self.tableView registerClass:[SYDeviceCell class]    forCellReuseIdentifier:[SYDeviceCell sy_className]];
    [self.tableView registerClass:[SYAddCell class]       forCellReuseIdentifier:[SYAddCell sy_className]];
    [self.view addSubview:self.tableView];
    
    self.thumbsView = [SYGalleryThumbsView showInToolbarOfController:self tintColor:nil];
    
    [self.navigationItem setRightBarButtonItem:
     [SYPrefVC barButtonItemWithTarget:self action:@selector(buttonSettingsTap:)]];
    
    [SYRefreshControl addRefreshControlToScrollView:self.tableView triggerBlock:^(UIScrollView *scrollView) {
        [[SYSaneHelper shared] updateDevices:^(NSError *error) {
            if (error)
                [SVProgressHUD showErrorWithStatus:error.sy_alertMessage];
        }];
    }];
    
    [[SYSaneHelper shared] setDelegate:self];
    
    [self.tableView mas_makeConstraints:^(MASConstraintMaker *make) {
        make.edges.equalTo(@0);
    }];
    
    [self sy_setBackButtonWithText:nil font:nil];
    // TODO: crash after going to background
    // TODO: no scanner if multiple apps are launched ?
    // TODO: long timeout if using wrong ip, removing doesn't stop refresh
}

- (void)viewWillAppear:(BOOL)animated
{
    [super viewWillAppear:animated];
    [self setTitle:[[UIApplication sharedApplication] sy_localizedName]];
    [self.tableView reloadData];
}

#pragma mark - IBActions

- (void)buttonSettingsTap:(id)sender
{
    [SYPrefVC showOnVC:self closeBlock:nil];
}

#pragma mark - SYSaneHelperDelegate

- (void)saneHelperDidStartUpdatingDevices:(SYSaneHelper *)saneHelper
{
    [self.tableView ins_beginPullToRefresh];
}

- (void)saneHelperDidEndUpdatingDevices:(SYSaneHelper *)saneHelper
{
    [self.tableView reloadData];
    [self.tableView ins_endPullToRefresh];
}

- (void)saneHelper:(SYSaneHelper *)saneHelper
needsAuthForDevice:(NSString *)device
       outUsername:(NSString **)username
       outPassword:(NSString **)password
{
    dispatch_semaphore_t semaphore = dispatch_semaphore_create(0);
    
    __block NSString *outUsername;
    __block NSString *outPassword;
    
    dispatch_async(dispatch_get_main_queue(), ^
    {
        DLAVAlertView *alertView =
        [[DLAVAlertView alloc] initWithTitle:$("DIALOG TITLE AUTH")
                                     message:[NSString stringWithFormat:$("DIALOG MESSAGE AUTH %@"), device]
                                    delegate:nil
                           cancelButtonTitle:$("ACTION CANCEL")
                           otherButtonTitles:$("ACTION CONTINUE"), nil];
        
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
            UITableViewCell *cell = [tableView dequeueReusableCellWithIdentifier:[UITableViewCell sy_className]];
            [cell.textLabel setText:[[SYSaneHelper shared] allHosts][indexPath.row]];
            [cell setAccessoryType:UITableViewCellAccessoryNone];
            return cell;
        }
        else
        {
            SYAddCell *cell = (SYAddCell *)[tableView dequeueReusableCellWithIdentifier:[SYAddCell sy_className]];
            [cell setText:$("DEVICES ROW ADD HOST")];
            return cell;
        }
    }
    else
    {
        SYDeviceCell *cell = [tableView dequeueReusableCellWithIdentifier:[SYDeviceCell sy_className]];
        [cell setDevice:[[SYSaneHelper shared] allDevices][indexPath.row]];
        return cell;
    }
}

- (NSString *)tableView:(UITableView *)tableView titleForHeaderInSection:(NSInteger)section
{
    return section == 0 ? $("DEVICES SECTION HOSTS") : $("DEVICES SECTION DEVICES");
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
    [self.tableView reloadSections:[NSIndexSet indexSetWithIndex:1] withRowAnimation:UITableViewRowAnimationFade];
    [self.tableView endUpdates];
}

#pragma mark - UITableViewDelegate

- (CGFloat)tableView:(UITableView *)tableView heightForRowAtIndexPath:(NSIndexPath *)indexPath
{
    return indexPath.section == 0 ? 44 : 60;
}

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
    return $("ACTION REMOVE");
}

- (void)tableView:(UITableView *)tableView didSelectRowAtIndexPath:(NSIndexPath *)indexPath
{
    [tableView deselectRowAtIndexPath:indexPath animated:YES];
    
    if (indexPath.section == 0 && indexPath.row >= [[SYSaneHelper shared] allHosts].count)
    {
        DLAVAlertView *av = [[DLAVAlertView alloc] initWithTitle:$("DIALOG TITLE ADD HOST")
                                                         message:$("DIALOG MESSAGE ADD HOST")
                                                        delegate:nil
                                               cancelButtonTitle:$("ACTION CANCEL")
                                               otherButtonTitles:$("ACTION ADD"), nil];
        
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
    
    [SVProgressHUD showWithStatus:$("LOADING")];
    [[SYSaneHelper shared] openDevice:device block:^(NSError *error) {
        [SVProgressHUD dismiss];
        if (error)
        {
            [[[UIAlertView alloc] initWithTitle:$("DIALOG TITLE COULDNT OPEN DEVICE")
                                        message:error.sy_alertMessage
                                       delegate:nil
                              cancelButtonTitle:nil
                              otherButtonTitles:$("ACTION CLOSE"), nil] show];
        }
        else
        {
            SYDeviceVC *vc = [[SYDeviceVC alloc] init];
            [vc setDevice:device];
            [self.navigationController pushViewController:vc animated:YES];
        }
    }];
}

@end
