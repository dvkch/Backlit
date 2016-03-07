//
//  SYSaneHelper.h
//  SaneScanner
//
//  Created by rominet on 06/05/15.
//  Copyright (c) 2015 Syan. All rights reserved.
//

#import <UIKit/UIKit.h>

@class SYSaneDevice;
@class SYSaneOption;
@class SYSaneHelper;

@protocol SYSaneHelperDelegate <NSObject>

- (void)saneHelperDidStartUpdatingDevices:(SYSaneHelper *)saneHelper;
- (void)saneHelperDidEndUpdatingDevices:(SYSaneHelper *)saneHelper;
- (void)saneHelper:(SYSaneHelper *)saneHelper
needsAuthForDevice:(NSString *)device
       outUsername:(NSString **)username
       outPassword:(NSString **)password;

@end

@interface SYSaneHelper : NSObject

@property (nonatomic, weak) id<SYSaneHelperDelegate> delegate;
@property (nonatomic, assign) BOOL isUpdatingDevices;
@property (atomic, strong) NSString *saneInitError;
@property (atomic, strong) NSString *saneLastGetDevicesError;

+ (instancetype)shared;

- (NSArray <NSString *> *)hostsCopy;
- (NSUInteger)hostsCount;
- (NSString *)hostAtIndex:(NSUInteger)index;
- (NSArray *)addHost:(NSString *)host;
- (NSArray *)removeHostAtIndex:(NSUInteger)index;

- (NSArray <SYSaneDevice *> *)devicesCopy;
- (NSUInteger)devicesCount;
- (SYSaneDevice *)deviceAtIndex:(NSUInteger)index;
- (void)updateDevices;

- (void)openDevice:(SYSaneDevice *)device block:(void(^)(NSString *error))block;
- (void)closeDevice:(SYSaneDevice *)device;

- (void)listOptionsForDevice:(SYSaneDevice *)device block:(void(^)(void))block;

- (void)getValueForOption:(SYSaneOption *)option
                    block:(void(^)(id value, NSString *error))block;

- (void)setValue:(id)value
     orAutoValue:(BOOL)autoValue
       forOption:(SYSaneOption *)option
 thenReloadValue:(BOOL)reloadValue
           block:(void(^)(BOOL reloadAllOptions, NSString *error))block;

- (void)scanWithDevice:(SYSaneDevice *)device
         progressBlock:(void(^)(float progress))progressBlock
          successBlock:(void(^)(UIImage *image, NSString *error))successBlock;

@end
