//
//  SYDeviceVC.m
//  SaneScanner
//
//  Created by rominet on 06/05/15.
//  Copyright (c) 2015 Syan. All rights reserved.
//

#import "SYDeviceVC.h"
#import "SYSaneDevice.h"
#import <SaneSwift/SaneSwift-umbrella.h>
#import "SYSaneOptionBool.h"
#import "SYSaneOptionNumber.h"
#import "SYSaneOptionString.h"
#import "SYSaneOptionButton.h"
#import "SYSaneOptionGroup.h"
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
#import "UIScrollView+SY.h"
#import "UIViewController+SYKit.h"
#import "SVProgressHUD+SY.h"
#import <SYMetadata.h>
#import "SYSaneScanParameters.h"
#import "UIApplication+SY.h"
#import "UIActivityViewController+SY.h"
#import "SaneScanner-Swift.h"

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
    [self.tableView registerNib:[UINib nibWithNibName:$$("OptionCell") bundle:nil] forCellReuseIdentifier:OptionCell.sy_className];
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
    @weakify(self);
    [self.tableView sy_addPullToResfreshWithBlock:^(UIScrollView * _) {
        @strongify(self)
        [self refresh];
    }];
    
    [[NSNotificationCenter defaultCenter] addObserver:self
                                             selector:@selector(prefsChangedNotification:)
                                                 name:SYPreferencesChangedNotification
                                               object:nil];

    // prevent tableView width to be 0 and have a constraint issue when computing cell size
    [self.view layoutIfNeeded];

    // initial refresh
    [self.tableView sy_showPullToRefreshAndRunBlock:YES];
}

- (void)dealloc
{
    [Sane.shared closeDevice:self.device];
    [[NSNotificationCenter defaultCenter] removeObserver:self name:SYPreferencesChangedNotification object:nil];
}

#pragma mark - Snapshots

- (void)prepareForSnapshotting
{
    if ([SYAppDelegate obtain].snapshotType == SYSnapshotType_None)
        return;
    
    if ([SYAppDelegate obtain].snapshotType == SYSnapshotType_DevicePreview ||
        [SYAppDelegate obtain].snapshotType == SYSnapshotType_DeviceOptions ||
        [SYAppDelegate obtain].snapshotType == SYSnapshotType_DeviceOptionPopup)
    {
        CGRect rect = CGRectMake(0.1, 0.2, 0.8, 0.6);
        self.device.lastPreviewImage = [UIImage imageNamed:[SYAppDelegate obtain].snapshotTestScanImagePath];
        [self updatePreviewCellWithCropAreaPercent:rect];
    }
    
    if ([SYAppDelegate obtain].snapshotType == SYSnapshotType_DeviceOptions ||
        [SYAppDelegate obtain].snapshotType == SYSnapshotType_DeviceOptionPopup)
    {
        NSIndexPath *firstOption = [NSIndexPath indexPathForRow:0 inSection:1];
        [self.tableView scrollToRowAtIndexPath:firstOption atScrollPosition:UITableViewScrollPositionTop animated:NO];
    }
    
    if ([SYAppDelegate obtain].snapshotType == SYSnapshotType_DeviceOptionPopup)
    {
        NSIndexPath *firstOption = [NSIndexPath indexPathForRow:0 inSection:1];
        [self tableView:self.tableView didSelectRowAtIndexPath:firstOption];
    }
    
    dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(1. * NSEC_PER_SEC)), dispatch_get_main_queue(), ^{
        [SVProgressHUD dismiss];
    });
}

#pragma mark - Data

- (void)setDevice:(SYSaneDevice *)device
{
    self->_device = device;
    [self setTitle:self.device.model];
}

- (void)refresh
{
    if (self.refreshing)
        return;
    
    self.refreshing = YES;
    
    @weakify(self);
    [Sane.shared listOptionsFor:self.device completion:^{
        @strongify(self)
        [self.tableView reloadData];
        self.refreshing = NO;
        [self.tableView sy_endPullToRefresh];
        
        dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(1. * NSEC_PER_SEC)), dispatch_get_main_queue(), ^{
            [self prepareForSnapshotting];
        });
    }];
}

- (NSArray <SYSaneOptionGroup *> *)optionGroups
{
    return [self.device filteredGroupedOptionsWithoutAdvanced:![[SYPreferences shared] showAdvancedOptions]];
}

