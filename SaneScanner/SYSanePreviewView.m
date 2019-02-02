//
//  SYSanePreviewView.m
//  SaneScanner
//
//  Created by Stan Chevallier on 08/03/2016.
//  Copyright © 2016 Syan. All rights reserved.
//

#import "SYSanePreviewView.h"
#import "SYCropMaskView.h"
#import <SaneSwift/SaneSwift-umbrella.h>
#import "SYSaneDevice.h"
#import <Masonry.h>
#import "SVProgressHUD.h"

static CGFloat const kMargin = 15.;

@interface SYSanePreviewView ()
@property (nonatomic, strong) UIImageView *imageView;
@property (nonatomic, strong) UIView *lineView;
@property (nonatomic, strong) SYCropMaskView *cropMaskView;
@property (nonatomic, strong) UIButton *buttonAcquirePreview;
@end

@implementation SYSanePreviewView

- (instancetype)initWithFrame:(CGRect)frame
{
    self = [super initWithFrame:frame];
    if (self) [self customInit];
    return self;
}

- (instancetype)initWithCoder:(NSCoder *)aDecoder
{
    self = [super initWithCoder:aDecoder];
    if (self) [self customInit];
    return self;
}

- (void)customInit
{
    if (self.imageView)
        return;
    
    [self setBackgroundColor:[UIColor clearColor]];
    
    self.imageView = [[UIImageView alloc] init];
    [self.imageView setContentMode:UIViewContentModeScaleAspectFit];
    [self.imageView setBackgroundColor:[UIColor whiteColor]];
    [self.imageView.layer setShadowColor:[UIColor blackColor].CGColor];
    [self.imageView.layer setShadowOffset:CGSizeZero];
    [self.imageView.layer setShadowRadius:5.f];
    [self.imageView.layer setShadowOpacity:0.3f];
    [self.imageView.layer setShouldRasterize:YES];
    [self.imageView.layer setRasterizationScale:[[UIScreen mainScreen] scale]];
    [self addSubview:self.imageView];
    
    self.lineView = [[UIView alloc] init];
    [self.lineView setBackgroundColor:[UIColor colorWithRed:200./255. green:199./255. blue:204./255. alpha:1.]];
    [self addSubview:self.lineView];
    
    @weakify(self);
    self.cropMaskView = [[SYCropMaskView alloc] init];
    [self.cropMaskView setCropAreaDidChangeBlock:^(CGRect newCropArea) {
        @strongify(self)
        self.device.cropArea = newCropArea;
    }];
    [self addSubview:self.cropMaskView];
    
    self.buttonAcquirePreview = [UIButton buttonWithType:UIButtonTypeCustom];
    [self.buttonAcquirePreview setTitle:$("DEVICE BUTTON UPDATE PREVIEW") forState:UIControlStateNormal];
    [self.buttonAcquirePreview setTitleColor:[UIColor blackColor] forState:UIControlStateNormal];
    [self.buttonAcquirePreview setBackgroundColor:[UIColor whiteColor]];
    [self.buttonAcquirePreview.titleLabel setFont:[UIFont systemFontOfSize:17]];
    [self.buttonAcquirePreview addTarget:self action:@selector(buttonAcquirePreviewTap:)
                        forControlEvents:UIControlEventTouchUpInside];
    [self addSubview:self.buttonAcquirePreview];
    
    [self.cropMaskView mas_makeConstraints:^(MASConstraintMaker *make) {
        make.edges.equalTo(self.imageView);
    }];
    
    [self.lineView mas_makeConstraints:^(MASConstraintMaker *make) {
        make.height.equalTo(@(0.5));
        make.left.equalTo(@15);
        make.right.equalTo(@(-15));
        make.bottom.equalTo(self.buttonAcquirePreview.mas_top);
    }];
    
    [self.buttonAcquirePreview mas_makeConstraints:^(MASConstraintMaker *make) {
        make.height.equalTo(@40);
        make.left.equalTo(@0);
        make.right.equalTo(@0);
        make.bottom.equalTo(@0);
    }];
    
    [self.imageView mas_makeConstraints:^(MASConstraintMaker *make) {
        make.top.equalTo(@(kMargin));
        make.centerX.equalTo(@0);
        make.left.greaterThanOrEqualTo(@(kMargin));
        make.right.lessThanOrEqualTo(@(-kMargin));
        make.bottom.lessThanOrEqualTo(self.buttonAcquirePreview.mas_top).offset(-kMargin);
        make.bottom.equalTo(self.buttonAcquirePreview.mas_top).offset(-kMargin).priorityLow();
    }];
    
    [self setNeedsUpdateConstraints];
}

- (void)updateConstraints
{
    [super updateConstraints];
    
    CGFloat ratio = self.device.previewImageRatio;
    if (ratio <= 0.)
        ratio = 3./4.;
    
    [self.imageView mas_updateConstraints:^(MASConstraintMaker *make) {
        make.width.equalTo(self.imageView.mas_height).multipliedBy(ratio);
    }];
}

#pragma mark - Data

- (void)refresh
{
    self.cropMaskView.hidden = !self.device.canCrop;
    [self.imageView setImage:self.device.lastPreviewImage];
    [self.cropMaskView setCropArea:self.device.cropArea andMaxCropArea:self.device.maxCropArea];
}

- (void)setDevice:(SYSaneDevice *)device
{
    self->_device = device;
    [self refresh];
}

#pragma mark - IBActions

- (void)buttonAcquirePreviewTap:(id)sender
{
    [SVProgressHUD showWithStatus:$("PREVIEWING")];
    [Sane.shared previewWithDevice:self.device progress:^(float progress, UIImage * _Nullable incompleteImage) {
        [self.imageView setImage:incompleteImage];
        [SVProgressHUD showProgress:progress];
    } completion:^(UIImage * _Nullable image, NSError * _Nullable error) {
        if (error)
            [SVProgressHUD showErrorWithStatus:error.sy_alertMessage];
        else
            [SVProgressHUD dismiss];
        
        [self refresh];
    }];
}


@end
