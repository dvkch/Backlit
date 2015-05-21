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

@interface SYDeviceVC () <UITableViewDataSource, UITableViewDelegate, SSPullToRefreshViewDelegate>
@property (nonatomic, strong) SSPullToRefreshView *pullToRefreshView;
@property (nonatomic, strong) UITableView *tableView;
@property (atomic, assign) BOOL refreshing;
@end

@implementation SYDeviceVC

- (void)loadView
{
    [super loadView];
    
    self.tableView = [[UITableView alloc] initWithFrame:self.view.bounds style:UITableViewStyleGrouped];
    [self.tableView setAutoresizingMask:(UIViewAutoresizingFlexibleHeight | UIViewAutoresizingFlexibleWidth)];
    [self.tableView setDelegate:self];
    [self.tableView setDataSource:self];
    [self.view addSubview:self.tableView];
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

#pragma mark - UITableViewDataSource

- (NSInteger)numberOfSectionsInTableView:(UITableView *)tableView
{
    return [self.device.groupedOptions count];
}

- (NSInteger)tableView:(UITableView *)tableView numberOfRowsInSection:(NSInteger)section
{
    return [(SYSaneOptionDescriptor *)[self.device.groupedOptions objectAtIndex:section] groupChildren].count;
}

- (UITableViewCell *)tableView:(UITableView *)tableView cellForRowAtIndexPath:(NSIndexPath *)indexPath
{
    SYOptionCell *cell = (SYOptionCell*)[tableView dequeueReusableCellWithIdentifier:@"cell"];
    SYSaneOptionDescriptor *group = [self.device.groupedOptions objectAtIndex:indexPath.section];
    SYSaneOptionDescriptor *opt = [group.groupChildren objectAtIndex:indexPath.row];
    
    if(opt.constraintType == SANE_CONSTRAINT_NONE)
        [cell setBackgroundColor:[UIColor redColor]];
    else
        [cell setBackgroundColor:[UIColor whiteColor]];
    
    
    if(!cell)
        cell = [[SYOptionCell alloc] initWithStyle:UITableViewCellStyleDefault reuseIdentifier:@"cell"];
    
    [cell setOption:opt];
    return cell;
}

- (NSString *)tableView:(UITableView *)tableView titleForHeaderInSection:(NSInteger)section
{
    return [(SYSaneOptionDescriptor *)[self.device.groupedOptions objectAtIndex:section] title];
}

#pragma mark - UITableViewDelegate

- (void)tableView:(UITableView *)tableView didSelectRowAtIndexPath:(NSIndexPath *)indexPath
{
    [tableView deselectRowAtIndexPath:indexPath animated:YES];
    SYSaneOptionDescriptor *group = [self.device.groupedOptions objectAtIndex:indexPath.section];
    SYSaneOptionDescriptor *opt = [group.groupChildren objectAtIndex:indexPath.row];
    NSLog(@"%@: %@", opt.name, [opt descriptionHuman]);
}

#pragma mark - SSPullToRefreshViewDelegate

- (void)pullToRefreshViewDidStartLoading:(SSPullToRefreshView *)view
{
    [self refresh];
}


@end
