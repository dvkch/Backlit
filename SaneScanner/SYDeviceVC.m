//
//  SYDeviceVC.m
//  SaneScanner
//
//  Created by rominet on 06/05/15.
//  Copyright (c) 2015 Syan. All rights reserved.
//

#import "SYDeviceVC.h"
#import "SYSaneDevice.h"
#import "SYSaneHelper.h"
#import "SYSaneOptionBool.h"
#import "SYSaneOptionNumber.h"
#import "SYSaneOptionString.h"
#import "SYSaneOptionButton.h"
#import "SYSaneOptionGroup.h"
#import "SSPullToRefresh.h"
#import "SYOptionCell.h"
#import "SYPreviewCell.h"
#import <SVProgressHUD.h>
#import "DLAVAlertView+SY.h"
#import "SYSaneOptionUI.h"
#import "SYTools.h"
#import <Masonry.h>
#import "SYPrefVC.h"
#import "SYPreferences.h"

@interface SYDeviceVC () <UITableViewDataSource, UITableViewDelegate, SSPullToRefreshViewDelegate>
@property (nonatomic, strong) SSPullToRefreshView *pullToRefreshView;
@property (nonatomic, strong) UITableView *tableView;
@property (nonatomic, strong) UIButton *buttonScan;
@property (nonatomic, assign) BOOL refreshing;
@end

@implementation SYDeviceVC

- (void)viewDidLoad
{
    [super viewDidLoad];
    
    CGFloat buttonHeight = 44;
    
    self.tableView = [[UITableView alloc] initWithFrame:CGRectZero style:UITableViewStyleGrouped];
    [self.tableView setDelegate:self];
    [self.tableView setDataSource:self];
    [self.tableView registerClass:[SYPreviewCell class] forCellReuseIdentifier:@"SYPreviewCell"];
    [self.tableView registerClass:[SYOptionCell  class] forCellReuseIdentifier:@"SYOptionCell"];
    [self.view addSubview:self.tableView];
    
    self.buttonScan = [UIButton buttonWithType:UIButtonTypeSystem];
    [self.buttonScan addTarget:self action:@selector(buttonScanTap:) forControlEvents:UIControlEventTouchUpInside];
    [self.buttonScan setBackgroundColor:[UIColor colorWithRed:0. green:0.48 blue:1. alpha:1.]];
    [self.buttonScan setTitleColor:[UIColor whiteColor] forState:UIControlStateNormal];
    [self.buttonScan setTitle:@"SCAN" forState:UIControlStateNormal];
    [self.buttonScan.titleLabel setFont:[UIFont systemFontOfSize:17]];
    [self.view addSubview:self.buttonScan];
    
    [self.navigationItem setRightBarButtonItem:[SYPrefVC barButtonItemWithTarget:self action:@selector(buttonSettingsTap:)]];
    
    [self.tableView mas_makeConstraints:^(MASConstraintMaker *make) {
        make.top.equalTo(@0);
        make.left.equalTo(@0);
        make.right.equalTo(@0);
        make.bottom.equalTo(@(-buttonHeight));
    }];
    
    [self.buttonScan mas_makeConstraints:^(MASConstraintMaker *make) {
        make.top.equalTo(self.tableView.mas_bottom);
        make.left.equalTo(@0);
        make.right.equalTo(@0);
        make.bottom.equalTo(@0);
    }];
}

- (void)viewWillAppear:(BOOL)animated
{
    [super viewWillAppear:animated];
    [self setTitle:[self.device model]];
    [self refresh];
}

- (void)dealloc
{
    [[SYSaneHelper shared] closeDevice:self.device];
}

- (void)viewDidLayoutSubviews
{
    if(self.pullToRefreshView == nil)
    {
        self.pullToRefreshView = [[SSPullToRefreshView alloc] initWithScrollView:self.tableView
                                                                        delegate:self];
        if(self.refreshing)
            [self.pullToRefreshView startLoadingAndExpand:YES animated:NO];
    }
}

#pragma mark - Data

- (void)refresh
{
    if(self.refreshing)
        return;
    
    self.refreshing = YES;
    
    __weak SYDeviceVC *wSelf = self;
    [[SYSaneHelper shared] listOptionsForDevice:self.device block:^ {
        [wSelf.tableView reloadData];
        wSelf.refreshing = NO;
        [wSelf.pullToRefreshView finishLoading];
    }];
}

- (SYSaneOptionGroup *)optionGroupForTableViewSection:(NSUInteger)section
{
    return self.device.groupedOptionsWithoutCrop[section-1];
}

- (SYSaneOption *)optionForTableViewIndexPath:(NSIndexPath *)indexPath
{
    return self.device.groupedOptionsWithoutCrop[indexPath.section-1].items[indexPath.row];
}

#pragma mark - IBActions

