//
//  SYAppDelegate.h
//  SaneScanner
//
//  Created by rominet on 06/05/15.
//  Copyright (c) 2015 Syan. All rights reserved.
//

#import <UIKit/UIKit.h>

typedef NS_ENUM(NSUInteger, SYSnapshotType) {
    SYSnapshotTypeNone,
    SYSnapshotTypeDevicePreview,
    SYSnapshotTypeDeviceOptions,
    SYSnapshotTypeDeviceOptionPopup,
    SYSnapshotTypeOther,
} ;

@interface SYAppDelegate : UIResponder <UIApplicationDelegate>

@property (nonatomic, strong) UIWindow *window;
@property (nonatomic, assign, readonly) SYSnapshotType snapshotType;
@property (nonatomic, strong, readonly) NSString *snapshotTestScanImagePath;

@property (nonatomic, readonly, nonnull, class) SYAppDelegate *obtain;

- (void)splitVCtraitCollectionWillChangeTo:(UITraitCollection *)traitCollection;

@end

