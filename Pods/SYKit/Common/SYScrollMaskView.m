//
//  SYScrollMaskView.m
//  SYKit
//
//  Created by Stanislas Chevallier on 22/05/2017.
//  Copyright © 2017 Syan. All rights reserved.
//

#import "SYScrollMaskView.h"

// TODO: put in SYKit

@interface SYScrollMaskView ()
@property (nonatomic, strong) UIImageView *imageView;
@end

@implementation SYScrollMaskView

- (instancetype)initWithFrame:(CGRect)frame
{
    self = [super initWithFrame:frame];
    if (self) {
        _scrollMask = SYScrollMaskVertical;
        _scrollMaskSize = 16;
        [self updateMask];
        
        self.imageView = [[UIImageView alloc] init];
        self.maskView = self.imageView;
    }
    return self;
}

- (void)setScrollMask:(SYScrollMask)scrollMask
{
    _scrollMask = scrollMask;
    [self updateMask];
}

- (void)setScrollMaskSize:(CGFloat)scrollMaskSize
{
    _scrollMaskSize = scrollMaskSize;
    [self updateMask];
}

- (void)setScrollView:(UIScrollView *)scrollView
{
    [_scrollView removeFromSuperview];
    _scrollView = scrollView;
    [self addSubview:scrollView];
    [self setNeedsLayout];
}

- (void)layoutSubviews
{
    self.scrollView.frame = self.bounds;
    self.imageView.frame = self.bounds;
}

- (void)updateMask
{
    if (self.scrollMask == SYScrollMaskNone)
    {
        self.imageView.image = nil;
        self.backgroundColor = UIColor.blackColor;
        return;
    }
    
    self.imageView.image = [self.class verticalGradientResizableImage:(self.scrollMask == SYScrollMaskVertical) size:self.scrollMaskSize];
    self.backgroundColor = nil;
}

+ (UIImage *)verticalGradientResizableImage:(BOOL)vertical size:(CGFloat)size
{
    // helper values to have less conditions on 'vertical' later
    CGFloat vSize0 = vertical ? size : 0;
    CGFloat hSize0 = vertical ? 0    : size;
    
    // gradient layer
    CAGradientLayer *gradient = [CAGradientLayer layer];
    gradient.locations = @[@0, @1];
    gradient.colors = @[(id)UIColor.clearColor.CGColor, (id)UIColor.blackColor.CGColor];
    gradient.frame = CGRectMake(0, 0, vertical ? 1 : size, vertical ? size : 1);

    UIGraphicsBeginImageContextWithOptions(CGSizeMake(1 + 2 * hSize0, 1 + 2 * vSize0), NO, 0.);
    {
        // top/left gradient, from clear to black
        gradient.startPoint = CGPointZero;
        gradient.endPoint = CGPointMake(vertical ? 0 : 1, vertical ? 1 : 0);
        [gradient renderInContext:UIGraphicsGetCurrentContext()];
        
        // middle pixel, black
        [[UIColor blackColor] setFill];
        [[UIBezierPath bezierPathWithRect:CGRectMake(hSize0, vSize0, 1, 1)] fill];
        
        // translate the context, required because -[CALayer renderInContext:] doesn't
        // take frame into account
        CGContextTranslateCTM(UIGraphicsGetCurrentContext(),
                              vertical ? 0        : size + 1,
                              vertical ? size + 1 : 0       );
        
        // bottom/right gradient, from black to clear
        gradient.startPoint = gradient.endPoint;
        gradient.endPoint = CGPointZero;
        [gradient renderInContext:UIGraphicsGetCurrentContext()];
    }
    UIImage *image = UIGraphicsGetImageFromCurrentImageContext();
    UIGraphicsEndImageContext();
    
    // return a resizable image
    return [image resizableImageWithCapInsets:UIEdgeInsetsMake(vSize0, hSize0, vSize0, hSize0)];
}

@end
