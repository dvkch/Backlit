//
//  UIApplication+IFM.m
//  iOSFileManager
//
//  Created by John Wong on 9/12/15.
//  Copyright (c) 2015 John Wong. All rights reserved.
//

#import "UIApplication+IFM.h"
#import "IFMDebugTool.h"

#import <objc/runtime.h>

@implementation UIApplication (IFM)

+ (void)load {
    Method original = class_getInstanceMethod(self, @selector(sendEvent:));
    Method swizzled = class_getInstanceMethod(self, @selector(ifm_sendEvent:));
    method_exchangeImplementations(original, swizzled);
}

- (void)ifm_sendEvent:(UIEvent *)event {
    [self ifm_sendEvent:event];
    if( event && (event.subtype == UIEventSubtypeMotionShake)) {
        static UIAlertView *alertView = nil;
        if (!alertView.visible) {
            alertView = [[UIAlertView alloc] initWithTitle:@"IFMDebugTool" message:[IFMDebugTool sharedInstance].accessUrl delegate:nil cancelButtonTitle:@"OK" otherButtonTitles:nil];
            [alertView show];
        }
    }
}



@end
