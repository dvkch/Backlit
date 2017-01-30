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
#import "UIViewController+SYKit.h"

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
    [self.tableView registerClass:[SYPreviewCell class] forCellReuseIdentifier:[SYPreviewCell sy_className]];
    [self.tableView registerClass:[SYOptionCell  class] forCellReuseIdentifier:[SYOptionCell sy_className]];
    [self.view addSubview:self.tableView];
    
    self.buttonScan = [UIButton buttonWithType:UIButtonTypeCustom];
    [self.buttonScan addTarget:self action:@selector(buttonScanTap:) forControlEvents:UIControlEventTouchUpInside];
    [self.buttonScan setBackgroundColor:[UIColor vividBlueColor]];
    [self.buttonScan setTitleColor:[UIColor whiteColor] forState:UIControlStateNormal];
    [self.buttonScan setTitle:$("ACTION SCAN").uppercaseString forState:UIControlStateNormal];
    [self.buttonScan.titleLabel setFont:[UIFont systemFontOfSize:17]];
    [self.view addSubview:self.buttonScan];
    
    self.thumbsView = [SYGalleryThumbsView showInToolbarOfController:self tintColor:[UIColor vividBlueColor]];
    
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
    [SVProgressHUD showWithStatus:$("SCANNING")];
    [[SYSaneHelper shared] scanWithDevice:self.device progressBlock:^(float progress, UIImage *incompleteImage) {
        [SVProgressHUD showProgress:progress];
    } successBlock:^(UIImage *image, NSError *error) {
        
        if (error)
            [SVProgressHUD showErrorWithStatus:error.sy_alertMessage];
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
    __block MHGalleryItem *item;
    
    void(^block)(float progress, BOOL finished, UIImage *image, NSError *error) =
    ^(float progress, BOOL finished, UIImage *image, NSError *error)
    {
        // Finished with error
        if (error)
        {
            [alertView dismissWithClickedButtonIndex:alertView.cancelButtonIndex animated:NO];
            [SVProgressHUD showErrorWithStatus:error.sy_alertMessage];
            return;
        }
        
        // Finished without error
        if (finished)
        {
            item = [[SYGalleryManager shared] addImage:image];
            [SVProgressHUD dismiss];
        }
        
        // need to show image (finished or partial with preview)
        if (!alertView && image)
        {
            alertView = [[DLAVAlertView alloc] initWithTitle:$("DIALOG TITLE SCANNED IMAGE")
                                                     message:nil
                                                    delegate:nil
                                           cancelButtonTitle:$("ACTION CLOSE")
                                           otherButtonTitles:$("ACTION SHARE"), nil];
            
            alertViewImageView = [alertView addImageViewForImage:image];
            [alertView showWithCompletion:^(DLAVAlertView *alertView, NSInteger buttonIndex)
            {
                if (buttonIndex == alertView.cancelButtonIndex)
                    return;
                
                [self shareItem:item];
            }];
            [SVProgressHUD dismiss];
        }
        
        // update alertview
        [alertView setButtonsEnabled:finished];
        
        // update image for partial preview
        if (image)
        {
            [alertViewImageView setImage:image];
        }
        
        // update progress when no partial preview
        if (!finished && !image)
        {
            [SVProgressHUD showProgress:progress];
        }
    };
    
    [SVProgressHUD showWithStatus:$("SCANNING")];
    [[SYSaneHelper shared] scanWithDevice:self.device progressBlock:^(float progress, UIImage *incompleteImage) {
        block(progress, NO, incompleteImage, nil);
    } successBlock:^(UIImage *image, NSError *error) {
        block(1., YES, image, error);
    }];
}

- (void)buttonSettingsTap:(id)sender
{
    [SYPrefVC showOnVC:self closeBlock:^{
        [self.tableView reloadData];
    }];
}

- (void)shareItem:(MHGalleryItem *)item
{
    if (!item.URL)
        return;
        
    UIActivityViewController *activityViewController =
    [[UIActivityViewController alloc] initWithActivityItems:@[item.URL]
                                      applicationActivities:nil];
    
    UIViewController *sourceVC = self.splitViewController;
    
    UIPopoverPresentationController *popover = activityViewController.popoverPresentationController;
    [popover setPermittedArrowDirections:0];
    [popover setSourceView:sourceVC.view];
    [popover setSourceRect:CGRectMake(CGRectGetMidX(sourceVC.view.frame) - .5,
                                      CGRectGetMaxY(sourceVC.view.frame) - 1., 1, 1)];
    
    [sourceVC presentViewController:activityViewController animated:YES completion:nil];
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
        SYPreviewCell *cell = (SYPreviewCell*)[tableView dequeueReusableCellWithIdentifier:[SYPreviewCell sy_className]];
        [cell setDevice:self.device];
        return cell;
    }
    else
    {
        SYOptionCell *cell = (SYOptionCell*)[tableView dequeueReusableCellWithIdentifier:[SYOptionCell sy_className]];
        SYSaneOption *opt = [self optionForTableViewIndexPath:indexPath];
        
        [cell setOption:opt];
        [cell setShowDescription:NO];
        
        return cell;
    }
}

- (NSString *)tableView:(UITableView *)tableView titleForHeaderInSection:(NSInteger)section
{
    if (section == 0)
        return $("DEVICE SECTION PREVIEW");
    
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
    
    [SYSaneOptionUI showDetailsAndInputForOption:opt block:^(BOOL reloadAllOptions, NSError *error) {
        if (reloadAllOptions)
        {
            [[SYSaneHelper shared] listOptionsForDevice:self.device block:^ {
                [self.tableView reloadData];
                if (error)
                    [SVProgressHUD showErrorWithStatus:error.sy_alertMessage];
                else
                    [SVProgressHUD showSuccessWithStatus:nil];
            }];
            return;
        }
        
        [self.tableView reloadData];
        if (error)
            [SVProgressHUD showErrorWithStatus:error.sy_alertMessage];
        else
            [SVProgressHUD showSuccessWithStatus:nil];
    }];
}

@end
