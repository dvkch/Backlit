//
//  MHGalleryOverViewController.m
//  MHVideoPhotoGallery
//
//  Created by Mario Hahn on 27.12.13.
//  Copyright (c) 2013 Mario Hahn. All rights reserved.
//

#import "MHOverviewController.h"
#import "MHGalleryController.h"
#import "MHGallerySharedManagerPrivate.h"
#import <MobileCoreServices/UTCoreTypes.h>
#import "SDWebImageManager.h"
#import <SVProgressHUD.h>

@implementation MHIndexPinchGestureRecognizer
@end

@interface MHOverviewController ()

@property (nonatomic, strong) MHTransitionShowDetail *interactivePushTransition;
@property (nonatomic        ) CGPoint                lastPoint;
@property (nonatomic        ) CGFloat                startScale;
@end


@implementation MHOverviewController

- (void)viewDidLoad{
    [super viewDidLoad];
    
    self.automaticallyAdjustsScrollViewInsets = NO;
    self.UICustomization = self.galleryViewController.UICustomization;
    
    self.title = self.UICustomization.overviewTitle;
    
    self.editing = NO;
    
    self.toolbarItems = @[[[UIBarButtonItem alloc] initWithBarButtonSystemItem:UIBarButtonSystemItemFlexibleSpace target:nil action:nil],
                          [[UIBarButtonItem alloc] initWithBarButtonSystemItem:UIBarButtonSystemItemAction target:self action:@selector(sharePressed)]];
    
    self.collectionViewLayout = self.UICustomization.overviewCollectionViewLayout;
    
    self.collectionView = [UICollectionView.alloc initWithFrame:self.view.bounds
                                           collectionViewLayout:self.collectionViewLayout];
    
    self.collectionView.backgroundColor = [self.galleryViewController.UICustomization MHGalleryBackgroundColorForViewMode:MHGalleryViewModeOverView];
    self.collectionView.contentInset = UIEdgeInsetsMake(64, 0, 0, 0);
    
    Class cellClass = (self.UICustomization.overviewCollectionViewCellClass ?: MHMediaPreviewCollectionViewCell.class);
    [self.collectionView registerClass:cellClass forCellWithReuseIdentifier:NSStringFromClass(cellClass)];
    
    self.collectionView.dataSource = self;
    self.collectionView.alwaysBounceVertical = YES;
    self.collectionView.delegate = self;
    self.collectionView.autoresizingMask = UIViewAutoresizingFlexibleWidth|UIViewAutoresizingFlexibleHeight|UIViewAutoresizingFlexibleTopMargin;
    [self.view addSubview:self.collectionView];
    [self.collectionView reloadData];
    
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wundeclared-selector"
    
    UIMenuItem *saveItem = [UIMenuItem.alloc initWithTitle:MHGalleryLocalizedString(@"overview.menue.item.save")
                                                    action:@selector(saveImage:)];
#pragma clang diagnostic pop
    
    UIMenuController.sharedMenuController.menuItems = @[saveItem];
    
}

-(void)viewWillAppear:(BOOL)animated{
    [super viewWillAppear:animated];
    
    [self setNeedsStatusBarAppearanceUpdate];
    
    [UIApplication.sharedApplication setStatusBarStyle:self.galleryViewController.preferredStatusBarStyleMH
                                              animated:YES];
    
}

- (void)setCollectionViewLayout:(UICollectionViewLayout *)collectionViewLayout {
    self->_collectionViewLayout = collectionViewLayout;
    [self.collectionView setCollectionViewLayout:self.collectionViewLayout];
}

-(UIStatusBarStyle)preferredStatusBarStyle{
    return self.galleryViewController.preferredStatusBarStyleMH;
}

-(MHGalleryController*)galleryViewController{
    if ([self.navigationController isKindOfClass:MHGalleryController.class]) {
        return (MHGalleryController*)self.navigationController;
    }
    return nil;
}

-(MHGalleryItem*)itemForIndex:(NSInteger)index{
    return [self.galleryViewController.dataSource itemForIndex:index];
}

-(void)donePressed{
    self.navigationController.transitioningDelegate = nil;

    MHGalleryController *galleryViewController = [self galleryViewController];
    if (galleryViewController.finishedCallback) {
        galleryViewController.finishedCallback(0,nil,nil,MHGalleryViewModeOverView);
    }
}

