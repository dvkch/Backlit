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
#import <UIView+SYKit.h>
#import "SaneScanner-Swift.h"

static CGFloat const kCornerViewSize = 20.;
static CGFloat const kBorderWidth    =  2.;

@interface SYCropMaskView () <UIGestureRecognizerDelegate>
@property (nonatomic, strong) SYShapeView *maskView;
@property (nonatomic, strong) NSMutableDictionary <NSNumber *, UIPanGestureRecognizer *> *cornerPanGestures;
@property (nonatomic, strong) NSMutableDictionary <NSNumber *, UIPanGestureRecognizer *> *borderPanGestures;
@property (nonatomic, strong) NSMutableDictionary <NSNumber *, UIView *> *cornerViews;
@property (nonatomic, strong) NSMutableDictionary <NSNumber *, UIView *> *borderViews;
@property (nonatomic, assign) CGPoint panFirstPoint;
@property (nonatomic, assign) CGRect panCropArea;
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
    
    self.panCropArea = CGRectNull;
    [self setBackgroundColor:[UIColor clearColor]];
    
    self.maskView = [[SYShapeView alloc] initWithFrame:self.bounds];
    [self.maskView setAutoresizingMask:(UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight)];
    [self.maskView.layer setFillColor:[UIColor colorWithWhite:0. alpha:0.4].CGColor];
    [self.maskView.layer setFillRule:kCAFillRuleEvenOdd];
    [self addSubview:self.maskView];
    
    self.borderViews = [NSMutableDictionary dictionary];
    self.borderViews[@(CGRectSideTop)]    = [[UIView alloc] init];
    self.borderViews[@(CGRectSideLeft)]   = [[UIView alloc] init];
    self.borderViews[@(CGRectSideRight)]  = [[UIView alloc] init];
    self.borderViews[@(CGRectSideBottom)] = [[UIView alloc] init];
    
    self.borderPanGestures = [NSMutableDictionary dictionary];
    for (NSNumber *borderN in self.borderViews.allKeys)
    {
        UIView *borderView = self.borderViews[borderN];
        [borderView setBackgroundColor:[UIColor vividBlue]];
        [borderView.layer setOpaque:YES];
        [self addSubview:borderView];
        
        if (CGRectSideIsVertical(borderN.unsignedIntegerValue))
            [borderView setSy_tapInsets:UIEdgeInsetsMake(  0, -15,   0, -15)];
        else
            [borderView setSy_tapInsets:UIEdgeInsetsMake(-15,   0, -15,   0)];

        UIPanGestureRecognizer *panGesture = [[UIPanGestureRecognizer alloc] init];
        [panGesture addTarget:self action:@selector(borderPanGestureTouch:)];
        [panGesture setDelegate:self];
        [borderView addGestureRecognizer:panGesture];
        
        [self.borderPanGestures setObject:panGesture forKey:borderN];
    }
    
    self.cornerViews = [NSMutableDictionary dictionary];
    self.cornerViews[@(CGRectCornerTopLeft)]     = [[UIView alloc] init];
    self.cornerViews[@(CGRectCornerTopRight)]    = [[UIView alloc] init];
    self.cornerViews[@(CGRectCornerBottomLeft)]  = [[UIView alloc] init];
    self.cornerViews[@(CGRectCornerBottomRight)] = [[UIView alloc] init];
    
    self.cornerPanGestures = [NSMutableDictionary dictionary];
    for (NSNumber *cornerN in self.cornerViews.allKeys)
    {
        UIView *cornerView = self.cornerViews[cornerN];
        [cornerView setSy_tapInsets:UIEdgeInsetsMake(-5, -5, -5, -5)];
        [cornerView setBackgroundColor:[UIColor vividBlue]];
        [cornerView.layer setBorderColor:[UIColor whiteColor].CGColor];
        [cornerView.layer setBorderWidth:2.];
        [cornerView.layer setCornerRadius:kCornerViewSize/2.];
        [cornerView.layer setShadowColor:[UIColor blackColor].CGColor];
        [cornerView.layer setShadowOffset:CGSizeZero];
        [cornerView.layer setShadowOpacity:0.3];
        [cornerView.layer setShadowRadius:3.];
        [cornerView.layer setRasterizationScale:[[UIScreen mainScreen] scale]];
        [cornerView.layer setShouldRasterize:YES];
        [self addSubview:cornerView];
        
        UIPanGestureRecognizer *panGesture = [[UIPanGestureRecognizer alloc] init];
        [panGesture addTarget:self action:@selector(cornerPanGestureTouch:)];
        [panGesture setDelegate:self];
        [cornerView addGestureRecognizer:panGesture];
        
        [self.cornerPanGestures setObject:panGesture forKey:cornerN];
    }
}

#pragma mark - Data

- (CGRect)maxCropAreaInViewBounds
{
    CGRect percents = CGRectAsPercentsInCGRect(self.maxCropArea, self.maxCropArea);
    return CGRectFromPercentsInCGRect(percents, self.bounds);
}

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

- (void)setPanCropArea:(CGRect)panCropArea
{
    self->_panCropArea = panCropArea;
    [self setNeedsLayout];
}

#pragma mark - Layout

