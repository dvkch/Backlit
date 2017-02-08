//
//  SYEmailService.h
//  SYEmailHelper
//
//  Created by Stanislas Chevallier on 08/02/2017.
//  Copyright © 2017 Syan. All rights reserved.
//

#import <UIKit/UIKit.h>

@interface SYEmailService : NSObject

@property (nonatomic, strong) NSString *name;

+ (NSArray <SYEmailService *> *)allServices;
+ (NSArray <SYEmailService *> *)availableServices;

- (BOOL)isAvailable;

- (void)openWithAddress:(NSString *)address
                subject:(NSString *)subject
                   body:(NSString *)body
               parentVC:(UIViewController *)parentVC
             completion:(void(^)(BOOL userCancelled, NSError *nativeError))completion;

@end

@interface SYEmailServiceApp : SYEmailService

@property (nonatomic, strong, readonly) NSString *baseURL;
@property (nonatomic, strong, readonly) NSString *parameterAddress;
@property (nonatomic, strong, readonly) NSString *parameterSubject;
@property (nonatomic, strong, readonly) NSString *parameterBody;
@property (nonatomic, strong, readonly) NSString *parameterCallback;

+ (instancetype)serviceWithName:(NSString *)name
                        baseURL:(NSString *)baseUrl
               parameterAddress:(NSString *)parameterAddress
               parameterSubject:(NSString *)parameterSubject
                  parameterBody:(NSString *)parameterBody
              parameterCallback:(NSString *)parameterCallback;

+ (NSArray <SYEmailServiceApp *> *)allThirdPartyApps;
+ (NSArray <SYEmailServiceApp *> *)availableThirdPartyApps;

@end

@interface SYEmailServiceNative : SYEmailService
@end

@interface SYEmailServicePasteboard : SYEmailService
@property (nonatomic, class, strong) NSString *name;
@end

