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
#import "UIApplication+SY.h"
#import "SYEmailHelper.h"

static NSString * const kContactAddress = $$("contact@syan.me");

@interface SYPrefVC () <UITableViewDataSource, UITableViewDelegate>

@property (nonatomic, strong) UITableView *tableView;
@property (nonatomic, strong) UIBarButtonItem *buttonClose;
@property (nonatomic, strong) NSArray<SYPair<NSString *, NSArray<NSString *> *> *> *keysGroups;

@end

@implementation SYPrefVC

+ (UIBarButtonItem *)barButtonItemWithTarget:(id)target action:(SEL)action
{
    UIBarButtonItem *button = [[UIBarButtonItem alloc] initWithImage:[UIImage imageNamed:$$("settings")]
                                                               style:UIBarButtonItemStylePlain
                                                              target:target action:action];
    return button;
}

- (void)viewDidLoad
{
    [super viewDidLoad];
    [self setTitle:$("PREFERENCES TITLE")];
    [self.view setBackgroundColor:[UIColor groupTableViewBackgroundColor]];
    
    self.tableView = [[UITableView alloc] initWithFrame:self.view.bounds style:UITableViewStyleGrouped];
    [self.tableView setDataSource:self];
    [self.tableView setDelegate:self];
    [self.tableView registerClass:[SYOptionCell class] forCellReuseIdentifier:[SYOptionCell sy_className]];
    [self.tableView setAutoresizingMask:(UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight)];
    [self.view addSubview:self.tableView];
 
    self.buttonClose = [[UIBarButtonItem alloc] initWithBarButtonSystemItem:UIBarButtonSystemItemStop
                                                                     target:self action:@selector(buttonCloseTap:)];
    [self.navigationItem setRightBarButtonItem:self.buttonClose];
    
    self.keysGroups = [[SYPreferences shared] allKeysGrouped];
    
    [self.tableView mas_makeConstraints:^(MASConstraintMaker *make) {
        make.edges.equalTo(@0);
    }];
}

#pragma mark - IBActions

- (void)buttonCloseTap:(id)sender
{
    [self dismissViewControllerAnimated:YES completion:nil];
}

#pragma mark - TableView

- (NSInteger)numberOfSectionsInTableView:(UITableView *)tableView
{
    return self.keysGroups.count + 1;
}

- (NSInteger)tableView:(UITableView *)tableView numberOfRowsInSection:(NSInteger)section
{
    if (section < self.keysGroups.count)
        return self.keysGroups[section].object2.count;
    return 2;
}

- (UITableViewCell *)tableView:(UITableView *)tableView cellForRowAtIndexPath:(NSIndexPath *)indexPath
{
    SYOptionCell *cell = (SYOptionCell *)[tableView dequeueReusableCellWithIdentifier:[SYOptionCell sy_className]];
    if (indexPath.section < self.keysGroups.count)
    {
        NSString *prefKey = self.keysGroups[indexPath.section].object2[indexPath.row];
        [cell setPrefKey:prefKey];
        [cell setShowDescription:YES];
    }
    else
    {
        if (indexPath.row == 0)
        {
            [cell updateWithLeftText:$("PREFERENCES TITLE CONTACT")
                           rightText:kContactAddress];
        }
        else
        {
            [cell updateWithLeftText:$("PREFERENCES TITLE VERSION")
                           rightText:[UIApplication sy_appVersionAndBuild]];
        }
    }
    return cell;
}

- (NSString *)tableView:(UITableView *)tableView titleForHeaderInSection:(NSInteger)section
{
    if (section < self.keysGroups.count)
        return self.keysGroups[section].object1;
    return $("PREFERENCES SECTION ABOUT APP");
}

- (CGFloat)tableView:(UITableView *)tableView heightForRowAtIndexPath:(NSIndexPath *)indexPath
{
    if (indexPath.section < self.keysGroups.count)
    {
        NSString *prefKey = self.keysGroups[indexPath.section].object2[indexPath.row];
        return [SYOptionCell cellHeightForPrefKey:prefKey showDescription:YES width:tableView.bounds.size.width];
    }
    
    if (indexPath.row == 0)
        return [SYOptionCell cellHeightForLeftText:$("PREFERENCES TITLE CONTACT")
                                         rightText:kContactAddress
                                             width:tableView.bounds.size.width];
    else
        return [SYOptionCell cellHeightForLeftText:$("PREFERENCES TITLE VERSION")
                                         rightText:[UIApplication sy_appVersionAndBuild]
                                             width:tableView.bounds.size.width];

    return UITableViewAutomaticDimension;
}

- (void)tableView:(UITableView *)tableView didSelectRowAtIndexPath:(NSIndexPath *)indexPath
{
    [tableView deselectRowAtIndexPath:indexPath animated:YES];
    
    // ABOUT APP section
    if (indexPath.section >= self.keysGroups.count)
    {
        if (indexPath.row == 0)
        {
            NSString *subject = [NSString stringWithFormat:$("CONTACT SUBJECT ABOUT APP %@"),
                                 [[UIApplication sharedApplication] sy_localizedName]];
            subject = [subject stringByAppendingFormat:$(" %@"), [UIApplication sy_appVersionAndBuild]];
            
            [SYEmailServicePasteboard setName:$("MAIL COPY PASTEBOARD NAME")];
            [[SYEmailHelper shared] composeEmailWithAddress:kContactAddress
                                                    subject:subject
                                                       body:nil
                                               presentingVC:self
                                                 completion:^(BOOL actionLaunched, SYEmailService *service, NSError *error)
            {
                NSLog($$("Completion: %@ %d %@"), service.name, actionLaunched, error);
            }];
        }
        return;
    }
    
    NSString *prefKey = self.keysGroups[indexPath.section].object2[indexPath.row];
    switch ([[SYPreferences shared] typeForKey:prefKey])
    {
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
