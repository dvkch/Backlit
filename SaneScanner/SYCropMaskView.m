//
//  SYCropMaskView.m
//  SaneScanner
//
//  Created by Stan Chevallier on 08/03/2016.
//  Copyright © 2016 Syan. All rights reserved.
//

#import "SYCropMaskView.h"
#import <SYShapeView.h>
#import "SYTools.h"

static CGFloat const kCornerViewSize = 20.;

@interface SYCropMaskView () <UIGestureRecognizerDelegate>
@property (nonatomic, strong) UIView *maskBorderView;
@property (nonatomic, strong) SYShapeView *maskView;
@property (nonatomic, strong) NSMutableDictionary <NSNumber *, UIPanGestureRecognizer *> *panGestures;
@property (nonatomic, strong) NSMutableDictionary <NSNumber *, UIView *> *cornerViews;
@property (nonatomic, assign) CGPoint panOriginPoint;
@property (nonatomic, assign) CGPoint panPreviousPoint;
@end

@implementation SYCropMaskView

- (instancetype)init
{
    self = [super init];
    if (self) [self customInit];
    return self;
}

- (void)customInit
{
    if (self.cornerViews)
        return;
    
    [self setBackgroundColor:[UIColor clearColor]];
    
    self.maskBorderView = [[SYShapeView alloc] initWithFrame:self.bounds];
    [self.maskBorderView setAutoresizingMask:(UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight)];
    [self.maskBorderView.layer setBorderColor:[UIColor colorWithRed:0. green:0.48 blue:1. alpha:1.].CGColor];
    [self.maskBorderView.layer setBorderWidth:2.];
    [self addSubview:self.maskBorderView];
    
    self.maskView = [[SYShapeView alloc] initWithFrame:self.bounds];
    [self.maskView setAutoresizingMask:(UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight)];
    [self.maskView.layer setFillColor:[UIColor colorWithWhite:0. alpha:0.4].CGColor];
    [self.maskView.layer setFillRule:kCAFillRuleEvenOdd];
    [self addSubview:self.maskView];
    
    self.cornerViews = [NSMutableDictionary dictionary];
    self.cornerViews[@(CGRectCornerTopLeft)]     = [[UIView alloc] init];
    self.cornerViews[@(CGRectCornerTopRight)]    = [[UIView alloc] init];
    self.cornerViews[@(CGRectCornerBottomLeft)]  = [[UIView alloc] init];
    self.cornerViews[@(CGRectCornerBottomRight)] = [[UIView alloc] init];
    
    self.panGestures = [NSMutableDictionary dictionary];
    for (NSNumber *cornerN in self.cornerViews.allKeys)
    {
        UIView *cornerView = self.cornerViews[cornerN];
        [cornerView setBackgroundColor:[UIColor colorWithRed:0. green:0.48 blue:1. alpha:1.]];
        [cornerView.layer setBorderColor:[UIColor whiteColor].CGColor];
        [cornerView.layer setBorderWidth:2.];
        [cornerView.layer setCornerRadius:kCornerViewSize/2.];
        [cornerView.layer setShadowColor:[UIColor blackColor].CGColor];
        [cornerView.layer setShadowOffset:CGSizeZero];
        [cornerView.layer setShadowOpacity:0.3];
        [cornerView.layer setShadowRadius:3.];
        [self addSubview:cornerView];
        
        UIPanGestureRecognizer *panGesture = [[UIPanGestureRecognizer alloc] init];
        [panGesture addTarget:self action:@selector(panGestureTouch:)];
        [panGesture setDelegate:self];
        [cornerView addGestureRecognizer:panGesture];
        
        [self.panGestures setObject:panGesture forKey:cornerN];
    }
}

#pragma mark - Data

- (CGRect)cropAreaInViewBounds
{
    CGRect percents = CGRectAsPercentsInCGRect(self.cropArea, self.maxCropArea);
    return CGRectFromPercentsInCGRect(percents, self.bounds);
}

