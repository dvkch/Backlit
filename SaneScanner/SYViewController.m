//
//  SYViewController.m
//  SaneScanner
//
//  Created by rominet on 06/05/15.
//  Copyright (c) 2015 Syan. All rights reserved.
//

#import "SYViewController.h"
#import "SYTools.h"
#import "SYSaneHelper.h"

@interface SYViewController () <UITableViewDataSource, UITableViewDelegate, UIAlertViewDelegate>
@property (nonatomic, weak) IBOutlet UITableView *tableView;
@property (nonatomic, weak) IBOutlet UIBarButtonItem *buttonAddHost;
@property (nonatomic, weak) IBOutlet UIBarButtonItem *buttonEditHosts;
@property (nonatomic, strong) NSArray *devices;
@property (nonatomic, strong) NSArray *hosts;
@end

@implementation SYViewController

- (void)viewWillAppear:(BOOL)animated
{
    [super viewWillAppear:animated];
    self.hosts = [NSArray arrayWithContentsOfFile:[SYTools hostsFile]];
    [self updateDevices];
}

- (void)updateDevices
{
    [SYSaneHelper listDevices:^(NSArray *devices, NSString *error) {
        if(error)
        {
            NSString *errorMessage = [NSString stringWithFormat:@"Error while listing devices: %@", error];
            [[[UIAlertView alloc] initWithTitle:nil message:errorMessage delegate:nil cancelButtonTitle:nil otherButtonTitles:@"Close", nil] show];
        }
        else
        {
            self.devices = [devices copy];
            [self.tableView reloadData];
        }
    }];
}

- (void)defineNewHosts:(NSArray *)hosts
{
    [hosts writeToFile:[SYTools hostsFile] atomically:YES];
    [SYSaneHelper updateSaneNetConfig];
    
    self.hosts = [hosts copy];
    [self.tableView reloadData];
    
    [self updateDevices];
}

#pragma mark - IBActions

- (IBAction)buttonAddHostTap:(id)sender
{
    UIAlertView *av = [[UIAlertView alloc] initWithTitle:@"Add a host"
                                                 message:@"Enter the hostname or IP address for the new host"
                                                delegate:self
                                       cancelButtonTitle:@"Cancel"
                                       otherButtonTitles:@"Add", nil];
    [av setAlertViewStyle:UIAlertViewStylePlainTextInput];
    [av show];
}

- (IBAction)buttonEditHostsTap:(id)sender
{
    [self.tableView setEditing:!self.tableView.editing];
}

#pragma mark - UIAlertViewDelegate

- (void)alertView:(UIAlertView *)alertView clickedButtonAtIndex:(NSInteger)buttonIndex
{
    if(buttonIndex == [alertView cancelButtonIndex])
        return;
    
    NSString *host = [[alertView textFieldAtIndex:0] text];
    if(![host length])
        return;
    
    NSArray *hosts = self.hosts ?: @[];
    hosts = [hosts arrayByAddingObject:host];
    [self defineNewHosts:hosts];
}

#pragma mark - UITableViewDataSource

- (NSInteger)numberOfSectionsInTableView:(UITableView *)tableView
{
    return 2;
}

- (NSInteger)tableView:(UITableView *)tableView numberOfRowsInSection:(NSInteger)section
{
    return section == 0 ? [self.hosts count] : [self.devices count];
}

- (UITableViewCell *)tableView:(UITableView *)tableView cellForRowAtIndexPath:(NSIndexPath *)indexPath
{
    UITableViewCell *cell = [tableView dequeueReusableCellWithIdentifier:@"cell"];
    if(!cell)
        cell = [[UITableViewCell alloc] initWithStyle:UITableViewCellStyleDefault reuseIdentifier:@"cell"];
    
    NSString *text;
    if(indexPath.section == 0)
        text = self.hosts[indexPath.row];
    else
        text = self.devices[indexPath.row];
    
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
    
    NSMutableArray *hosts = [self.hosts mutableCopy];
    [hosts removeObjectAtIndex:indexPath.row];
    
    [self defineNewHosts:hosts];
    
    if(![self.hosts count])
        [self.tableView setEditing:NO];
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

@end
