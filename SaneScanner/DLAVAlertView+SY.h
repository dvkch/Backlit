//
//  DLAVAlertView+SY.h
//  SaneScanner
//
//  Created by Stan Chevallier on 06/03/2016.
//  Copyright © 2016 Syan. All rights reserved.
//

#import <DLAVAlertView.h>

@interface DLAVAlertView (SY)

- (void)setText:(NSString *)text forButtonAtIndex:(NSUInteger)index;

- (NSUInteger)lastOtherButtonIndex;
- (void)addSliderWithMin:(float)min
                     max:(float)max
                 current:(float)current
             updateBlock:(void(^)(DLAVAlertView *alertView, float value))block;

- (void)addImageViewForImage:(UIImage *)image;

@end
