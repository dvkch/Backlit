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
@class SYSaneScanParameters;

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

+ (instancetype)shared;

- (NSString *)translationForKey:(NSString *)key;

- (NSArray <NSString *> *)allHosts;
- (void)addHost:(NSString *)host;
- (void)removeHost:(NSString *)host;
- (void)clearHosts;

- (void)updateDevices:(void(^)(NSArray <SYSaneDevice *> *devices, NSError *error))block;

- (void)openDevice:(SYSaneDevice *)device block:(void(^)(NSError *error))block;
- (void)closeDevice:(SYSaneDevice *)device;

- (void)listOptionsForDevice:(SYSaneDevice *)device block:(void(^)(void))block;

- (void)getValueForOption:(SYSaneOption *)option
                    block:(void(^)(id value, NSError *error))block;

- (void)setValue:(id)value
     orAutoValue:(BOOL)autoValue
       forOption:(SYSaneOption *)option
           block:(void(^)(BOOL reloadAllOptions, NSError *error))block;

- (void)previewWithDevice:(SYSaneDevice *)device
            progressBlock:(void(^)(float progress, UIImage *incompleteImage))progressBlock
             successBlock:(void(^)(UIImage *image, NSError *error))successBlock;

- (void)scanWithDevice:(SYSaneDevice *)device
         progressBlock:(void(^)(float progress, UIImage *incompleteImage))progressBlock
          successBlock:(void(^)(UIImage *image, SYSaneScanParameters *parameters, NSError *error))successBlock;

- (void)stopCurrentScan;

@end