-(void)setEditing:(BOOL)editing{
    [super setEditing:editing];
    
    if (!self.UICustomization.hideDoneButton && !self.editing){
        self.navigationItem.leftBarButtonItem =
        [UIBarButtonItem.alloc initWithBarButtonSystemItem:UIBarButtonSystemItemDone
                                                    target:self action:@selector(donePressed)];
    }
    else{
        self.navigationItem.leftBarButtonItem = nil;
    }
    
    if (self.UICustomization.allowMultipleSelection){
        self.navigationItem.rightBarButtonItem =
        [UIBarButtonItem.alloc initWithBarButtonSystemItem:(self.editing ? UIBarButtonSystemItemDone : UIBarButtonSystemItemEdit)
                                                    target:self action:@selector(selectPressed)];
    }
    else{
        self.navigationItem.rightBarButtonItem = nil;
    }
    
    self.collectionView.allowsMultipleSelection = self.UICustomization.allowMultipleSelection && self.editing;
    
    // cells hide their "slectionImageView" when not in multiple selection mode. needs to refresh
    // when we toggle it
    [self reloadData];
    
    self.navigationController.toolbarHidden = !self.editing;
}

-(void)selectPressed{
    self.editing = !self.editing;
}

-(void)sharePressed{
    
    [SVProgressHUD show];
    
    NSMutableDictionary <NSNumber*, UIImage *> *orderedImages = [NSMutableDictionary dictionary];
    dispatch_group_t group = dispatch_group_create();
    
    for (NSIndexPath *indexPath in self.collectionView.indexPathsForSelectedItems){
        dispatch_group_enter(group);
        [[self itemForIndex:indexPath.item] getImageWithCompletion:^(UIImage *image, NSError *error) {
            if (image) orderedImages[@(indexPath.item)] = image;
            dispatch_group_leave(group);
        }];
    }
    
    dispatch_notify(group, dispatch_get_main_queue(), ^{
        [SVProgressHUD dismiss];
        
        NSArray <NSNumber *> *sortedKeys = [[orderedImages allKeys] sortedArrayUsingSelector:@selector(compare:)];
        NSArray <UIImage *> *images = [orderedImages objectsForKeys:sortedKeys notFoundMarker:[UIImage new]];
        [self shareImages:images];
    });
}

-(void)shareImages:(NSArray <UIImage *> *)images{
    
    if (!images.count)
        return;
    
    UIActivityViewController *activityViewController =
    [[UIActivityViewController alloc] initWithActivityItems:images
                                      applicationActivities:nil];
    activityViewController.popoverPresentationController.barButtonItem = self.toolbarItems.lastObject;
    [self presentViewController:activityViewController animated:YES completion:nil];
}

-(void)updateTitle{
    
    if (self.editing) {
        NSString *format = MHGalleryLocalizedString(@"shareview.title.select.plural");
        if (self.collectionView.indexPathsForSelectedItems.count <= 1 )
            format = MHGalleryLocalizedString(@"shareview.title.select.singular");
        
        self.title = [NSString stringWithFormat:format, @(self.collectionView.indexPathsForSelectedItems.count).stringValue];
    } else {
        self.title = self.UICustomization.overviewTitle;

    }
}

-(NSInteger)collectionView:(UICollectionView *)collectionView numberOfItemsInSection:(NSInteger)section{
    return [self.galleryViewController.dataSource numberOfItemsInGallery:self.galleryViewController];
}

-(UICollectionViewCell *)collectionView:(UICollectionView *)collectionView cellForItemAtIndexPath:(NSIndexPath *)indexPath{
    Class cellClass = self.UICustomization.overviewCollectionViewCellClass ?: MHMediaPreviewCollectionViewCell.class;
    
    MHMediaPreviewCollectionViewCell *cell = (MHMediaPreviewCollectionViewCell *)[collectionView dequeueReusableCellWithReuseIdentifier:NSStringFromClass(cellClass) forIndexPath:indexPath];
    [self makeMHGalleryOverViewCell:cell
                        atIndexPath:indexPath];
    
    return cell;
}

-(void)makeMHGalleryOverViewCell:(MHMediaPreviewCollectionViewCell*)cell atIndexPath:(NSIndexPath*)indexPath{
    
    __weak typeof(self) weakSelf = self;
    
    MHGalleryItem *item =  [self itemForIndex:indexPath.row];
    cell.thumbnail.image = nil;
    
    
    cell.videoGradient.hidden = YES;
    cell.videoIcon.hidden     = YES;
    
    
    cell.saveImage = ^(BOOL shouldSave){
        [weakSelf getImageForItem:item
                   finishCallback:^(UIImage *image) {
                       UIImageWriteToSavedPhotosAlbum(image, nil, nil, nil);
                   }];
    };
    
    cell.videoDurationLength.text = @"";
    cell.thumbnail.backgroundColor = [UIColor lightGrayColor];
    cell.galleryItem = item;
    
    cell.thumbnail.userInteractionEnabled =YES;
    
    cell.selectionImageView.hidden = !self.UICustomization.allowMultipleSelection || !self.editing;
    
    MHIndexPinchGestureRecognizer *pinch = [MHIndexPinchGestureRecognizer.alloc initWithTarget:self
                                                                                        action:@selector(userDidPinch:)];
    pinch.indexPath = indexPath;
    [cell.thumbnail addGestureRecognizer:pinch];
    
    UIRotationGestureRecognizer *rotate = [UIRotationGestureRecognizer.alloc initWithTarget:self
                                                                                     action:@selector(userDidRoate:)];
    rotate.delegate = self;
    [cell.thumbnail addGestureRecognizer:rotate];
}