- (SYSaneOptionGroup *)optionGroupForTableViewSection:(NSUInteger)section
{
    if (section >= self.optionGroups.count)
        return nil;

    return self.optionGroups[section-1];
}

- (SYSaneOption *)optionForTableViewIndexPath:(NSIndexPath *)indexPath
{
    SYSaneOptionGroup *group = [self optionGroupForTableViewSection:indexPath.section];
    
    if (indexPath.row >= group.items.count)
        return nil;
    
    return group.items[indexPath.row];
}

- (SYMetadata *)metadataForSaneParameters:(SYSaneScanParameters *)parameters
{
    SYSaneOptionNumber *optionResX = (SYSaneOptionNumber *)[self.device standardOption:SYSaneStandardOptionResolutionX];
    SYSaneOptionNumber *optionResY = (SYSaneOptionNumber *)[self.device standardOption:SYSaneStandardOptionResolutionY];
    SYSaneOptionNumber *optionRes  = (SYSaneOptionNumber *)[self.device standardOption:SYSaneStandardOptionResolution];
    
    NSNumber *resXinches = (optionResX.value ?: optionRes.value);
    NSNumber *resYinches = (optionResY.value ?: optionRes.value);
    
    NSUInteger resXmeters = (NSUInteger)(resXinches.doubleValue / 2.54 * 100.);
    NSUInteger resYmeters = (NSUInteger)(resYinches.doubleValue / 2.54 * 100.);
    
    SYMetadata *metadata = [[SYMetadata alloc] init];
    
    metadata.metadataTIFF = [[SYMetadataTIFF alloc] init];
    [metadata.metadataTIFF setOrientation:@(SYPictureTiffOrientation_TopLeft)];
    [metadata.metadataTIFF setMake:self.device.vendor];
    [metadata.metadataTIFF setModel:self.device.model];
    [metadata.metadataTIFF setSoftware:[[UIApplication sharedApplication] sy_localizedName]];
    [metadata.metadataTIFF setXResolution:resXinches];
    [metadata.metadataTIFF setYResolution:resYinches];
    [metadata.metadataTIFF setResolutionUnit:@(2)]; // 2 = inches, let's hope it'll make sense for every device
    
    metadata.metadataPNG = [[SYMetadataPNG alloc] init];
    [metadata.metadataPNG setXPixelsPerMeter:(resXinches ? @(resXmeters) : nil)];
    [metadata.metadataPNG setYPixelsPerMeter:(resYinches ? @(resYmeters) : nil)];
    
    metadata.metadataJFIF = [[SYMetadataJFIF alloc] init];
    [metadata.metadataJFIF setXDensity:resXinches];
    [metadata.metadataJFIF setYDensity:resYinches];
    
    return metadata;
}

#pragma mark - Notifications

- (void)prefsChangedNotification:(NSNotification *)notification
{
    [self.tableView reloadData];
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
    [Sane.shared scanWithDevice:self.device progress:^(float progress, UIImage * _Nullable incompleteImage) {
        [SVProgressHUD showProgress:progress];
    } completion:^(UIImage *image, SYSaneScanParameters *parameters, NSError *error) {
        if (error)
            [SVProgressHUD showErrorWithStatus:error.sy_alertMessage];
        else
        {
            SYMetadata *metadata = [self metadataForSaneParameters:parameters];
            [[SYGalleryManager shared] addImage:image metadata:metadata];
            [SVProgressHUD showSuccessWithStatus:nil duration:1];
        }
    }];
}

