//
//  SYEmailHelper.h
//  SYEmailHelper
//
//  Created by Stanislas Chevallier on 07/02/2017.
//  Copyright © 2017 Syan. All rights reserved.
//

#import <UIKit/UIKit.h>
#import "SYEmailService.h"

extern NSString * const SYEmailHelperErrorDomain;

typedef enum : NSUInteger {
    SYEmailHelperErrorCode_NoServiceAvailable,
    SYEmailHelperErrorCode_CouldntOpenApp,
} SYEmailHelperErrorCode;

@interface SYEmailHelper : NSObject

@property (nonatomic, strong) NSString *actionSheetTitleText;
@property (nonatomic, strong) NSString *actionSheetCancelButtonText;
@property (nonatomic, assign) BOOL showCopyToPasteboard;

+ (instancetype)shared;

- (void)composeEmailWithAddress:(NSString *)address
                        subject:(NSString *)subject
                           body:(NSString *)body
                   presentingVC:(UIViewController *)presentingVC
                     completion:(void(^)(BOOL userCancelled, SYEmailService *service, NSError *error))completion;

@end
