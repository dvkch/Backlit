//
//  SYSaneHelper.h
//  SaneScanner
//
//  Created by rominet on 06/05/15.
//  Copyright (c) 2015 Syan. All rights reserved.
//

#import <UIKit/UIKit.h>

@class SYSaneDevice;
@class SYSaneOptionDescriptor;

@interface SYSaneHelper : NSObject

@property (nonatomic, assign) BOOL isUpdatingDevices;
@property (atomic, strong) NSString *saneInitError;
@property (atomic, strong) NSString *saneLastGetDevicesError;
@property (atomic, copy) void(^startingDevicesUpdateBlock)(void);
@property (atomic, copy) void(^endedDevicesUpdateBlock)(void);

+ (instancetype)shared;

- (NSArray *)hostsCopy;
- (NSUInteger)hostsCount;
- (NSString *)hostAtIndex:(NSUInteger)index;
- (NSArray *)addHost:(NSString *)host;
- (NSArray *)removeHostAtIndex:(NSUInteger)index;

- (NSArray *)devicesCopy;
- (NSUInteger)devicesCount;
- (SYSaneDevice *)deviceAtIndex:(NSUInteger)index;
- (void)updateDevices;

- (void)openDevice:(SYSaneDevice *)device block:(void(^)(NSString *error))block;
- (void)closeDevice:(SYSaneDevice *)device;

- (void)listOptionsForDevice:(SYSaneDevice *)device block:(void(^)(void))block;

- (void)getValueForOption:(SYSaneOptionDescriptor *)option
                 onDevice:(SYSaneDevice *)device
                    block:(void(^)(id value, NSString *error))block;

- (void)scanWithDevice:(SYSaneDevice *)device block:(void(^)(UIImage *image, NSString *error))block;

@end
