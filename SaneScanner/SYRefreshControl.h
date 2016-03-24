//
//  SYRefreshControl.h
//  SaneScanner
//
//  Created by Stan Chevallier on 24/03/2016.
//  Copyright © 2016 Syan. All rights reserved.
//

#import <UIKit/UIKit.h>
#import <INSPullToRefreshBackgroundView.h>
#import <UIScrollView+INSPullToRefresh.h>

@interface SYRefreshControl : UIView <INSPullToRefreshBackgroundViewDelegate>

+ (instancetype)addRefreshControlToScrollView:(UIScrollView *)scrollView
                                 triggerBlock:(void(^)(UIScrollView *))triggerBlock;

@end
