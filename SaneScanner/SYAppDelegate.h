//
//  SYAppDelegate.h
//  SaneScanner
//
//  Created by rominet on 06/05/15.
//  Copyright (c) 2015 Syan. All rights reserved.
//

#import <UIKit/UIKit.h>

typedef enum : NSUInteger {
    SYSnapshotType_None,
    SYSnapshotType_DevicePreview,
    SYSnapshotType_DeviceOptions,
    SYSnapshotType_DeviceOptionPopup,
    SYSnapshotType_Other,
} SYSnapshotType;

@interface SYAppDelegate : UIResponder <UIApplicationDelegate>

@property (nonatomic, strong) UIWindow *window;
@property (nonatomic, assign, readonly) SYSnapshotType snapshotType;

+ (instancetype)obtain;

- (void)splitVCtraitCollectionWillChangeTo:(UITraitCollection *)traitCollection;

@end

