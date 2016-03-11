//
//  SYPrefVC.m
//  SaneScanner
//
//  Created by Stan Chevallier on 11/03/2016.
//  Copyright © 2016 Syan. All rights reserved.
//

#import "SYPrefVC.h"
#import <Masonry.h>
#import "SYPreferences.h"
#import "SYOptionCell.h"
#import <UIImage+SYKit.h>
#import "UIImage+SY.h"

@interface SYPrefVC () <UITableViewDataSource, UITableViewDelegate>

@property (nonatomic, strong) UITableView *tableView;
@property (nonatomic, strong) UIBarButtonItem *buttonClose;
@property (nonatomic, strong) NSArray<SYPair<NSString *, NSArray<NSString *> *> *> *keysGroups;
@property (nonatomic, copy) void(^closeBlock)(void);

@end

@implementation SYPrefVC

+ (UIBarButtonItem *)barButtonItemWithTarget:(id)target action:(SEL)action
{
    UIBarButtonItem *button = [[UIBarButtonItem alloc] initWithImage:[UIImage imageNamed:@"settings"]
                                                               style:UIBarButtonItemStyleBordered
                                                              target:target action:action];
    [button setTintColor:[UIColor grayColor]];
    return button;
}

+ (void)showOnVC:(UIViewController *)vc closeBlock:(void(^)(void))closeBlock
{
    SYPrefVC *prefVC = [[self alloc] init];
    [prefVC setCloseBlock:closeBlock];
    
    UINavigationController *nc = [[UINavigationController alloc] initWithRootViewController:prefVC];
    [vc presentViewController:nc animated:YES completion:nil];
}

- (void)viewDidLoad
{
    [super viewDidLoad];
    [self setTitle:@"Preferences"];
    [self.view setBackgroundColor:[UIColor whiteColor]];
    
    self.tableView = [[UITableView alloc] initWithFrame:self.view.bounds style:UITableViewStyleGrouped];
    [self.tableView setDataSource:self];
    [self.tableView setDelegate:self];
    [self.tableView registerClass:[SYOptionCell class] forCellReuseIdentifier:@"SYOptionCell"];
    [self.tableView setAutoresizingMask:(UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight)];
    [self.view addSubview:self.tableView];
 
    self.buttonClose = [[UIBarButtonItem alloc] initWithBarButtonSystemItem:UIBarButtonSystemItemStop
                                                                     target:self action:@selector(buttonCloseTap:)];
    [self.buttonClose setTintColor:[UIColor grayColor]];
    [self.navigationItem setRightBarButtonItem:self.buttonClose];
    
    self.keysGroups = [[SYPreferences shared] allKeysGrouped];
}

#pragma mark - IBActions

- (void)buttonCloseTap:(id)sender
{
    if (self.closeBlock)
        self.closeBlock();
    
    [self dismissViewControllerAnimated:YES completion:nil];
}

#pragma mark - TableView

- (NSInteger)numberOfSectionsInTableView:(UITableView *)tableView
{
    return self.keysGroups.count;
}

- (NSInteger)tableView:(UITableView *)tableView numberOfRowsInSection:(NSInteger)section
{
    return self.keysGroups[section].object2.count;
}

- (UITableViewCell *)tableView:(UITableView *)tableView cellForRowAtIndexPath:(NSIndexPath *)indexPath
{
    NSString *prefKey = self.keysGroups[indexPath.section].object2[indexPath.row];
    SYOptionCell *cell = (SYOptionCell *)[tableView dequeueReusableCellWithIdentifier:@"SYOptionCell"];
    [cell setPrefKey:prefKey];
    [cell setShowDescription:YES];
    return cell;
}

- (NSString *)tableView:(UITableView *)tableView titleForHeaderInSection:(NSInteger)section
{
    return self.keysGroups[section].object1;
}

- (CGFloat)tableView:(UITableView *)tableView heightForRowAtIndexPath:(NSIndexPath *)indexPath
{
    NSString *prefKey = self.keysGroups[indexPath.section].object2[indexPath.row];
    return [SYOptionCell cellHeightForPrefKey:prefKey showDescription:YES width:tableView.bounds.size.width];
}

- (void)tableView:(UITableView *)tableView didSelectRowAtIndexPath:(NSIndexPath *)indexPath
{
    [tableView deselectRowAtIndexPath:indexPath animated:YES];
    
    NSString *prefKey = self.keysGroups[indexPath.section].object2[indexPath.row];
    switch ([[SYPreferences shared] typeForKey:prefKey]) {
        case SYPreferenceTypeBool:
        {
            BOOL oldValue = [[[SYPreferences shared] objectForKey:prefKey] boolValue];
            [[SYPreferences shared] setObject:@(!oldValue) forKey:prefKey];
            break;
        }
        default:
            break;
    }
    
    [tableView beginUpdates];
    [tableView reloadRowsAtIndexPaths:@[indexPath] withRowAnimation:UITableViewRowAnimationAutomatic];
    [tableView endUpdates];
}

@end
