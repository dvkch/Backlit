//
//  SYSplitVC.m
//  SaneScanner
//
//  Created by Stan Chevallier on 17/03/2016.
//  Copyright © 2016 Syan. All rights reserved.
//

#import "SYSplitVC.h"
#import "SYAppDelegate.h"

@implementation SYSplitVC

- (void)willTransitionToTraitCollection:(UITraitCollection *)newCollection
              withTransitionCoordinator:(id<UIViewControllerTransitionCoordinator>)coordinator
{
    [super willTransitionToTraitCollection:newCollection withTransitionCoordinator:coordinator];
    [coordinator animateAlongsideTransition:^(id<UIViewControllerTransitionCoordinatorContext>  _Nonnull context) {
        [[SYAppDelegate obtain] updateToolbarAndGalleryForTraitCollection:newCollection];
    } completion:nil];
}

@end
