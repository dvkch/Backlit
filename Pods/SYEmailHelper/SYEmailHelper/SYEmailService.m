//
//  SYEmailService.m
//  SYEmailHelper
//
//  Created by Stanislas Chevallier on 08/02/2017.
//  Copyright © 2017 Syan. All rights reserved.
//

#import "SYEmailService.h"
#import "SYEmailHelper.h"
#import <MessageUI/MessageUI.h>

@implementation SYEmailService

+ (NSArray <SYEmailService *> *)allServices
{
    NSMutableArray <SYEmailService *> *services = [NSMutableArray array];
    
    [services addObjectsFromArray:
     [SYEmailServiceApp allThirdPartyApps]];
    
    [services addObject:
     [SYEmailServiceNative new]];
    
    [services addObject:
     [SYEmailServicePasteboard new]];
    
    return [services copy];
}

+ (NSArray <SYEmailService *> *)availableServices
{
    return [[self allServices] filteredArrayUsingPredicate:[NSPredicate predicateWithBlock:^BOOL(SYEmailService *service, id _) {
        return service.isAvailable;
    }]];
}

- (BOOL)isAvailable
{
    return NO;
}

- (void)openWithAddress:(NSString *)address
                subject:(NSString *)subject
                   body:(NSString *)body
               parentVC:(UIViewController *)parentVC
             completion:(void (^)(BOOL, NSError *))completion
{
    if (completion)
        completion(NO, nil);
}

@end

#pragma mark - Third party apps

@implementation SYEmailServiceApp

+ (NSArray <SYEmailServiceApp *> *)allThirdPartyApps
{
    NSMutableArray <SYEmailServiceApp *> *services = [NSMutableArray array];
    
    [services addObject:[self gmailService]];
    [services addObject:[self googleInboxService]];
    [services addObject:[self microsoftOutlookService]];
    [services addObject:[self sparkService]];
    
    return [services copy];
}

+ (NSArray <SYEmailServiceApp *> *)availableThirdPartyApps
{
    return [[self allThirdPartyApps] filteredArrayUsingPredicate:[NSPredicate predicateWithBlock:^BOOL(SYEmailServiceApp *service, id _) {
        return service.isAvailable;
    }]];
}

+ (instancetype)gmailService
{
    return [SYEmailServiceApp serviceWithName:@"Gmail"
                                      baseURL:@"googlegmail://co"
                             parameterAddress:@"to"
                             parameterSubject:@"subject"
                                parameterBody:@"body"
                            parameterCallback:nil];
}

+ (instancetype)googleInboxService
{
    return [SYEmailServiceApp serviceWithName:@"Google Inbox"
                                      baseURL:@"inbox-gmail://co"
                             parameterAddress:@"to"
                             parameterSubject:@"subject"
                                parameterBody:@"body"
                            parameterCallback:nil];
}

+ (instancetype)microsoftOutlookService
{
    return [SYEmailServiceApp serviceWithName:@"Microsoft Outlook"
                                      baseURL:@"ms-outlook://compose"
                             parameterAddress:@"to"
                             parameterSubject:@"subject"
                                parameterBody:@"body"
                            parameterCallback:nil];
}

+ (instancetype)sparkService
{
    return [SYEmailServiceApp serviceWithName:@"Spark"
                                      baseURL:@"readdle-spark://compose"
                             parameterAddress:@"recipient"
                             parameterSubject:@"subject"
                                parameterBody:@"body"
                            parameterCallback:nil];
}

+ (instancetype)serviceWithName:(NSString *)name
                        baseURL:(NSString *)baseUrl
               parameterAddress:(NSString *)parameterAddress
               parameterSubject:(NSString *)parameterSubject
                  parameterBody:(NSString *)parameterBody
              parameterCallback:(NSString *)parameterCallback
{
    SYEmailServiceApp *instance = [[self alloc] init];
    instance.name                   = name;
    instance->_baseURL              = baseUrl;
    instance->_parameterBody        = parameterBody;
    instance->_parameterAddress     = parameterAddress;
    instance->_parameterSubject     = parameterSubject;
    instance->_parameterCallback    = parameterCallback;
    return instance;
}

