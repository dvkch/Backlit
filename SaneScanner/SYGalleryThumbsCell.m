//
//  SYGalleryThumbsCell.m
//  SaneScanner
//
//  Created by Stan Chevallier on 18/03/2016.
//  Copyright © 2016 Syan. All rights reserved.
//

#import "SYGalleryThumbsCell.h"
#import "SYGalleryManager.h"
#import <MHPresenterImageView.h>
#import <Masonry.h>
#import "MHUICustomization+SY.h"
#import "SYGalleryController.h"

static CGFloat const kShadowRadius = 2;

@interface SYGalleryThumbsCell ()
@end

@implementation SYGalleryThumbsCell

- (instancetype)initWithFrame:(CGRect)frame
{
    self = [super initWithFrame:frame];
    if (self)
    {
        [self.contentView.layer setShadowColor:[UIColor blackColor].CGColor];
        [self.contentView.layer setShadowOffset:CGSizeZero];
        [self.contentView.layer setShadowOpacity:.6];
        [self.contentView.layer setShadowRadius:kShadowRadius];
        [self.contentView.layer setRasterizationScale:[[UIScreen mainScreen] scale]];
        [self.contentView.layer setShouldRasterize:YES];
        
        self.imageView = [[MHPresenterImageView alloc] init];
        [self.imageView setContentMode:UIViewContentModeScaleAspectFit];
        [self.imageView setShoudlUsePanGestureReconizer:NO];
        [self.contentView addSubview:self.imageView];
        
        [self.imageView mas_makeConstraints:^(MASConstraintMaker *make) {
            make.edges.equalTo(@0);
        }];
    }
    return self;
}

- (void)updateWithItems:(NSArray<MHGalleryItem *> *)items
                  index:(NSUInteger)index
       parentController:(UIViewController *)parentController
           dismissBlock:(UIImageView *(^)(NSUInteger))dismissBlock
{
    __weak UIViewController *wParentController = parentController;
    UIImageView *(^dismissBlockCopy)(NSUInteger) = [dismissBlock copy];
    
    [self.imageView setImage:[[SYGalleryManager shared] thumbnailForItem:items[index]]];
    [self.imageView setUICustomization:[MHUICustomization sy_defaultTheme]];
    [self.imageView setGalleryClass:SYGalleryController.class];
    [self.imageView setInseractiveGalleryPresentionWithItems:items
                                           currentImageIndex:index
                                       currentViewController:parentController
                                              finishCallback:
     ^(NSInteger currentIndex, UIImage *image,
       MHTransitionDismissMHGallery *interactiveTransition,
       MHGalleryViewMode viewMode)
     {
         if (viewMode == MHGalleryViewModeOverView)
             [wParentController dismissViewControllerAnimated:YES completion:nil];
         else
         {
             UIImageView *dismissImageView = nil;
             if (dismissBlockCopy)
                 dismissImageView = dismissBlockCopy(currentIndex);
             
             [wParentController.presentedViewController dismissViewControllerAnimated:YES
                                                                     dismissImageView:dismissImageView
                                                                           completion:nil];
         }
     }];
}

@end
