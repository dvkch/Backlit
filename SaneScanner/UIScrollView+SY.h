//
//  UIScrollView+SY.h
//  SaneScanner
//
//  Created by Stanislas Chevallier on 02/02/2019.
//  Copyright © 2019 Syan. All rights reserved.
//

#import <UIKit/UIKit.h>

@interface UIScrollView (SY)

- (void)sy_addPullToResfreshWithBlock:(void(^)(UIScrollView *))block;
- (void)sy_showPullToRefreshAndRunBlock:(BOOL)runBlock;
- (void)sy_endPullToRefresh;

@end
