//
//  SYScrollMaskView.h
//  SYKit
//
//  Created by Stanislas Chevallier on 22/05/2017.
//  Copyright © 2017 Syan. All rights reserved.
//

#import <UIKit/UIKit.h>

typedef NS_ENUM(NSInteger, SYScrollMask) {
    SYScrollMaskNone,
    SYScrollMaskVertical,
    SYScrollMaskHorizontal,
};

/**
 * @class SYScrollMaskView
 *
 * Container for UIScrollView and subclasses, used to mask the top and
 * bottom (or left and right) of a scrollview. This creates an effect
 * that shows the user there is something so scroll to.
 *  
 * The only way to achieve that is to add the scrollView to this view, which
 * is itself masked to achieve the desired effect, since scrollViews can't be
 * directly masked (shows weird glitches, tableView cells are dropped, gradient
 * moves when we scroll etc)
 *
 * You should position this view as you would have your scrollView, set the
 * scrollView property (this will add the scrollView to this container) and
 * set the desired mask orientation and size.
 *
 */
@interface SYScrollMaskView : UIView

@property (nonatomic, weak) UIScrollView *scrollView;

@property (nonatomic, assign) SYScrollMask scrollMask;
@property (nonatomic, assign) CGFloat scrollMaskSize;

@end
