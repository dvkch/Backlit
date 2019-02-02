//
//  UIScrollView+SY.m
//  SaneScanner
//
//  Created by Stanislas Chevallier on 02/02/2019.
//  Copyright © 2019 Syan. All rights reserved.
//

#import "UIScrollView+SY.h"
#import <Masonry.h>
#import <objc/runtime.h>
#import "UIColor+SY.h"

@implementation UIScrollView (SY)

#pragma mark - Refresh control property

- (UIRefreshControl *)sy_refreshControl
{
    return objc_getAssociatedObject(self, @selector(sy_refreshControl));
}

- (void)sy_setRefreshControl:(UIRefreshControl *)control
{
    objc_setAssociatedObject(self, @selector(sy_refreshControl), control, OBJC_ASSOCIATION_RETAIN_NONATOMIC);
}

- (void(^)(UIScrollView *))sy_refreshControlAction
{
    return objc_getAssociatedObject(self, @selector(sy_refreshControlAction));
}

- (void)sy_setRefreshControlAction:(void(^)(UIScrollView *))action
{
    objc_setAssociatedObject(self, @selector(sy_refreshControlAction), action, OBJC_ASSOCIATION_COPY_NONATOMIC);
}

- (void)sy_addPullToResfreshWithBlock:(void (^)(UIScrollView *))block
{
    [self sy_setRefreshControlAction:block];
    
    [self.sy_refreshControl removeFromSuperview];
    [self sy_setRefreshControl:nil];
    
    // TODO: use cuter control
    
    /*
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
     */
    
    UIRefreshControl *control = [[UIRefreshControl alloc] init];
    control.tintColor = [[UIColor groupTableViewHeaderTitleColor] colorWithAlphaComponent:0.8];
    control.attributedTitle = nil;
    control.backgroundColor = UIColor.clearColor;
    [control addTarget:self action:@selector(sy_refreshControlValueChanged) forControlEvents:UIControlEventValueChanged];
    
    if (@available(iOS 10, *)) {
        self.refreshControl = control;
    }
    else {
        [self addSubview:control];
    }
    
    [self sy_setRefreshControl:control];
}

- (void)sy_refreshControlValueChanged
{
    void(^block)(UIScrollView *) = self.sy_refreshControlAction;
    if (block)
        block(self);
}

- (void)sy_showPullToRefreshAndRunBlock:(BOOL)runBlock
{
    [self.sy_refreshControl beginRefreshing];
    // 60 is the average height for the refreshControl. we can't use UIRefreshControl.frame.size.height because
    // when it is closed its height is 0.5px...
    [self setContentOffset:CGPointMake(0, -60) animated:YES];
    
    if (runBlock && self.sy_refreshControlAction) {
        self.sy_refreshControlAction(self);
    }
}

- (void)sy_endPullToRefresh
{
    [self.sy_refreshControl endRefreshing];
}

@end
