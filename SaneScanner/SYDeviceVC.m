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
#import "SYSaneOptionInt.h"
#import "SYSaneOptionDouble.h"
#import "SYSaneOptionString.h"
#import "SYSaneOptionButton.h"
#import "SYSaneOptionGroup.h"
#import "SSPullToRefresh.h"
#import "SYOptionCell.h"
#import <SVProgressHUD.h>
#import "DLAVAlertView+SY.h"
#import "SYSaneOptionUI.h"
#import "SYTools.h"

@interface SYDeviceVC () <UITableViewDataSource, UITableViewDelegate, SSPullToRefreshViewDelegate>
@property (nonatomic, strong) SSPullToRefreshView *pullToRefreshView;
@property (nonatomic, strong) UITableView *tableView;
@property (nonatomic, strong) UIBarButtonItem *buttonToggleAdvanced;
@property (nonatomic, strong) UIButton *buttonScan;

@property (nonatomic, assign) BOOL showAdvanced;
@property (nonatomic, assign) BOOL refreshing;
@end

@implementation SYDeviceVC

- (void)viewDidLoad
{
    [super viewDidLoad];
    
    CGFloat w = self.view.bounds.size.width;
    CGFloat h = self.view.bounds.size.height;
    CGFloat buttonHeight = 60;
    
    self.tableView = [[UITableView alloc] initWithFrame:CGRectMake(0, 0, w, h-buttonHeight) style:UITableViewStyleGrouped];
    [self.tableView setAutoresizingMask:(UIViewAutoresizingFlexibleHeight | UIViewAutoresizingFlexibleWidth)];
    [self.tableView setDelegate:self];
    [self.tableView setDataSource:self];
    [self.view addSubview:self.tableView];
    
    self.buttonScan = [UIButton buttonWithType:UIButtonTypeSystem];
    [self.buttonScan addTarget:self action:@selector(buttonScanTap:) forControlEvents:UIControlEventTouchUpInside];
    [self.buttonScan setTitle:@"Scan" forState:UIControlStateNormal];
    [self.buttonScan setFrame:CGRectMake(0, h-buttonHeight, w, buttonHeight)];
    [self.tableView setAutoresizingMask:(UIViewAutoresizingFlexibleTopMargin | UIViewAutoresizingFlexibleWidth)];
    [self.view addSubview:self.buttonScan];
    
    self.buttonToggleAdvanced = [[UIBarButtonItem alloc] initWithTitle:@""
                                                                 style:UIBarButtonItemStylePlain
                                                                target:self
                                                                action:@selector(buttonToggleAdvancedTap:)];
    [self.navigationItem setRightBarButtonItem:self.buttonToggleAdvanced];
    
    self.showAdvanced = NO;
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

- (void)setShowAdvanced:(BOOL)showAdvanced
{
    self->_showAdvanced = showAdvanced;
    [self.tableView reloadData];
    [self.buttonToggleAdvanced setTitle:(showAdvanced ? @"Expert Mode" : @"Normal Mode")];
}

- (void)buttonScanTap:(id)sender
{
    [SVProgressHUD showWithStatus:@"Loading..."];
    [[SYSaneHelper shared] scanWithDevice:self.device progressBlock:^(float progress) {
        [SVProgressHUD showProgress:progress];
    } successBlock:^(UIImage *image, NSString *error) {
        if (error)
        {
            [SVProgressHUD showErrorWithStatus:error];
        }
        else
        {
            [SVProgressHUD dismiss];
            
            NSData *imageData = UIImagePNGRepresentation(image);
            NSString *path = [[SYTools documentsPath] stringByAppendingPathComponent:@"image.png"];
            [imageData writeToFile:path atomically:YES];
            NSLog(@"%@", path);
            
            DLAVAlertView *alertView = [[DLAVAlertView alloc] initWithTitle:@"Scanned image"
                                                                    message:nil
                                                                   delegate:nil
                                                          cancelButtonTitle:nil
                                                          otherButtonTitles:nil];
            [alertView setTitle:@"Scanned image"];
            [alertView addButtonWithTitle:@"Close"];
            [alertView addImageViewForImage:image];
            [alertView show];
        }
    }];
}

- (void)buttonToggleAdvancedTap:(id)sender
{
    self.showAdvanced = !self.showAdvanced;
}

#pragma mark - UITableViewDataSource

- (NSInteger)numberOfSectionsInTableView:(UITableView *)tableView
{
    return self.device.groupedOptions.count;
}

- (NSInteger)tableView:(UITableView *)tableView numberOfRowsInSection:(NSInteger)section
{
    if (self.device.groupedOptions[section].containsOnlyAdvancedOptions && !self.showAdvanced)
        return 0;
    
    return self.device.groupedOptions[section].items.count;
}

- (UITableViewCell *)tableView:(UITableView *)tableView cellForRowAtIndexPath:(NSIndexPath *)indexPath
{
    SYOptionCell *cell = (SYOptionCell*)[tableView dequeueReusableCellWithIdentifier:@"cell"];
    SYSaneOption *opt = self.device.groupedOptions[indexPath.section].items[indexPath.row];
    
    if(!cell)
        cell = [[SYOptionCell alloc] initWithStyle:UITableViewCellStyleDefault reuseIdentifier:@"cell"];
    
    [cell setOption:opt];
    [cell setShowDescription:NO];

    return cell;
}

- (NSString *)tableView:(UITableView *)tableView titleForHeaderInSection:(NSInteger)section
{
    if (self.device.groupedOptions[section].containsOnlyAdvancedOptions && !self.showAdvanced)
    {
        return [self.device.groupedOptions[section].title stringByAppendingString:@" (ADVANCED)"];
    }
    
    return self.device.groupedOptions[section].title;
}

- (CGFloat)tableView:(UITableView *)tableView heightForRowAtIndexPath:(NSIndexPath *)indexPath
{
    SYSaneOption *opt = self.device.groupedOptions[indexPath.section].items[indexPath.row];
    if (opt.capAdvanced && !self.showAdvanced)
        return 0.;
    
    return [SYOptionCell cellHeightForOption:opt showDescription:NO width:tableView.frame.size.width];
}

#pragma mark - UITableViewDelegate

- (void)tableView:(UITableView *)tableView didSelectRowAtIndexPath:(NSIndexPath *)indexPath
{
    [tableView deselectRowAtIndexPath:indexPath animated:YES];
    SYSaneOptionGroup *group = self.device.groupedOptions[indexPath.section];
    SYSaneOption *opt = group.items[indexPath.row];
    
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