- (void)buttonScanTap:(id)sender
{
    __block DLAVAlertView *alertView;
    __block UIImageView *alertViewImageView;
    __block MHGalleryItem *item;
    
    @weakify(self);
    void(^block)(float progress, BOOL finished, UIImage *image, SYSaneScanParameters *parameters, NSError *error) =
    ^(float progress, BOOL finished, UIImage *image, SYSaneScanParameters *parameters, NSError *error)
    {
        @strongify(self)
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
            SYMetadata *metadata = [self metadataForSaneParameters:parameters];
            item = [[SYGalleryManager shared] addImage:image metadata:metadata];
            [SVProgressHUD dismiss];
            [self updatePreviewImageCellWithImage:image];
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
    [Sane.shared scanWithDevice:self.device progress:^(float progress, UIImage * _Nullable incompleteImage) {
        block(progress, NO, incompleteImage, nil, nil);
    } completion:^(UIImage *image,SYSaneScanParameters *parameters, NSError *error) {
        block(1., YES, image, parameters, error);
    }];
}

- (void)buttonSettingsTap:(id)sender
{
    SYPrefVC *prefVC = [[SYPrefVC alloc] init];
    UINavigationController *nc = [[UINavigationController alloc] initWithRootViewController:prefVC];
    [nc setModalPresentationStyle:UIModalPresentationFormSheet];
    [self presentViewController:nc animated:YES completion:nil];
}

- (void)shareItem:(MHGalleryItem *)item
{
    if (!item.URL)
        return;
    
    [UIActivityViewController sy_showForUrls:@[item.URL]
                        bottomInPresentingVC:self.splitViewController
                                  completion:nil];
}

- (void)updatePreviewCellWithCropAreaPercent:(CGRect)rect
{
    if (!self.device.canCrop)
        return;
    
    SYPreviewCell *previewCell;
    
    for (UITableViewCell *cell in self.tableView.visibleCells)
        if ([cell isKindOfClass:[SYPreviewCell class]])
            previewCell = (SYPreviewCell *)cell;
    
    if (!previewCell)
        return;
    
    CGRect cropArea = CGRectMake(self.device.maxCropArea.origin.x + CGRectGetWidth (self.device.maxCropArea) * rect.origin.x,
                                 self.device.maxCropArea.origin.y + CGRectGetHeight(self.device.maxCropArea) * rect.origin.y,
                                 CGRectGetWidth (self.device.maxCropArea) * rect.size.width,
                                 CGRectGetHeight(self.device.maxCropArea) * rect.size.height);
    
    self.device.cropArea = CGRectIntersection(self.device.maxCropArea, cropArea);
    [previewCell refresh];
}

- (void)updatePreviewImageCellWithImage:(UIImage *)image
{
    // update only if we scanned without cropping
    if (!CGRectEqualToRect(self.device.cropArea, self.device.maxCropArea))
        return;
    
    // update only if we don't require color mode to be set at auto, or when auto is not available
    if ([[SYPreferences shared] previewWithAutoColorMode] && [self.device standardOption:SYSaneStandardOptionColorMode].capSetAuto)
        return;
    
    SYPreviewCell *previewCell;
    
    for (UITableViewCell *cell in self.tableView.visibleCells)
        if ([cell isKindOfClass:[SYPreviewCell class]])
            previewCell = (SYPreviewCell *)cell;
    
    if (!previewCell)
        return;
    
    self.device.lastPreviewImage = image;
    [previewCell refresh];
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
        OptionCell *cell = (OptionCell*)[tableView dequeueReusableCellWithIdentifier:OptionCell.sy_className];
        SYSaneOption *opt = [self optionForTableViewIndexPath:indexPath];
        
        [cell updateWithOption:opt];
        [cell setShowDescription:NO];
        
        return cell;
    }
}

- (NSString *)tableView:(UITableView *)tableView titleForHeaderInSection:(NSInteger)section
{
    if (section == 0)
        return $("DEVICE SECTION PREVIEW");
    
    return [self optionGroupForTableViewSection:section].localizedTitle;
}

- (CGFloat)tableView:(UITableView *)tableView heightForRowAtIndexPath:(NSIndexPath *)indexPath
{
    CGFloat width = tableView.bounds.size.width;
    CGFloat maxHeight = tableView.bounds.size.height * 2. / 3.;
    
    if (self.traitCollection.verticalSizeClass == UIUserInterfaceSizeClassCompact)
        maxHeight = 500;
    
    if (indexPath.section == 0)
        return [SYPreviewCell cellHeightForDevice:self.device width:width maxHeight:maxHeight];
    
    return [OptionCell cellHeightWithOption:[self optionForTableViewIndexPath:indexPath]
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
    if (!opt)
        return;
    
    [SYSaneOptionUI showDetailsAndInputForOption:opt block:^(BOOL reloadAllOptions, NSError *error) {
        if (reloadAllOptions)
        {
            [Sane.shared listOptionsFor:self.device completion:^{
                [self.tableView reloadData];
                if (error)
                    [SVProgressHUD showErrorWithStatus:error.sy_alertMessage];
                else
                    [SVProgressHUD dismiss];
            }];
            return;
        }
        
        [self.tableView reloadData];
        if (error)
            [SVProgressHUD showErrorWithStatus:error.sy_alertMessage];
        else
            [SVProgressHUD dismiss];
    }];
}

@end
