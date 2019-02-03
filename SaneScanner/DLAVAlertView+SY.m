//
//  DLAVAlertView+SY.m
//  SaneScanner
//
//  Created by Stan Chevallier on 06/03/2016.
//  Copyright © 2016 Syan. All rights reserved.
//

#import "DLAVAlertView+SY.h"
#import <objc/runtime.h>

@interface DLAVAlertView (SY_SuperPrivate)
@property (nonatomic, readonly, strong) NSArray <UIButton *> *buttons;
@end

@implementation DLAVAlertView (SY)

- (void)setText:(NSString *)text forButtonAtIndex:(NSUInteger)index
{
    if (index >= self.buttons.count)
        return;
    
    [self.buttons[index] setTitle:text forState:UIControlStateNormal];
}

- (NSUInteger)lastOtherButtonIndex
{
    // Consider 2 other buttons.
    //
    // With cancel button :
    //  - cancel  : 0
    //  - button1 : 1
    //  - button2 : 2
    // => nbOfButtons - 1
    // => 3 - 1
    // => 2 -> OK
    //
    // Without cancel button :
    //  - button1 : 0
    //  - button2 : 1
    // => nbOfButtons - 1
    // => 2 - 1
    // => 1 -> OK
    
    return self.numberOfButtons - 1;
}

- (void)setSliderUpdateBlock:(void(^)(DLAVAlertView *alertView, float value))block {
    objc_setAssociatedObject(self, @selector(sliderUpdateBlock), block, OBJC_ASSOCIATION_COPY_NONATOMIC);
}

- (void(^)(DLAVAlertView *alertView, float value))sliderUpdateBlock {
    return objc_getAssociatedObject(self, @selector(sliderUpdateBlock));
}

- (BOOL)buttonsEnabled
{
    if (self.buttons.count)
        return self.buttons[0].enabled;
    return YES;
}

- (void)setButtonsEnabled:(BOOL)buttonsEnabled
{
    for (UIButton *button in self.buttons)
        [button setEnabled:buttonsEnabled];
}

- (UISlider *)addSliderWithMin:(float)min
                           max:(float)max
                       current:(float)current
                   updateBlock:(void(^)(DLAVAlertView *alertView, float value))block;
{
    [self setSliderUpdateBlock:block];
    [self setMinContentWidth:280];
    
    UISlider *slider = [[UISlider alloc] initWithFrame:CGRectMake(10, 0, 260, 50)];
    [slider setAutoresizingMask:(UIViewAutoresizingFlexibleWidth |
                                 UIViewAutoresizingFlexibleTopMargin |
                                 UIViewAutoresizingFlexibleBottomMargin)];
    [slider setMinimumValue:min];
    [slider setMaximumValue:max];
    [slider setValue:current];
    [slider addTarget:self action:@selector(sliderUpdatedValue:) forControlEvents:UIControlEventValueChanged];
    [self setContentView:slider];
    
    if (block)
        block(self, slider.value);
    
    return slider;
}

- (void)sliderUpdatedValue:(UISlider *)slider
{
    void(^block)(DLAVAlertView *alertView, float value) = [self sliderUpdateBlock];
    if (block)
        block(self, slider.value);
}

- (UIImageView *)addImageViewForImage:(UIImage *)image
{
    UIImageView *imageView = [[UIImageView alloc] initWithFrame:CGRectMake(10, 10, 280, 280)];
    [imageView setContentMode:UIViewContentModeScaleAspectFit];
    [imageView setBackgroundColor:[UIColor clearColor]];
    [imageView setImage:image];
    [self setContentView:imageView];
    [self setMinContentWidth:300];
    return imageView;
}

- (instancetype)initWithTitle:(NSString *)title
                      message:(NSString *)message
                     delegate:(id)delegate
                       cancel:(NSString *)cancel
                       others:(NSArray <NSString *> *)others
{
    DLAVAlertView *av = [self initWithTitle:title message:message delegate:delegate cancelButtonTitle:cancel otherButtonTitles:nil];
    for (NSString *other in others) {
        [av addButtonWithTitle:other];
    }
    return av;
}

@end
