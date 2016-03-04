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
#import "SYSaneOptionDescriptor.h"
#import "SSPullToRefresh.h"
#import "SYOptionCell.h"
#import <CustomIOSAlertView.h>
#import <SVProgressHUD.h>

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
    UIImageView *imageView = [[UIImageView alloc] initWithFrame:CGRectMake(0, 0, 300, 300)];
    [imageView setContentMode:UIViewContentModeScaleAspectFit];
    [imageView setBackgroundColor:[UIColor clearColor]];
    
    CustomIOSAlertView *alertView = [[CustomIOSAlertView alloc] init];
    [alertView setContainerView:imageView];
    [alertView setButtonTitles:@[@"Close"]];
    [alertView setOnButtonTouchUpInside:^(CustomIOSAlertView *alertView, int buttonIndex) {
        [alertView close];
    }];
    
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
            [imageView setImage:image];
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
    SYSaneOptionDescriptor *opt = self.device.groupedOptions[indexPath.section].items[indexPath.row];
    
    if(!cell)
        cell = [[SYOptionCell alloc] initWithStyle:UITableViewCellStyleDefault reuseIdentifier:@"cell"];
    
    [cell setOption:opt];

    if(opt.readOnlyOrSingleOption)
        [cell setBackgroundColor:[UIColor colorWithWhite:0.92 alpha:1.]];
    else
        [cell setBackgroundColor:[UIColor whiteColor]];

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
    SYSaneOptionDescriptor *opt = self.device.groupedOptions[indexPath.section].items[indexPath.row];
    if (opt.capAdvanced && !self.showAdvanced)
        return 0.;
    return 44.;
}

#pragma mark - UITableViewDelegate

- (void)tableView:(UITableView *)tableView didSelectRowAtIndexPath:(NSIndexPath *)indexPath
{
    [tableView deselectRowAtIndexPath:indexPath animated:YES];
    SYSaneOptionDescriptorGroup *group = self.device.groupedOptions[indexPath.section];
    SYSaneOptionDescriptor *opt = group.items[indexPath.row];
    NSLog(@"%@: %@", opt.name, [opt descriptionHuman]);
}

#pragma mark - SSPullToRefreshViewDelegate

- (void)pullToRefreshViewDidStartLoading:(SSPullToRefreshView *)view
{
    [self refresh];
}

@end