- (void)layoutSubviews
{
    [super layoutSubviews];
    
    if (!CGRectEqualToRect(self.cropArea, CGRectZero) && !CGRectEqualToRect(self.maxCropArea, CGRectZero))
    {
        CGRect cropRect = CGRectIsNull(self.panCropArea) ? self.cropAreaInViewBounds : self.panCropArea;
        
        [self.maskView setHidden:NO];
        
        for (NSNumber *cornerN in self.cornerViews.allKeys)
        {
            UIView *cornerView = self.cornerViews[cornerN];
            [cornerView setFrame:CGRectMake(0, 0, kCornerViewSize, kCornerViewSize)];
            [cornerView setCenter:CGPointForCornerOfCGRect(cornerN.unsignedIntegerValue, cropRect)];
            [cornerView setHidden:NO];
        }
        
        for (NSNumber *borderN in self.borderViews.allKeys)
        {
            UIView *borderView = self.borderViews[borderN];
            [borderView setHidden:NO];
            
            CGPoint originCentered = CGPointMake(cropRect.origin.x - kBorderWidth/2.,
                                                 cropRect.origin.y - kBorderWidth/2.);
            
            switch ((CGRectSide)borderN.unsignedIntegerValue) {
                case CGRectSideTop:
                    [borderView setFrame:CGRectMake(originCentered.x,
                                                    originCentered.y,
                                                    cropRect.size.width,
                                                    kBorderWidth)];
                    break;
                case CGRectSideBottom:
                    [borderView setFrame:CGRectMake(originCentered.x,
                                                    originCentered.y + cropRect.size.height,
                                                    cropRect.size.width,
                                                    kBorderWidth)];
                    break;
                case CGRectSideLeft:
                    [borderView setFrame:CGRectMake(originCentered.x,
                                                    originCentered.y,
                                                    kBorderWidth,
                                                    cropRect.size.height)];
                    break;
                case CGRectSideRight:
                    [borderView setFrame:CGRectMake(originCentered.x + cropRect.size.width,
                                                    originCentered.y,
                                                    kBorderWidth,
                                                    cropRect.size.height)];
                    break;
            }
        }
        
        UIEdgeInsets insetsHalfBorderWidth = UIEdgeInsetsMake(-kBorderWidth/2.,
                                                              -kBorderWidth/2.,
                                                              -kBorderWidth/2.,
                                                              -kBorderWidth/2.);
        
        UIBezierPath *path = [UIBezierPath bezierPathWithRect:self.bounds];
        [path appendPath:[UIBezierPath bezierPathWithRect:UIEdgeInsetsInsetRect(cropRect, insetsHalfBorderWidth)]];
        self.maskView.layer.path = path.CGPath;
    }
    else
    {
        for (UIView *cornerView in self.cornerViews.allValues)
            [cornerView setHidden:YES];
        
        for (UIView *borderView in self.borderViews.allValues)
            [borderView setHidden:YES];
        
        [self.maskView setHidden:YES];
    }
}

#pragma mark - Touch

- (BOOL)gestureRecognizer:(UIGestureRecognizer *)gestureRecognizer
shouldRecognizeSimultaneouslyWithGestureRecognizer:(UIGestureRecognizer *)otherGestureRecognizer
{
    if ([self.cornerPanGestures.allValues containsObject:(id)gestureRecognizer])
        return NO;
    if ([self.cornerPanGestures.allValues containsObject:(id)otherGestureRecognizer])
        return NO;
    if ([self.borderPanGestures.allValues containsObject:(id)gestureRecognizer])
        return NO;
    if ([self.borderPanGestures.allValues containsObject:(id)otherGestureRecognizer])
        return NO;
    return YES;
}

- (void)cornerPanGestureTouch:(UIPanGestureRecognizer *)panGesture
{
    switch (panGesture.state) {
        case UIGestureRecognizerStateBegan:
            self.panFirstPoint = [panGesture locationInView:self];
            break;
        case UIGestureRecognizerStateChanged:
        {
            CGPoint point = [panGesture locationInView:self];
            
            CGSize delta = CGSizeMake(point.x - self.panFirstPoint.x,
                                      point.y - self.panFirstPoint.y);
            
            CGRectCorner corner = [self.cornerPanGestures allKeysForObject:panGesture].firstObject.unsignedIntegerValue;
            CGRect newCrop = CGRectByMovingCornerOfCGRectByDelta(corner, self.cropAreaInViewBounds, delta, self.maxCropAreaInViewBounds);
            self.panCropArea = newCrop;
            break;
        }
        case UIGestureRecognizerStateEnded:
            [self setCropAreaInViewBounds:self.panCropArea notifyDelegate:YES];
            self.panCropArea = CGRectNull;
            break;
        default:
            break;
    }
}

- (void)borderPanGestureTouch:(UIPanGestureRecognizer *)panGesture
{
    switch (panGesture.state) {
        case UIGestureRecognizerStateBegan:
            self.panFirstPoint = [panGesture locationInView:self];
            break;
        case UIGestureRecognizerStateChanged:
        {
            CGPoint point = [panGesture locationInView:self];
            CGRectSide side = [self.borderPanGestures allKeysForObject:panGesture].firstObject.unsignedIntegerValue;
            
            CGFloat delta = (CGRectSideIsVertical(side)
                             ? (point.x - self.panFirstPoint.x)
                             : (point.y - self.panFirstPoint.y));
            
            CGRect newCrop = CGRectByMovingSideOfCGRectByDelta(side, self.cropAreaInViewBounds, delta, self.maxCropAreaInViewBounds);
            self.panCropArea = newCrop;
            break;
        }
        case UIGestureRecognizerStateEnded:
            [self setCropAreaInViewBounds:self.panCropArea notifyDelegate:YES];
            self.panCropArea = CGRectNull;
            break;
        default:
            break;
    }
}

- (BOOL)pointInside:(CGPoint)point withEvent:(UIEvent *)event
{
    UIEdgeInsets insets = UIEdgeInsetsMake(-kCornerViewSize, -kCornerViewSize,
                                           -kCornerViewSize, -kCornerViewSize);
    
    if (CGRectContainsPoint(UIEdgeInsetsInsetRect(self.bounds, insets), point))
        return YES;
    
    return [super pointInside:point withEvent:event];
}

@end
