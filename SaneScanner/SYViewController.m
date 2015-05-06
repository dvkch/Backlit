//
//  SYViewController.m
//  SaneScanner
//
//  Created by rominet on 06/05/15.
//  Copyright (c) 2015 Syan. All rights reserved.
//

#import "SYViewController.h"
#import "SYTools.h"
#import "sane.h"

@interface SYViewController () <UITableViewDataSource, UITableViewDelegate, UIAlertViewDelegate>
@property (nonatomic, weak) IBOutlet UITableView *tableView;
@property (nonatomic, weak) IBOutlet UIBarButtonItem *buttonAddHost;
@property (nonatomic, strong) NSArray *devices;
@property (nonatomic, strong) NSArray *hosts;
@end

@implementation SYViewController

- (void)viewWillAppear:(BOOL)animated
{
    [super viewWillAppear:animated];
    self.hosts = [NSArray arrayWithContentsOfFile:[SYTools pathForFile:@"hosts.cfg"]];
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

#pragma mark - UIAlertViewDelegate

- (void)alertView:(UIAlertView *)alertView clickedButtonAtIndex:(NSInteger)buttonIndex
{
    if(buttonIndex == [alertView cancelButtonIndex])
        return;
    
    NSString *host = [[alertView textFieldAtIndex:0] text];
    if(![host length])
        return;
    
    NSMutableArray *hosts = [NSMutableArray arrayWithArray:self.hosts];
    [hosts addObject:host];
    
    self.hosts = [hosts copy];
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

@end
/*
NSString *docs = [[self class] applicationDocumentsDirectory];
[[NSFileManager defaultManager] changeCurrentDirectoryPath:docs];
[@"connect_timeout = 30\n192.168.1.42" writeToFile:[docs stringByAppendingPathComponent:@"net.conf"] atomically:YES encoding:NSUTF8StringEncoding error:NULL];

NSLog(@"%@", [[NSFileManager defaultManager] currentDirectoryPath]);

// Override point for customization after application launch.
dispatch_async(dispatch_get_main_queue(), ^
               {
                   SANE_Int version;
                   SANE_Status s = sane_init(&version, NULL);
                   if(s == SANE_STATUS_GOOD)
                       NSLog(@"yo! %d", version);
                   
                   const SANE_Device **devices = NULL;
                   SANE_Handle handle;
                   //sane_open("net:192.168.1.42", &handle);
                   sane_get_devices(&devices, SANE_FALSE);
                   uint i = 0;
                   while(devices[i]) {
                       const SANE_Device *d = devices[i];
                       NSLog(@"Device %d: %s %s %s (%s)", i, d->name, d->vendor, d->model, d->type);
                       ++i;
                   }
                   
                   
                   
                   sane_open("net:192.168.1.42", &handle);
                   
                   if (s == SANE_STATUS_GOOD)
                       NSLog(@"opened");
                   else
                       NSLog(@"erro %s", sane_strstatus(s));
                   
                   NSLog(@"%d devices", i);
               });
*/