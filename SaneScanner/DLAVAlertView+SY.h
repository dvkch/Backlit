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

- (void)setText:(NSString * _Nullable)text forButtonAtIndex:(NSUInteger)index;

- (NSUInteger)lastOtherButtonIndex;
- (UISlider * _Nonnull )addSliderWithMin:(float)min
                                     max:(float)max
                                 current:(float)current
                             updateBlock:(void(^_Nullable)(DLAVAlertView * _Nonnull alertView, float value))block;

- (UIImageView * _Nonnull )addImageViewForImage:(UIImage * _Nullable)image;

- (nonnull instancetype)initWithTitle:(NSString * _Nullable)title
                              message:(NSString * _Nullable)message
                             delegate:(id _Nullable)delegate
                               cancel:(NSString * _Nullable)cancel
                               others:(NSArray <NSString *> * _Nonnull)others;


@end