- (BOOL)gestureRecognizer:(UIGestureRecognizer *)gestureRecognizer shouldRecognizeSimultaneouslyWithGestureRecognizer:(UIGestureRecognizer *)otherGestureRecognizer{
    return YES;
}

-(void)userDidRoate:(UIRotationGestureRecognizer*)recognizer{
    if (self.interactivePushTransition) {
        CGFloat angle = recognizer.rotation;
        self.interactivePushTransition.angle = angle;
    }
}
-(void)userDidPinch:(MHIndexPinchGestureRecognizer*)recognizer{
    
    CGFloat scale = recognizer.scale/5;
    
    if (recognizer.state == UIGestureRecognizerStateBegan) {
        if (recognizer.scale>1) {
            self.interactivePushTransition = MHTransitionShowDetail.new;
            self.interactivePushTransition.indexPath = recognizer.indexPath;
            self.lastPoint = [recognizer locationInView:self.view];
            
            MHGalleryImageViewerViewController *detail = MHGalleryImageViewerViewController.new;
            detail.galleryItems = self.galleryItems;
            detail.pageIndex = recognizer.indexPath.row;
            self.startScale = recognizer.scale/8;
            [self.navigationController pushViewController:detail
                                                 animated:YES];
        }else{
            recognizer.cancelsTouchesInView = YES;
        }
    }else if (recognizer.state == UIGestureRecognizerStateChanged) {
        
        if (recognizer.numberOfTouches <2) {
            recognizer.enabled = NO;
            recognizer.enabled = YES;
        }
        
        CGPoint point = [recognizer locationInView:self.view];
        self.interactivePushTransition.scale = recognizer.scale/8-self.startScale;
        self.interactivePushTransition.changedPoint = CGPointMake(self.lastPoint.x - point.x, self.lastPoint.y - point.y) ;
        [self.interactivePushTransition updateInteractiveTransition:scale];
        self.lastPoint = point;
    }else if (recognizer.state == UIGestureRecognizerStateEnded || recognizer.state == UIGestureRecognizerStateCancelled) {
        if (scale > 0.5) {
            [self.interactivePushTransition finishInteractiveTransition];
        }else {
            [self.interactivePushTransition cancelInteractiveTransition];
        }
        self.interactivePushTransition = nil;
    }
    
}


- (id<UIViewControllerInteractiveTransitioning>)navigationController:(UINavigationController *)navigationController
                         interactionControllerForAnimationController:(id<UIViewControllerAnimatedTransitioning>)animationController {
    if ([animationController isKindOfClass:MHTransitionShowDetail.class]) {
        return self.interactivePushTransition;
    }else {
        return nil;
    }
}

- (id<UIViewControllerAnimatedTransitioning>)navigationController:(UINavigationController *)navigationController
                                  animationControllerForOperation:(UINavigationControllerOperation)operation
                                               fromViewController:(UIViewController *)fromVC
                                                 toViewController:(UIViewController *)toVC {
    
    if (fromVC == self && [toVC isKindOfClass:MHGalleryImageViewerViewController.class]) {
        return MHTransitionShowDetail.new;
    }else {
        return nil;
    }
}

- (void)viewDidAppear:(BOOL)animated {
    [super viewDidAppear:animated];
    self.navigationController.delegate = self;
    self.editing = NO;
}

