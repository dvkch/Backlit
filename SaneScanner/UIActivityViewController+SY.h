//
//  UIActivityViewController+SY.h
//  SaneScanner
//
//  Created by Stanislas Chevallier on 19/04/2017.
//  Copyright © 2017 Syan. All rights reserved.
//

#import <UIKit/UIKit.h>

@interface UIActivityViewController (SY)

+ (void)sy_showForUrls:(NSArray <NSURL *> *)urls
     fromBarButtonItem:(UIBarButtonItem *)barButtonItem
          presentingVC:(UIViewController *)presentingVC
            completion:(void(^)(void))completion;

+ (void)sy_showForUrls:(NSArray <NSURL *> *)urls
  bottomInPresentingVC:(UIViewController *)presentingVC
            completion:(void(^)(void))completion;

@end