- (BOOL)isAvailable
{
    return [[UIApplication sharedApplication] canOpenURL:[NSURL URLWithString:self.baseURL]];
}

- (void)openWithAddress:(NSString *)address
                subject:(NSString *)subject
                   body:(NSString *)body
               parentVC:(UIViewController *)parentVC
             completion:(void(^)(BOOL openedApp, NSError *nativeError))completion
{
    NSURLComponents *components = [NSURLComponents componentsWithString:self.baseURL];
    NSMutableArray <NSURLQueryItem *> *queryItems = [NSMutableArray array];
    
    if (self.parameterAddress && address)
        [queryItems addObject:[NSURLQueryItem queryItemWithName:self.parameterAddress value:address]];
    
    if (self.parameterSubject && subject)
        [queryItems addObject:[NSURLQueryItem queryItemWithName:self.parameterSubject value:subject]];
    
    if (self.parameterBody)
        [queryItems addObject:[NSURLQueryItem queryItemWithName:self.parameterBody value:body]];
    
    /*
    if (self.parameterCallback)
    {
        [queryItems addObject:[NSURLQueryItem queryItemWithName:@"x-source" value:@"AppName"]];
        [queryItems addObject:[NSURLQueryItem queryItemWithName:@"x-success" value:@"AppScheme://"]];
    }
    */
    
    components.queryItems = [queryItems copy];

    BOOL result = [[UIApplication sharedApplication] openURL:components.URL];
    
    NSError *error = nil;
    if (!result)
        error = [NSError errorWithDomain:SYEmailHelperErrorDomain
                                    code:SYEmailHelperErrorCode_CouldntOpenApp
                                userInfo:@{NSLocalizedDescriptionKey:@"Couldn't open app"}];
    
    if (completion)
        completion(NO, error);
}

@end

#pragma mark - In-App Native dialog

@interface SYEmailServiceNative () <MFMailComposeViewControllerDelegate>
@property (nonatomic, strong) id selfRetain;
@property (nonatomic, copy) void(^completionBlock)(BOOL openedApp, NSError *nativeError);
@end

@implementation SYEmailServiceNative

- (instancetype)init
{
    self = [super init];
    if (self)
    {
        self.name = @"Apple Mail";
        self.completionBlock = nil;
    }
    return self;
}

- (BOOL)isAvailable
{
    return [MFMailComposeViewController canSendMail];
}

- (void)openWithAddress:(NSString *)address
                subject:(NSString *)subject
                   body:(NSString *)body
               parentVC:(UIViewController *)parentVC
             completion:(void(^)(BOOL openedApp, NSError *nativeError))completion
{
    self.selfRetain = self;
    self.completionBlock = completion;
    
    MFMailComposeViewController *vc = [[MFMailComposeViewController alloc] init];
    [vc setMailComposeDelegate:self];
    [vc setSubject:subject];
    if (address)
        [vc setToRecipients:@[address]];
    [vc setMessageBody:body isHTML:NO];
    
    [parentVC presentViewController:vc animated:YES completion:nil];
}

- (void)mailComposeController:(MFMailComposeViewController *)controller didFinishWithResult:(MFMailComposeResult)result error:(NSError *)error
{
    [controller dismissViewControllerAnimated:YES completion:nil];
    
    if (self.completionBlock)
        self.completionBlock(result == MFMailComposeResultCancelled, error);
    
    self.completionBlock = nil;
    self.selfRetain = nil;
}

@end

#pragma mark - Copy to clipboard

@implementation SYEmailServicePasteboard

static NSString *serviceName;

+ (NSString *)name
{
    return serviceName;
}

+ (void)setName:(NSString *)name
{
    serviceName = name;
}

- (instancetype)init
{
    self = [super init];
    if (self)
    {
        self.name = (serviceName ?: @"Copy address to pasteboard");
    }
    return self;
}

- (BOOL)isAvailable
{
    return YES;
}

- (void)openWithAddress:(NSString *)address subject:(NSString *)subject parentVC:(UIViewController *)parentVC completion:(void (^)(BOOL, NSError *))completion
{
    [[UIPasteboard generalPasteboard] setString:address];
    if (completion)
        completion(NO, nil);
}

@end