- (void)viewWillDisappear:(BOOL)animated {
    [super viewWillDisappear:animated];
    
    if (self.navigationController.delegate == self) {
        self.navigationController.delegate = nil;
    }
}
-(void)pushToImageViewerForIndexPath:(NSIndexPath*)indexPath{
    
    MHGalleryImageViewerViewController *detail = MHGalleryImageViewerViewController.new;
    detail.pageIndex = indexPath.row;
    detail.galleryItems = self.galleryItems;
    if ([self.navigationController isKindOfClass:MHGalleryController.class]) {
        [self.navigationController pushViewController:detail animated:YES];
    }
}
-(void)collectionView:(UICollectionView *)collectionView didSelectItemAtIndexPath:(NSIndexPath *)indexPath{
    
    MHMediaPreviewCollectionViewCell *cell = (MHMediaPreviewCollectionViewCell*)[collectionView cellForItemAtIndexPath:indexPath];
    
    if (self.editing) {
        [cell setSelected:!cell.selected];
        [self updateTitle];
        return;
    }
    
    __weak typeof(self) weakSelf = self;
    
    MHGalleryItem *item =  [self itemForIndex:indexPath.row];
    
    UIImage *thumbImage = [SDImageCache.sharedImageCache imageFromDiskCacheForKey:item.URL.absoluteString];
    if (thumbImage) {
        cell.thumbnail.image = thumbImage;
    }
    if ([item.URL.scheme isEqualToString:MHAssetLibrary]) {
        
        [MHGallerySharedManager.sharedManager getImageFromAssetLibrary:item.URL
                                                             assetType:MHAssetImageTypeFull
                                                          successBlock:^(UIImage *image, NSError *error) {
                                                              cell.thumbnail.image = image;
                                                              [weakSelf pushToImageViewerForIndexPath:indexPath];
                                                          }];
    }else{
        [self pushToImageViewerForIndexPath:indexPath];
    }
}

- (void)collectionView:(UICollectionView *)collectionView didDeselectItemAtIndexPath:(NSIndexPath *)indexPath{
    [self updateTitle];
}

- (BOOL)collectionView:(UICollectionView *)collectionView shouldShowMenuForItemAtIndexPath:(NSIndexPath *)indexPath {
    return YES;
}

- (BOOL)collectionView:(UICollectionView *)collectionView canPerformAction:(SEL)action forItemAtIndexPath:(NSIndexPath *)indexPath withSender:(id)sender {
    MHGalleryItem *item =  [self itemForIndex:indexPath.row];
    if (item.galleryType == MHGalleryTypeImage) {
        if ([NSStringFromSelector(action) isEqualToString:@"copy:"] || [NSStringFromSelector(action) isEqualToString:@"saveImage:"]){
            return YES;
        }
    }
    return NO;
}

-(void)getImageForItem:(MHGalleryItem*)item
        finishCallback:(void(^)(UIImage *image))FinishBlock{
    
    [SDWebImageManager.sharedManager downloadImageWithURL:item.URL
                                                  options:SDWebImageContinueInBackground
                                                 progress:nil
                                                completed:^(UIImage *image, NSError *error, SDImageCacheType cacheType, BOOL finished, NSURL *imageURL) {
                                                    FinishBlock(image);
                                                }];
}
-(void)didReceiveMemoryWarning{
    [super didReceiveMemoryWarning];
    
    
}

- (void)collectionView:(UICollectionView *)collectionView performAction:(SEL)action forItemAtIndexPath:(NSIndexPath *)indexPath withSender:(id)sender{
    if ([NSStringFromSelector(action) isEqualToString:@"copy:"]) {
        UIPasteboard *pasteBoard = [UIPasteboard pasteboardWithName:UIPasteboardNameGeneral create:NO];
        pasteBoard.persistent = YES;
        MHGalleryItem *item =  [self itemForIndex:indexPath.row];
        [self getImageForItem:item finishCallback:^(UIImage *image) {
            if (image) {
                UIPasteboard *pasteboard = UIPasteboard.generalPasteboard;
                if (image.images) {
                    NSData *data = [NSData dataWithContentsOfFile:[SDImageCache.sharedImageCache defaultCachePathForKey:item.URL.absoluteString]];
                    [pasteboard setData:data forPasteboardType:(__bridge NSString *)kUTTypeGIF];
                }else{
                    NSData *data = UIImagePNGRepresentation(image);
                    [pasteboard setData:data forPasteboardType:(__bridge NSString *)kUTTypeImage];
                    
                }
            }
        }];
    }
}

- (void)reloadData
{
    self.collectionView.collectionViewLayout = self.UICustomization.overviewCollectionViewLayout;
    [self.collectionView.collectionViewLayout invalidateLayout];
    [self.collectionView reloadData];
    [self updateTitle];
}

/*
-(void)willRotateToInterfaceOrientation:(UIInterfaceOrientation)toInterfaceOrientation duration:(NSTimeInterval)duration{
    [self.collectionViewLayout invalidateLayout];
}

- (void)viewWillTransitionToSize:(CGSize)size withTransitionCoordinator:(id<UIViewControllerTransitionCoordinator>)coordinator{
    [self.collectionViewLayout invalidateLayout];
}
*/

@end
