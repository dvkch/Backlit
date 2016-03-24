//
//  SYGalleryThumbsView.m
//  SaneScanner
//
//  Created by Stan Chevallier on 14/03/2016.
//  Copyright © 2016 Syan. All rights reserved.
//

#import "SYGalleryThumbsView.h"
#import "SYGalleryManager.h"
#import <Masonry.h>
#import "UIColor+SY.h"
#import "SYGalleryThumbsCell.h"
#import <SYGradientView.h>
#import <MHGallery.h>
#import <AVFoundation/AVUtilities.h>

static CGFloat const kGradientWidth = 30;

@interface SYGalleryThumbsView () <UICollectionViewDataSource, UICollectionViewDelegateFlowLayout, SYGalleryManagerDelegate>
@property (nonatomic, strong) UICollectionView *collectionView;
@property (nonatomic, strong) NSArray <MHGalleryItem *> *galleryItems;
@property (nonatomic, strong) SYGradientView *leftGradientView;
@property (nonatomic, strong) SYGradientView *rightGradientView;
@end

@implementation SYGalleryThumbsView

+ (instancetype)showInToolbarOfController:(UIViewController *)controller
                                tintColor:(UIColor *)tintColor
{
    UIColor *color = (tintColor ?: controller.navigationController.toolbar.backgroundColor);
    
    CGRect thumbsViewInitialRect = controller.navigationController.toolbar.bounds;
    SYGalleryThumbsView *thumbsView = [[SYGalleryThumbsView alloc] initWithFrame:thumbsViewInitialRect];
    [thumbsView setParentViewController:controller];
    [thumbsView setTintColor:color];
    [thumbsView setBackgroundColor:color];
    [thumbsView setAutoresizingMask:UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight];
    [controller setToolbarItems:@[[[UIBarButtonItem alloc] initWithCustomView:thumbsView]]];
    return thumbsView;
}

- (instancetype)init
{
    self = [super init];
    if (self) [self customInit];
    return self;
}

- (instancetype)initWithFrame:(CGRect)frame
{
    self = [super initWithFrame:frame];
    if (self) [self customInit];
    return self;
}

- (void)customInit
{
    if (self.collectionView)
        return;
    
    [self setClipsToBounds:YES];
    [[SYGalleryManager shared] addDelegate:self];
    
    UICollectionViewFlowLayout *layout = [[UICollectionViewFlowLayout alloc] init];
    [layout setScrollDirection:UICollectionViewScrollDirectionHorizontal];
    
    self.collectionView = [[UICollectionView alloc] initWithFrame:CGRectZero collectionViewLayout:layout];
    [self.collectionView setClipsToBounds:NO];
    [self.collectionView setBackgroundColor:[UIColor clearColor]];
    [self.collectionView setDataSource:self];
    [self.collectionView setDelegate:self];
    [self.collectionView registerClass:[SYGalleryThumbsCell class] forCellWithReuseIdentifier:@"cell"];
    [self.collectionView setContentInset:UIEdgeInsetsMake(0, kGradientWidth, 0, kGradientWidth)];
    [self.collectionView setScrollIndicatorInsets:UIEdgeInsetsMake(0, kGradientWidth, 0, kGradientWidth)];
    [self.collectionView setContentCompressionResistancePriority:UILayoutPriorityDefaultHigh
                                                         forAxis:UILayoutConstraintAxisVertical];
    [self addSubview:self.collectionView];
    
    self.leftGradientView = [[SYGradientView alloc] init];
    [self.leftGradientView.layer setColors:@[(id)[UIColor whiteColor].CGColor,
                                             (id)[UIColor colorWithWhite:1.0 alpha:0.0].CGColor]];
    [self.leftGradientView.layer setLocations:@[@0.2, @1]];
    [self.leftGradientView.layer setStartPoint:CGPointZero];
    [self.leftGradientView.layer setEndPoint:CGPointMake(1, 0)];
    [self addSubview:self.leftGradientView];
    
    self.rightGradientView = [[SYGradientView alloc] init];
    [self.rightGradientView.layer setColors:@[(id)[UIColor whiteColor].CGColor,
                                              (id)[UIColor colorWithWhite:1.0 alpha:0.0].CGColor]];
    [self.rightGradientView.layer setLocations:@[@0.2, @1]];
    [self.rightGradientView.layer setStartPoint:CGPointMake(1, 0)];
    [self.rightGradientView.layer setEndPoint:CGPointZero];
    [self addSubview:self.rightGradientView];
    
    [self.leftGradientView mas_makeConstraints:^(MASConstraintMaker *make) {
        make.top.equalTo(@0);
        make.left.equalTo(@0);
        make.bottom.equalTo(@0);
        make.width.equalTo(@(kGradientWidth));
    }];
    
    [self.rightGradientView mas_makeConstraints:^(MASConstraintMaker *make) {
        make.top.equalTo(@0);
        make.right.equalTo(@0);
        make.bottom.equalTo(@0);
        make.width.equalTo(@(kGradientWidth));
    }];
    
    [self.collectionView mas_makeConstraints:^(MASConstraintMaker *make) {
        make.top.equalTo(@(10)).priorityLow();
        make.left.equalTo(@0);
        make.right.equalTo(@0);
        make.bottom.equalTo(@(-10)).priorityLow();
        make.centerY.equalTo(@0);
        make.height.greaterThanOrEqualTo(@30);
    }];
}

