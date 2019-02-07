//
//  SYGalleryThumbsCell.m
//  SaneScanner
//
//  Created by Stan Chevallier on 18/03/2016.
//  Copyright © 2016 Syan. All rights reserved.
//

#import "SYGalleryThumbsCell.h"
#import "SaneScanner-Swift.h"
#import <MHPresenterImageView.h>
#import <Masonry.h>
#import "MHUICustomization+SY.h"
#import "SYGalleryController.h"

static CGFloat const kShadowRadius = 2;

@interface SYGalleryThumbsCell () <GalleryManagerDelegate>
@property (nonatomic, strong) MHGalleryItem *item;
@property (nonatomic, strong) UIActivityIndicatorView *spinner;
@end

@implementation SYGalleryThumbsCell

- (instancetype)initWithFrame:(CGRect)frame
{
    self = [super initWithFrame:frame];
    if (self)
    {
        [self.contentView.layer setShadowColor:[UIColor blackColor].CGColor];
        [self.contentView.layer setShadowOffset:CGSizeZero];
        [self.contentView.layer setShadowRadius:kShadowRadius];
        [self.contentView.layer setRasterizationScale:[[UIScreen mainScreen] scale]];
        
        self.imageView = [[MHPresenterImageView alloc] init];
        [self.imageView setContentMode:UIViewContentModeScaleAspectFit];
        [self.imageView setShoudlUsePanGestureReconizer:NO];
        [self.contentView addSubview:self.imageView];
        
        self.spinner = [[UIActivityIndicatorView alloc] initWithActivityIndicatorStyle:UIActivityIndicatorViewStyleWhite];
        [self.spinner setHidesWhenStopped:YES];
        [self.contentView addSubview:self.spinner];
        
        [self.imageView mas_makeConstraints:^(MASConstraintMaker *make) {
            make.edges.equalTo(@0);
        }];
        
        [self.spinner mas_makeConstraints:^(MASConstraintMaker *make) {
            make.center.equalTo(@0);
        }];
        
        [GalleryManager.shared addDelegate:self];
    }
    return self;
}

- (void)galleryManager:(GalleryManager *)manager didCreate:(UIImage *)thumbnail for:(MHGalleryItem *)item
{
    if (![item isEqual:self.item])
        return;
    [self setImage:thumbnail];
}

- (void)setImage:(UIImage *)image
{
    [self.imageView setImage:image];
    if (image)
    {
        [self.spinner stopAnimating];
        [self.contentView.layer setShadowOpacity:.6];
        [self.contentView.layer setShouldRasterize:YES];
        //[self setBackgroundColor:[UIColor redColor]];
    }
    else
    {
        [self.spinner startAnimating];
        [self.contentView.layer setShadowOpacity:0];
        [self.contentView.layer setShouldRasterize:NO];
        //[self setBackgroundColor:[UIColor greenColor]];
    }
}

- (void)updateWithItems:(NSArray<MHGalleryItem *> *)items
                  index:(NSUInteger)index
       parentController:(UIViewController *)parentController
           spinnerColor:(UIColor *)spinnerColor
           dismissBlock:(UIImageView *(^)(NSUInteger))dismissBlock
{
    __weak UIViewController *wParentController = parentController;
    UIImageView *(^dismissBlockCopy)(NSUInteger) = [dismissBlock copy];
    
    [self.spinner setColor:spinnerColor];
    
    self.item = items[index];
    [self setImage:[GalleryManager.shared thumbnailFor:self.item]];
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
