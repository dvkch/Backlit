//
//  SYAppDelegate.h
//  SaneScanner
//
//  Created by rominet on 06/05/15.
//  Copyright (c) 2015 Syan. All rights reserved.
//

#import <UIKit/UIKit.h>

@interface SYAppDelegate : UIResponder <UIApplicationDelegate>

@property (nonatomic, strong) UIWindow *window;

+ (instancetype)obtain;

- (void)splitVCtraitCollectionWillChangeTo:(UITraitCollection *)traitCollection;

@end

