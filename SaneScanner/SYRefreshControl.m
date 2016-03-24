//
//  SYRefreshControl.m
//  SaneScanner
//
//  Created by Stan Chevallier on 24/03/2016.
//  Copyright © 2016 Syan. All rights reserved.
//

#import "SYRefreshControl.h"
#import <RTSpinKitView.h>
#import "UIColor+SY.h"

static CGFloat const kControlSize = 70.;
static CGFloat const kSpinnerSize = 40.;

@interface SYRefreshControl ()
@property (nonatomic, strong) RTSpinKitView *spinner;
@end

@implementation SYRefreshControl

+ (instancetype)addRefreshControlToScrollView:(UIScrollView *)scrollView
                                 triggerBlock:(void(^)(UIScrollView *))triggerBlock
{
    CGRect frame = CGRectMake((scrollView.bounds.size.width - kControlSize)/2., 0, kControlSize, kControlSize);

    SYRefreshControl *control = [[self alloc] initWithFrame:frame];
    [control setAutoresizingMask:(UIViewAutoresizingFlexibleLeftMargin | UIViewAutoresizingFlexibleRightMargin)];
    
    [scrollView ins_addPullToRefreshWithHeight:kControlSize handler:triggerBlock];
    [scrollView.ins_pullToRefreshBackgroundView setDelegate:control];
    [scrollView.ins_pullToRefreshBackgroundView addSubview:control];
    
    return control;
}

- (instancetype)initWithFrame:(CGRect)frame
{
    self = [super initWithFrame:frame];
    if (self) [self setup];
    return self;
}

- (void)setup
{
    if (self.spinner)
        return;
    
    UIColor *color = [[UIColor groupTableViewHeaderTitleColor] colorWithAlphaComponent:0.8];
    self.spinner = [[RTSpinKitView alloc] initWithStyle:RTSpinKitViewStyleChasingDots
                                                  color:color
                                            spinnerSize:kSpinnerSize];
    [self.spinner setHidesWhenStopped:NO];
    [self.spinner setFrame:CGRectMake(0, 0, kSpinnerSize, kSpinnerSize)];
    [self.spinner setCenter:self.center];
    [self.spinner setAutoresizingMask:(UIViewAutoresizingFlexibleTopMargin|
                                       UIViewAutoresizingFlexibleLeftMargin|
                                       UIViewAutoresizingFlexibleRightMargin|
                                       UIViewAutoresizingFlexibleBottomMargin)];
    [self addSubview:self.spinner];
}

- (void)layoutSubviews
{
    [super layoutSubviews];
    
    CGFloat w = self.bounds.size.width;
    CGFloat h = self.bounds.size.height;
    
    [self.spinner setFrame:CGRectMake((w - kSpinnerSize)/2., (h - kSpinnerSize)/2., kSpinnerSize, kSpinnerSize)];
}

- (void)pullToRefreshBackgroundView:(INSPullToRefreshBackgroundView *)pullToRefreshBackgroundView
                     didChangeState:(INSPullToRefreshBackgroundViewState)state
{
    switch (state) {
        case INSPullToRefreshBackgroundViewStateNone:
            [self.spinner setAlpha:0];
            [self.spinner stopAnimating];
            break;
        case INSPullToRefreshBackgroundViewStateTriggered:
            [self.spinner setAlpha:1];
            [self.spinner startAnimating];
            break;
        case INSPullToRefreshBackgroundViewStateLoading:
            [self.spinner startAnimating];
            break;
    }
}

- (void)pullToRefreshBackgroundView:(INSPullToRefreshBackgroundView *)pullToRefreshBackgroundView
      didChangeTriggerStateProgress:(CGFloat)progress
{
    // dont know why but an opacity animation happens at start and screws up all that is following here
    [self.spinner.layer removeAllAnimations];
    
    CGFloat minProgress = 0.2;
    CGFloat maxProgress = 1.0;
    CGFloat newProgress = MAX(0, progress - minProgress)/(maxProgress - minProgress);
    [self.spinner setAlpha:newProgress];
}

@end
