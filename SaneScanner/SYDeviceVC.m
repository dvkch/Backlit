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
#import "SYOptionCell.h"
#import "SYPreviewCell.h"
#import "SVProgressHUD.h"
#import "DLAVAlertView+SY.h"
#import "SYSaneOptionUI.h"
#import "SYTools.h"
#import <Masonry.h>
#import "SYPrefVC.h"
#import "SYPreferences.h"
#import "SYGalleryManager.h"
#import "SYGalleryThumbsView.h"
#import "UIColor+SY.h"
#import "SYAppDelegate.h"
#import <UIImage+SYKit.h>
#import "SYGalleryController.h"
#import "SYRefreshControl.h"

@interface SYDeviceVC () <UITableViewDataSource, UITableViewDelegate>
@property (nonatomic, strong) SYGalleryThumbsView *thumbsView;
@property (nonatomic, strong) UITableView *tableView;
@property (nonatomic, strong) UIButton *buttonScan;
@property (nonatomic, assign) BOOL refreshing;
@end

@implementation SYDeviceVC

- (void)viewDidLoad
{
    [super viewDidLoad];
    [self.view setBackgroundColor:[UIColor groupTableViewBackgroundColor]];
    
    // can't reenable swipe back because we wouldn't get the possiblity to close the device once it's not needed
    //[self.navigationController.interactivePopGestureRecognizer setDelegate:nil];
    
    self.tableView = [[UITableView alloc] initWithFrame:CGRectZero style:UITableViewStyleGrouped];
    [self.tableView setDelegate:self];
    [self.tableView setDataSource:self];
    [self.tableView registerClass:[SYPreviewCell class] forCellReuseIdentifier:@"SYPreviewCell"];
    [self.tableView registerClass:[SYOptionCell  class] forCellReuseIdentifier:@"SYOptionCell"];
    [self.view addSubview:self.tableView];
    
    self.buttonScan = [UIButton buttonWithType:UIButtonTypeCustom];
    [self.buttonScan addTarget:self action:@selector(buttonScanTap:) forControlEvents:UIControlEventTouchUpInside];
    [self.buttonScan setBackgroundColor:[UIColor vividBlueColor]];
    [self.buttonScan setTitleColor:[UIColor whiteColor] forState:UIControlStateNormal];
    [self.buttonScan setTitle:@"SCAN" forState:UIControlStateNormal];
    [self.buttonScan.titleLabel setFont:[UIFont systemFontOfSize:17]];
    [self.view addSubview:self.buttonScan];
    
    self.thumbsView = [SYGalleryThumbsView showInToolbarOfController:self tintColor:[UIColor vividBlueColor]];
    
    [self.navigationItem setLeftBarButtonItem:
     [[UIBarButtonItem alloc] initWithImage:[UIImage imageNamed:@"back"]
                                      style:UIBarButtonItemStyleDone
                                     target:self action:@selector(backButtonTap:)]];
    
    [self.navigationItem setRightBarButtonItem:
     [SYPrefVC barButtonItemWithTarget:self action:@selector(buttonSettingsTap:)]];
    
    [self.tableView mas_makeConstraints:^(MASConstraintMaker *make) {
        make.top.equalTo(@0);
        make.left.equalTo(@0);
        make.right.equalTo(@0);
    }];
    
    [self.buttonScan mas_makeConstraints:^(MASConstraintMaker *make) {
        make.top.equalTo(self.tableView.mas_bottom);
        make.height.equalTo(@44);
        make.left.equalTo(@0);
        make.right.equalTo(@0);
        make.bottom.equalTo(self.mas_bottomLayoutGuideTop);
    }];
    
    // adding pull to refresh
    __weak SYDeviceVC *wSelf = self;
    [SYRefreshControl addRefreshControlToScrollView:self.tableView triggerBlock:^(UIScrollView *scollView) {
        [wSelf refresh];
    }];

    // prevent tableView width to be 0 and have a constraint issue when computing cell size
    [self.view layoutIfNeeded];

    // initial refresh
    [self.tableView ins_beginPullToRefresh];
}

- (void)dealloc
{
    [[SYSaneHelper shared] closeDevice:self.device];
}

#pragma mark - Data

- (void)setDevice:(SYSaneDevice *)device
{
    self->_device = device;
    [self setTitle:self.device.model];
}

- (void)refresh
{
    if(self.refreshing)
        return;
    
    self.refreshing = YES;
    
    __weak SYDeviceVC *wSelf = self;
    [[SYSaneHelper shared] listOptionsForDevice:self.device block:^ {
        [wSelf.tableView reloadData];
        wSelf.refreshing = NO;
        [wSelf.tableView ins_endPullToRefresh];
    }];
}

- (NSArray <SYSaneOptionGroup *> *)optionGroups
{
    return [self.device filteredGroupedOptionsWithoutAdvanced:![[SYPreferences shared] showAdvancedOptions]];
}

- (SYSaneOptionGroup *)optionGroupForTableViewSection:(NSUInteger)section
{
    return self.optionGroups[section-1];
}

- (SYSaneOption *)optionForTableViewIndexPath:(NSIndexPath *)indexPath
{
    return self.optionGroups[indexPath.section-1].items[indexPath.row];
}

#pragma mark - IBActions

- (void)backButtonTap:(id)sender
{
    // prevent retain cycle
    [self setToolbarItems:@[]];
    [self setThumbsView:nil];
    
    [self.navigationController popViewControllerAnimated:YES];
}

- (void)buttonScanTap2:(id)sender
{
    [SVProgressHUD showWithStatus:@"Scanning..."];
    [[SYSaneHelper shared] scanWithDevice:self.device progressBlock:^(float progress, UIImage *incompleteImage) {
        [SVProgressHUD showProgress:progress];
    } successBlock:^(UIImage *image, NSString *error) {
        
        if (error)
            [SVProgressHUD showErrorWithStatus:error];
        else
        {
            [[SYGalleryManager shared] addImage:image];
            [SVProgressHUD showSuccessWithStatus:nil];
        }
    }];
}

- (void)buttonScanTap:(id)sender
{
    __block DLAVAlertView *alertView;
    __block UIImageView *alertViewImageView;
    __block UIImage *completeImage;
    
    [SVProgressHUD showWithStatus:@"Scanning..."];
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
    
    return self.optionGroups.count + 1;
}

- (NSInteger)tableView:(UITableView *)tableView numberOfRowsInSection:(NSInteger)section
{
    if (section == 0)
        return 1;
    
    return [self optionGroupForTableViewSection:section].items.count;
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
        return @"Preview";
    
    return [self optionGroupForTableViewSection:section].title;
}

- (CGFloat)tableView:(UITableView *)tableView heightForRowAtIndexPath:(NSIndexPath *)indexPath
{
    CGFloat width = tableView.bounds.size.width;
    CGFloat maxHeight = tableView.bounds.size.height * 2. / 3.;
    
    if (self.traitCollection.verticalSizeClass == UIUserInterfaceSizeClassCompact)
        maxHeight = 500;
    
    if (indexPath.section == 0)
        return [SYPreviewCell cellHeightForDevice:self.device width:width maxHeight:maxHeight];
    
    return [SYOptionCell cellHeightForOption:[self optionForTableViewIndexPath:indexPath]
                             showDescription:NO
                                       width:width];
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

@end
