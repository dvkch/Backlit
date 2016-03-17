//
//  SYGalleryThumbsView.h
//  SaneScanner
//
//  Created by Stan Chevallier on 14/03/2016.
//  Copyright © 2016 Syan. All rights reserved.
//

#import <UIKit/UIKit.h>

@interface SYGalleryThumbsView : UIView

@property (nonatomic, weak) UIViewController *parentViewController;

+ (instancetype)showInToolbarOfController:(UIViewController *)controller;

@end