- (void)buttonScanTap:(id)sender
{
    __block DLAVAlertView *alertView;
    __block UIImageView *alertViewImageView;
    __block UIImage *completeImage;
    
    [SVProgressHUD showWithStatus:@"Loading..."];
    [[SYSaneHelper shared] scanWithDevice:self.device progressBlock:^(float progress, UIImage *incompleteImage) {
        if (!alertView && incompleteImage)
        {
            alertView = [[DLAVAlertView alloc] initWithTitle:@"Scanned image"
                                                     message:nil
                                                    delegate:nil
                                           cancelButtonTitle:@"Close"
                                           otherButtonTitles:@"Share", nil];
            
            alertViewImageView = [alertView addImageViewForImage:incompleteImage];
            [alertView showWithCompletion:^(DLAVAlertView *alertView, NSInteger buttonIndex) {
                if (buttonIndex == alertView.cancelButtonIndex)
                    return;
                
                UIActivityViewController *activityViewController =
                [[UIActivityViewController alloc] initWithActivityItems:@[completeImage]
                                                  applicationActivities:nil];
                [self presentViewController:activityViewController animated:YES completion:nil];
            }];
            [alertView setButtonsEnabled:NO];
            [SVProgressHUD dismiss];
        }
        else
        {
            [SVProgressHUD showProgress:progress];
        }
        
        if (incompleteImage)
            [alertViewImageView setImage:incompleteImage];
        
    } successBlock:^(UIImage *image, NSString *error) {
        completeImage = image;
        if (error)
        {
            [alertView dismissWithClickedButtonIndex:alertView.cancelButtonIndex animated:NO];
            [SVProgressHUD showErrorWithStatus:error];
        }
        else
        {
            [SVProgressHUD dismiss];
            
            if (!alertView)
            {
                alertView = [[DLAVAlertView alloc] initWithTitle:@"Scanned image"
                                                         message:nil
                                                        delegate:nil
                                               cancelButtonTitle:@"Close"
                                               otherButtonTitles:@"Share", nil];
                
                alertViewImageView = [alertView addImageViewForImage:completeImage];
                [alertView showWithCompletion:^(DLAVAlertView *alertView, NSInteger buttonIndex) {
                    if (buttonIndex == alertView.cancelButtonIndex)
                        return;
                    
                    UIActivityViewController *activityViewController =
                    [[UIActivityViewController alloc] initWithActivityItems:@[completeImage]
                                                      applicationActivities:nil];
                    [self presentViewController:activityViewController animated:YES completion:nil];
                }];
            }
            
            [alertViewImageView setImage:completeImage];
            [alertView setButtonsEnabled:YES];
        }
    }];
}

- (void)buttonSettingsTap:(id)sender
{
    [SYPrefVC showOnVC:self closeBlock:^{
        [self.tableView reloadData];
    }];
}

#pragma mark - UITableViewDataSource

- (NSInteger)numberOfSectionsInTableView:(UITableView *)tableView
{
    if (!self.device.allOptions.count)
        return 0;
    
    return self.device.groupedOptionsWithoutCrop.count + 1;
}

- (NSInteger)tableView:(UITableView *)tableView numberOfRowsInSection:(NSInteger)section
{
    if (section == 0)
        return 1;
    
    SYSaneOptionGroup *group = [self optionGroupForTableViewSection:section];
    
    if (group.containsOnlyAdvancedOptions && ![[SYPreferences shared] showAdvancedOptions])
        return 0;
    
    return group.items.count;
}

- (UITableViewCell *)tableView:(UITableView *)tableView cellForRowAtIndexPath:(NSIndexPath *)indexPath
{
    if (indexPath.section == 0)
    {
        SYPreviewCell *cell = (SYPreviewCell*)[tableView dequeueReusableCellWithIdentifier:@"SYPreviewCell"];
        [cell setDevice:self.device];
        return cell;
    }
    else
    {
        SYOptionCell *cell = (SYOptionCell*)[tableView dequeueReusableCellWithIdentifier:@"SYOptionCell"];
        SYSaneOption *opt = [self optionForTableViewIndexPath:indexPath];
        
        [cell setOption:opt];
        [cell setShowDescription:NO];
        
        return cell;
    }
}

- (NSString *)tableView:(UITableView *)tableView titleForHeaderInSection:(NSInteger)section
{
    if (section == 0)
        return @"PREVIEW";
    
    SYSaneOptionGroup *group = [self optionGroupForTableViewSection:section];
    
    if (group.containsOnlyAdvancedOptions && ![[SYPreferences shared] showAdvancedOptions])
        return [group.title stringByAppendingString:@" (ADVANCED)"];
    
    return group.title;
}

- (CGFloat)tableView:(UITableView *)tableView heightForRowAtIndexPath:(NSIndexPath *)indexPath
{
    CGFloat width = tableView.bounds.size.width;
    
    if (indexPath.section == 0)
        return [SYPreviewCell cellHeightForDevice:self.device width:width];
    
    SYSaneOption *opt = [self optionForTableViewIndexPath:indexPath];
    if (opt.capAdvanced && ![[SYPreferences shared] showAdvancedOptions])
        return 0.;
    
    return [SYOptionCell cellHeightForOption:opt showDescription:NO width:width];
}

#pragma mark - UITableViewDelegate

- (void)tableView:(UITableView *)tableView didSelectRowAtIndexPath:(NSIndexPath *)indexPath
{
    [tableView deselectRowAtIndexPath:indexPath animated:YES];
    
    if (indexPath.section == 0)
        return;
    
    SYSaneOption *opt = [self optionForTableViewIndexPath:indexPath];
    
    [SYSaneOptionUI showDetailsAndInputForOption:opt block:^(BOOL reloadAllOptions, NSString *error) {
        if (reloadAllOptions)
        {
            [[SYSaneHelper shared] listOptionsForDevice:self.device block:^ {
                [self.tableView reloadData];
                if (error)
                    [SVProgressHUD showErrorWithStatus:error];
                else
                    [SVProgressHUD showSuccessWithStatus:nil];
            }];
            return;
        }
        
        [self.tableView reloadData];
        if (error)
            [SVProgressHUD showErrorWithStatus:error];
        else
            [SVProgressHUD showSuccessWithStatus:nil];
    }];
}

#pragma mark - SSPullToRefreshViewDelegate

- (void)pullToRefreshViewDidStartLoading:(SSPullToRefreshView *)view
{
    [self refresh];
}

@end
