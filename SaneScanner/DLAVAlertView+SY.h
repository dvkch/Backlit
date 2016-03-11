//
//  DLAVAlertView+SY.h
//  SaneScanner
//
//  Created by Stan Chevallier on 06/03/2016.
//  Copyright © 2016 Syan. All rights reserved.
//

#import <DLAVAlertView.h>

@interface DLAVAlertView (SY)

@property (nonatomic, assign) BOOL buttonsEnabled;

- (void)setText:(NSString *)text forButtonAtIndex:(NSUInteger)index;

- (NSUInteger)lastOtherButtonIndex;
- (UISlider *)addSliderWithMin:(float)min
                           max:(float)max
                       current:(float)current
                   updateBlock:(void(^)(DLAVAlertView *alertView, float value))block;

- (UIImageView *)addImageViewForImage:(UIImage *)image;

@end
