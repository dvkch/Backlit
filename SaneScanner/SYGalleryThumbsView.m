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
#import <SYGradientView.h>
#import <MHGallery.h>
#import <AVFoundation/AVUtilities.h>

static CGFloat const kGradientWidth = 30;
static CGFloat const kShadowRadius  =  2;

@interface SYGalleryThumbsView () <UICollectionViewDataSource, UICollectionViewDelegateFlowLayout, SYGalleryManagerDelegate>
@property (nonatomic, strong) UICollectionView *collectionView;
@property (nonatomic, strong) NSArray <MHGalleryItem *> *galleryItems;
@property (nonatomic, strong) SYGradientView *leftGradientView;
@property (nonatomic, strong) SYGradientView *rightGradientView;
@end

@implementation SYGalleryThumbsView

+ (instancetype)showInToolbarOfController:(UIViewController *)controller
{
    CGRect thumbsViewInitialRect = controller.navigationController.toolbar.bounds;
    //thumbsViewInitialRect.size.width  -= 2 * [UIToolbar horizontalPadding];
    thumbsViewInitialRect.size.height -= 8;
    
    SYGalleryThumbsView *thumbsView = [[SYGalleryThumbsView alloc] initWithFrame:thumbsViewInitialRect];
    [thumbsView setParentViewController:controller];
    [thumbsView setTintColor:controller.navigationController.toolbar.backgroundColor];
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
    [self.collectionView registerClass:[UICollectionViewCell class] forCellWithReuseIdentifier:@"cell"];
    [self.collectionView setContentInset:UIEdgeInsetsMake(0, kGradientWidth, 0, kGradientWidth)];
    [self.collectionView setScrollIndicatorInsets:UIEdgeInsetsMake(0, kGradientWidth, 0, kGradientWidth)];
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
        make.top.equalTo(@(2*kShadowRadius));
        make.left.equalTo(@0);
        make.right.equalTo(@0);
        make.bottom.equalTo(@(-2*kShadowRadius));
    }];
    
    self.galleryItems = [[SYGalleryManager shared] galleryItems];
}

- (void)setTintColor:(UIColor *)tintColor
{
    [super setTintColor:tintColor];
    
    UIColor *gradientOpaqueColor = tintColor ?: [UIColor whiteColor];
    
    [self.leftGradientView.layer  setColors:@[(id)gradientOpaqueColor.CGColor,
                                              (id)[UIColor colorWithWhite:1.0 alpha:0.0].CGColor]];
    [self.rightGradientView.layer setColors:@[(id)gradientOpaqueColor.CGColor,
                                              (id)[UIColor colorWithWhite:1.0 alpha:0.0].CGColor]];
}

- (void)layoutSubviews
{
    [super layoutSubviews];
    
    NSIndexPath *leftIndexPath = self.collectionView.indexPathsForVisibleItems.firstObject;
    
    UICollectionViewFlowLayout *layout = [[UICollectionViewFlowLayout alloc] init];
    [layout setScrollDirection:UICollectionViewScrollDirectionHorizontal];
    [layout setMinimumInteritemSpacing:10];
    
    [self.collectionView setCollectionViewLayout:layout animated:NO];
    [self.collectionView scrollToItemAtIndexPath:leftIndexPath
                                atScrollPosition:UICollectionViewScrollPositionLeft
                                        animated:NO];
}

- (UIImage *)imageForItemAtIndexPath:(NSIndexPath *)indexPath
{
    return [[SYGalleryManager shared] thumbnailImageForGalleryItem:self.galleryItems[indexPath.item]];
}

#pragma mark - SYGalleryManager

- (void)gallerymanager:(SYGalleryManager *)gallerymanager didAddImage:(NSString *)imageName
{
    [self.collectionView performBatchUpdates:^{
        self.galleryItems = gallerymanager.galleryItems;
        [self.collectionView reloadItemsAtIndexPaths:self.collectionView.indexPathsForVisibleItems];
        [self.collectionView insertItemsAtIndexPaths:@[[NSIndexPath indexPathForItem:0 inSection:0]]];
    } completion:nil];
}

- (void)gallerymanager:(SYGalleryManager *)gallerymanager didUpdateImageList:(NSArray<NSString *> *)imageList
{
    self.galleryItems = gallerymanager.galleryItems;
    [self.collectionView reloadData];
}

#pragma mark - UICollectionView

- (NSInteger)numberOfSectionsInCollectionView:(UICollectionView *)collectionView
{
    return 1;
}

- (NSInteger)collectionView:(UICollectionView *)collectionView numberOfItemsInSection:(NSInteger)section
{
    return self.galleryItems.count;
}

- (UICollectionViewCell *)collectionView:(UICollectionView *)collectionView cellForItemAtIndexPath:(NSIndexPath *)indexPath
{
    UICollectionViewCell *cell = [collectionView dequeueReusableCellWithReuseIdentifier:@"cell" forIndexPath:indexPath];
    [cell.contentView.subviews makeObjectsPerformSelector:@selector(removeFromSuperview)];
    //[cell.contentView.layer setBorderColor:[UIColor vividBlueColor].CGColor];
    //[cell.contentView.layer setBorderWidth:2.];
    [cell.contentView.layer setShadowColor:[UIColor blackColor].CGColor];
    [cell.contentView.layer setShadowOffset:CGSizeZero];
    [cell.contentView.layer setShadowOpacity:.6];
    [cell.contentView.layer setShadowRadius:kShadowRadius];
    [cell.contentView.layer setRasterizationScale:[[UIScreen mainScreen] scale]];
    [cell.contentView.layer setShouldRasterize:YES];
    
    MHPresenterImageView *imageView = [[MHPresenterImageView alloc] init];
    [imageView setImage:[self imageForItemAtIndexPath:indexPath]];
    
    __weak SYGalleryThumbsView *wSelf = self;
    __weak MHPresenterImageView *wImageView = imageView;

    [imageView setInseractiveGalleryPresentionWithItems:self.galleryItems
                                      currentImageIndex:indexPath.item
                                  currentViewController:self.parentViewController
                                         finishCallback:
     ^(NSInteger currentIndex, UIImage *image, MHTransitionDismissMHGallery *interactiveTransition, MHGalleryViewMode viewMode)
    {
        if (viewMode == MHGalleryViewModeOverView) {
            [wSelf.parentViewController dismissViewControllerAnimated:YES completion:nil];
        }else{
            [wSelf.parentViewController.presentedViewController dismissViewControllerAnimated:YES
                                                                             dismissImageView:wImageView
                                                                                   completion:nil];
        }
    }];
    [imageView setContentMode:UIViewContentModeScaleAspectFit];
    [imageView setShoudlUsePanGestureReconizer:NO];
    [cell.contentView addSubview:imageView];
    
    [imageView mas_makeConstraints:^(MASConstraintMaker *make) {
        make.edges.equalTo(@0);
    }];
    
    return cell;
}

- (CGSize)collectionView:(UICollectionView *)collectionView layout:(UICollectionViewLayout *)collectionViewLayout sizeForItemAtIndexPath:(NSIndexPath *)indexPath
{
    UIImage *image = [self imageForItemAtIndexPath:indexPath];

    CGFloat availableHeight = self.bounds.size.height - 4 * kShadowRadius;
    CGSize size = CGSizeMake(availableHeight, availableHeight);
    size.width = AVMakeRectWithAspectRatioInsideRect(image.size, (CGRect){CGPointZero, size}).size.width;
    
    return size;
}

#warning centrer si pas assez de photos et/ou mettre marge gauche et droite pour avoir la première image centrée
@end
