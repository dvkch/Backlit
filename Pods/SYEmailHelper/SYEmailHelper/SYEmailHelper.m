//
//  SYEmailHelper.m
//  SYEmailHelper
//
//  Created by Stanislas Chevallier on 07/02/2017.
//  Copyright © 2017 Syan. All rights reserved.
//

#import "SYEmailHelper.h"
#import "SYEmailService.h"
#import <MessageUI/MessageUI.h>

NSString * const SYEmailHelperErrorDomain = @"SYEmailHelperErrorDomain";

@interface SYEmailHelper ()
@end

@implementation SYEmailHelper

+ (instancetype)shared
{
    static dispatch_once_t onceToken;
    static SYEmailHelper *instance;
    dispatch_once(&onceToken, ^{
        instance = [[self alloc] init];
    });
    return instance;
}

- (instancetype)init
{
    self = [super init];
    if (self)
    {
        self.showCopyToPasteboard = YES;
    }
    return self;
}

- (void)composeEmailWithAddress:(NSString *)address
                        subject:(NSString *)subject
                           body:(NSString *)body
                   presentingVC:(UIViewController *)presentingVC
                     completion:(void(^)(BOOL userCancelled, SYEmailService *service, NSError *error))completion
{
    NSArray <SYEmailService *> *services = [SYEmailService availableServices];
    
    if (!self.showCopyToPasteboard)
    {
        services =
        [services filteredArrayUsingPredicate:[NSPredicate predicateWithBlock:^BOOL(id service, id _) {
            return ![service isKindOfClass:[SYEmailServicePasteboard class]];
        }]];
    }
    
    if (!services.count)
    {
        if (completion)
            completion(NO, nil, [NSError errorWithDomain:SYEmailHelperErrorDomain
                                                    code:SYEmailHelperErrorCode_NoServiceAvailable
                                                userInfo:@{NSLocalizedDescriptionKey:@"No services found"}]);
        return;
    }

    if (services.count == 1)
    {
        [services.firstObject openWithAddress:address subject:subject body:body parentVC:presentingVC completion:^(BOOL userCancelled, NSError *nativeError) {
            if (completion)
                completion(userCancelled, services.firstObject, nativeError);
        }];
        return;
    }
    
    UIAlertController *actionSheet = [UIAlertController alertControllerWithTitle:self.actionSheetTitleText
                                                                         message:nil
                                                                  preferredStyle:UIAlertControllerStyleActionSheet];
    for (SYEmailService *service in services)
    {
        [actionSheet addAction:
         [UIAlertAction actionWithTitle:service.name style:UIAlertActionStyleDefault handler:^(UIAlertAction * _Nonnull action)
        {
            [service openWithAddress:address subject:subject body:body parentVC:presentingVC completion:^(BOOL userCancelled, NSError *nativeError) {
                if (completion)
                    completion(userCancelled, service, nativeError);
            }];
        }]];
    }

    [actionSheet addAction:
     [UIAlertAction actionWithTitle:(self.actionSheetCancelButtonText ?: @"Cancel")
                              style:UIAlertActionStyleCancel
                            handler:^(UIAlertAction * _Nonnull action)
    {
        if (completion)
            completion(YES, nil, nil);
    }]];
    
    [presentingVC presentViewController:actionSheet animated:YES completion:nil];
}

@end