- (void)setTintColor:(UIColor *)tintColor
{
    [super setTintColor:tintColor];
    
    UIColor *gradientOpaqueColor = tintColor ?: [UIColor whiteColor];
    
    [self.leftGradientView.layer  setColors:@[(id)gradientOpaqueColor.CGColor,
                                              (id)[gradientOpaqueColor colorWithAlphaComponent:0.].CGColor]];
    [self.rightGradientView.layer setColors:@[(id)gradientOpaqueColor.CGColor,
                                              (id)[gradientOpaqueColor colorWithAlphaComponent:0.].CGColor]];
}

- (void)layoutSubviews
{
    [super layoutSubviews];
    
    NSArray <NSIndexPath *> *indexPaths = self.collectionView.indexPathsForVisibleItems;
    indexPaths = [indexPaths sortedArrayUsingSelector:@selector(compare:)];
    
    NSIndexPath *leftIndexPath = indexPaths.firstObject;
    if (!leftIndexPath && self.galleryItems.count)
        leftIndexPath = [NSIndexPath indexPathForItem:0 inSection:0];
    
    UICollectionViewFlowLayout *layout = [[UICollectionViewFlowLayout alloc] init];
    [layout setScrollDirection:UICollectionViewScrollDirectionHorizontal];
    [layout setMinimumInteritemSpacing:10];
    
    [self.collectionView setCollectionViewLayout:layout animated:NO];
    [self.collectionView scrollToItemAtIndexPath:leftIndexPath
                                atScrollPosition:UICollectionViewScrollPositionLeft
                                        animated:NO];
    
    // makes sure contentSize is correct
    [self.collectionView layoutIfNeeded];
    CGFloat availableWidth = self.collectionView.bounds.size.width - self.collectionView.contentSize.width;
    availableWidth = MAX(0, availableWidth - 2 * kGradientWidth);
    CGFloat horizontalInset = kGradientWidth + availableWidth / 2.;
    [self.collectionView setContentInset:UIEdgeInsetsMake(0, horizontalInset, 0, horizontalInset)];
}

- (UIImage *)imageForItemAtIndexPath:(NSIndexPath *)indexPath
{
    return [[SYGalleryManager shared] thumbnailForItem:self.galleryItems[indexPath.item]];
}

#pragma mark - SYGalleryManager

- (void)gallerymanager:(SYGalleryManager *)gallerymanager
 didUpdateGalleryItems:(NSArray<MHGalleryItem *> *)items
               newItem:(MHGalleryItem *)newItem
           removedItem:(MHGalleryItem *)removedItem
{
    [UIView animateWithDuration:0.3 animations:^{
        self.galleryItems = items;
        [self.collectionView reloadData];
        [self setNeedsLayout];
    }];
}

#pragma mark - UICollectionView

- (NSInteger)numberOfSectionsInCollectionView:(UICollectionView *)collectionView
{
    return 1;
}

- (NSInteger)collectionView:(UICollectionView *)collectionView
     numberOfItemsInSection:(NSInteger)section
{
    return self.galleryItems.count;
}

- (UICollectionViewCell *)collectionView:(UICollectionView *)collectionView
                  cellForItemAtIndexPath:(NSIndexPath *)indexPath
{
    __weak SYGalleryThumbsView *wSelf = self;
    
    SYGalleryThumbsCell *cell = [collectionView dequeueReusableCellWithReuseIdentifier:@"cell" forIndexPath:indexPath];
    [cell updateWithItems:self.galleryItems
                    index:indexPath.item
         parentController:self.parentViewController
             dismissBlock:^UIImageView *(NSUInteger index)
    {
        if (index >= wSelf.galleryItems.count)
            return nil;
        
        NSIndexPath *dismissIndexPath = [NSIndexPath indexPathForItem:index inSection:0];
        [wSelf.collectionView scrollToItemAtIndexPath:dismissIndexPath
                                     atScrollPosition:(UICollectionViewScrollPositionCenteredVertically |
                                                       UICollectionViewScrollPositionCenteredHorizontally)
                                             animated:NO];
        
        // needed to be sure the cell is loaded
        [wSelf.collectionView layoutIfNeeded];
        
        SYGalleryThumbsCell *dismissCell =
        (SYGalleryThumbsCell *)[wSelf.collectionView cellForItemAtIndexPath:dismissIndexPath];
        
        return dismissCell.imageView;
    }];
    
    return cell;
}

- (CGSize)collectionView:(UICollectionView *)collectionView
                  layout:(UICollectionViewLayout *)collectionViewLayout
  sizeForItemAtIndexPath:(NSIndexPath *)indexPath
{
    UIImage *image = [self imageForItemAtIndexPath:indexPath];

    CGFloat availableHeight = collectionView.bounds.size.height;
    CGSize size = CGSizeMake(availableHeight, availableHeight);
    size.width = AVMakeRectWithAspectRatioInsideRect(image.size, (CGRect){CGPointZero, size}).size.width;
    
    return size;
}

@end