- (void)setCropAreaInViewBounds:(CGRect)cropArea notifyDelegate:(BOOL)notifyDelegate
{
    CGRect percents = CGRectAsPercentsInCGRect(cropArea, self.bounds);
    [self setCropArea:CGRectFromPercentsInCGRect(percents, self.maxCropArea) andMaxCropArea:self.maxCropArea];
    
    if (notifyDelegate && self.cropAreaDidChangeBlock)
        self.cropAreaDidChangeBlock(self.cropArea);
}

- (void)setCropArea:(CGRect)cropArea andMaxCropArea:(CGRect)maxCropArea
{
    self->_maxCropArea = maxCropArea;
    self->_cropArea = CGRectIntersection(cropArea, self.maxCropArea);
    [self setNeedsLayout];
}

#pragma mark - Layout

- (void)layoutSubviews
{
    [super layoutSubviews];
    
    if (!CGRectEqualToRect(self.cropArea, CGRectZero) && !CGRectEqualToRect(self.maxCropArea, CGRectZero))
    {
        [self.maskView setHidden:NO];
        [self.maskBorderView setHidden:NO];
        
        for (NSNumber *cornerN in self.cornerViews.allKeys)
        {
            UIView *cornerView = self.cornerViews[cornerN];
            [cornerView setFrame:CGRectMake(0, 0, kCornerViewSize, kCornerViewSize)];
            [cornerView setCenter:CGPointForCornerOfCGRect(cornerN.unsignedIntegerValue, self.cropAreaInViewBounds)];
            [cornerView setHidden:NO];
        }
        
        UIBezierPath *path = [UIBezierPath bezierPathWithRect:self.bounds];
        [path appendPath:[UIBezierPath bezierPathWithRect:self.cropAreaInViewBounds]];
        self.maskView.layer.path = path.CGPath;
        self.maskBorderView.frame = self.cropAreaInViewBounds;
    }
    else
    {
        for (UIView *cornerView in self.cornerViews.allValues)
            [cornerView setHidden:YES];
        
        [self.maskView setHidden:YES];
        [self.maskBorderView setHidden:YES];
    }
}

#pragma mark - Touch

- (BOOL)gestureRecognizer:(UIGestureRecognizer *)gestureRecognizer
shouldRecognizeSimultaneouslyWithGestureRecognizer:(UIGestureRecognizer *)otherGestureRecognizer
{
    BOOL gesture1IsPan = [self.panGestures.allValues containsObject:(id)gestureRecognizer];
    BOOL gesture2IsPan = [self.panGestures.allValues containsObject:(id)otherGestureRecognizer];
    return (gesture1IsPan || gesture2IsPan) ? NO : YES;
}

- (void)panGestureTouch:(UIPanGestureRecognizer *)panGesture
{
    switch (panGesture.state) {
        case UIGestureRecognizerStateBegan:
            self.panOriginPoint = [panGesture locationInView:self];
            self.panPreviousPoint = self.panOriginPoint;
            break;
        case UIGestureRecognizerStateChanged:
        {
            CGPoint point = [panGesture locationInView:self];
            BOOL preventShrinkingX = point.x < 0 || point.x > self.bounds.size.width;
            BOOL preventShrinkingY = point.y < 0 || point.y > self.bounds.size.height;
            
            CGSize delta = CGSizeMake(point.x - self.panPreviousPoint.x,
                                      point.y - self.panPreviousPoint.y);
            
            CGRectCorner corner = [self.panGestures allKeysForObject:panGesture].firstObject.unsignedIntegerValue;
            CGRect newCrop = CGRectByMovingCornerOfCGRectByDeltaWithoutShrinking(corner,
                                                                                 self.cropAreaInViewBounds,
                                                                                 delta,
                                                                                 preventShrinkingX,
                                                                                 preventShrinkingY);
            [self setCropAreaInViewBounds:newCrop notifyDelegate:YES];
            [self setPanPreviousPoint:point];
        }
        default:
            break;
    }
}

- (BOOL)pointInside:(CGPoint)point withEvent:(UIEvent *)event
{
    for (UIView *cornerView in self.cornerViews.allValues)
        if (CGRectContainsPoint(cornerView.frame, point))
            return YES;
    
    return [super pointInside:point withEvent:event];
}

@end
