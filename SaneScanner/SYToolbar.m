//
//  SYToolbar.m
//  SaneScanner
//
//  Created by Stan Chevallier on 15/03/2016.
//  Copyright © 2016 Syan. All rights reserved.
//

#import "SYToolbar.h"


@interface UIToolbar (Private)

@property (setter=_setHidesShadow:, nonatomic) BOOL _hidesShadow;
@property (getter=_isLocked, setter=_setLocked:, nonatomic) BOOL _locked;
@property (setter=_setWantsLetterpressContent:, nonatomic) BOOL _wantsLetterpressContent;
@property (nonatomic, readonly) int barPosition;
@property (nonatomic) BOOL centerTextButtons;
@property (readonly, copy) NSString *debugDescription;
@property (readonly, copy) NSString *description;
@property (readonly) unsigned int hash;
@property (getter=isMinibar, nonatomic, readonly) BOOL minibar;
@property (readonly) Class superclass;

+ (Class)defaultButtonClass;
+ (id)defaultButtonFont;
+ (float)defaultHeight;
+ (float)defaultHeightForBarSize:(int)arg1;
+ (float)defaultSelectionModeHeight;
+ (Class)defaultTextButtonClass;

- (id)_adaptiveBackdrop;
- (float)_autolayoutSpacingAtEdge:(int)arg1 inContainer:(id)arg2;
- (float)_autolayoutSpacingAtEdge:(int)arg1 nextToNeighbor:(id)arg2;
- (id)_backdropViewLayerGroupName;
- (id)_backgroundView;
- (int)_barPosition;
- (void)_buttonBarFinishedAnimating;
- (id)_buttonName:(id)arg1 withType:(int)arg2;
- (void)_cleanupAdaptiveBackdrop;
- (BOOL)_contentHuggingDefault_isUsuallyFixedHeight;
- (id)_currentCustomBackground;
- (id)_currentCustomBackgroundRespectOversize_legacy:(BOOL*)arg1;
- (void)_customViewChangedForButtonItem:(id)arg1;
- (void)_didMoveFromWindow:(id)arg1 toWindow:(id)arg2;
- (float)_edgeMarginForBorderedItem:(BOOL)arg1 isText:(BOOL)arg2;
- (id)_effectiveBarTintColor;
- (void)_effectiveBarTintColorDidChangeWithPreviousColor:(id)arg1;
- (CGRect)_frameOfBarButtonItem:(id)arg1;
- (void)_frameOrBoundsChangedWithVisibleSizeChange:(BOOL)arg1 wasMinibar:(BOOL)arg2;
- (void)_frameOrCenterChanged;
- (BOOL)_hasCustomAutolayoutNeighborSpacing;
- (BOOL)_hidesShadow;
- (BOOL)_isAdaptiveToolbarDisabled;
- (BOOL)_isInNavigationBar;
- (BOOL)_isLocked;
- (BOOL)_isTopBar_legacy;
- (void)_layoutBackgroundViewConsideringStatusBar;
- (void)_populateArchivedSubviews:(id)arg1;
- (void)_positionToolbarButtons:(id)arg1 ignoringItem:(id)arg2 resetFontScaleAdjustment:(BOOL)arg3;
- (id)_positionToolbarButtons:(id)arg1 ignoringItem:(id)arg2 resetFontScaleAdjustment:(BOOL)arg3 actuallyRepositionButtons:(BOOL)arg4;
- (id)_repositionedItemsFromItems:(id)arg1 withBarButtonFrames:(id*)arg2 withHitRects:(id*)arg3 buttonIndexes:(id*)arg4 textButtonIndexes:(id*)arg5;
- (void)_sendAction:(id)arg1 withEvent:(id)arg2;
- (void)_setAdaptiveToolbarDisabled:(BOOL)arg1;
- (void)_setBackdropViewLayerGroupName:(id)arg1;
- (void)_setBackgroundImage:(id)arg1 mini:(id)arg2;
- (void)_setBackgroundView:(id)arg1;
- (void)_setBarPosition:(int)arg1;
- (void)_setButtonBackgroundImage:(id)arg1 mini:(id)arg2 forStates:(unsigned int)arg3;
- (void)_setForceTopBarAppearance:(BOOL)arg1;
- (void)_setHidesShadow:(BOOL)arg1;
- (void)_setLocked:(BOOL)arg1;
- (void)_setShadowView:(id)arg1;
- (void)_setVisualAltitude:(float)arg1;
- (void)_setVisualAltitudeBias:(CGSize)arg1;
- (void)_setWantsLetterpressContent:(BOOL)arg1;
- (id)_shadowView;
- (BOOL)_subclassImplementsDrawRect;
- (BOOL)_supportsAdaptiveBackground;
- (void)_updateBackgroundColor;
- (void)_updateBackgroundImage;
- (void)_updateBarForStyle;
- (void)_updateItemsForNewFrame:(id)arg1;
- (void)_updateOpacity;
- (void)_updateScriptingInfo:(id)arg1 view:(id)arg2;
- (BOOL)_wantsLetterpressContent;
- (void)addConstraint:(id)arg1;
- (void)animateToolbarItemIndex:(unsigned int)arg1 duration:(double)arg2 target:(id)arg3 didFinishSelector:(SEL)arg4;
- (void)animateWithDuration:(float)arg1 forButton:(int)arg2;
- (void)backdropView:(id)arg1 didChangeToGraphicsQuality:(int)arg2;
- (id)backdropView:(id)arg1 willChangeToGraphicsQuality:(int)arg2;
- (int)barPosition;
- (id)buttonItems;
- (BOOL)centerTextButtons;
- (id)createButtonWithDescription:(id)arg1;
- (int)currentButtonGroup;
- (void)dealloc;
- (CGSize)defaultSizeForOrientation:(int)arg1;
- (float)extraEdgeInsets;
- (void)getVisibleButtonTags:(int*)arg1 count:(unsigned int*)arg2 maxItems:(unsigned int)arg3;
- (id)initInView:(id)arg1 withFrame:(CGRect)arg2 withItemList:(id)arg3;
- (id)initInView:(id)arg1 withItemList:(id)arg2;
- (CGSize)intrinsicContentSize;
- (void)invalidateIntrinsicContentSize;
- (BOOL)isElementAccessibilityExposedToInterfaceBuilder;
- (BOOL)isMinibar;
- (int)mode;
- (BOOL)onStateForButton:(int)arg1;
- (void)positionButtons:(id)arg1 tags:(int*)arg2 count:(int)arg3 group:(int)arg4;
- (void)registerButtonGroup:(int)arg1 withButtons:(int*)arg2 withCount:(int)arg3;
- (void)removeConstraint:(id)arg1;
- (id)scriptingInfoWithChildren;
- (void)setAutoresizingMask:(unsigned int)arg1;
- (void)setBadgeAnimated:(BOOL)arg1 forButton:(int)arg2;
- (void)setBadgeGlyph:(id)arg1 forButton:(int)arg2;
- (void)setBadgeValue:(id)arg1 forButton:(int)arg2;
- (void)setButtonBarTrackingMode:(int)arg1;
- (void)setButtonItems:(id)arg1;
- (void)setCenterTextButtons:(BOOL)arg1;
- (void)setExtraEdgeInsets:(float)arg1;
- (void)setMode:(int)arg1;
- (void)setOnStateForButton:(BOOL)arg1 forButton:(int)arg2;
- (void)setTranslatesAutoresizingMaskIntoConstraints:(BOOL)arg1;
- (void)showButtonGroup:(int)arg1 withDuration:(double)arg2;
- (void)showButtons:(int*)arg1 withCount:(int)arg2 withDuration:(double)arg3;
- (CGSize)sizeThatFits:(CGSize)arg1;

@end






@interface SYToolbar ()
@property (nonatomic, strong) NSArray <UIBarButtonItem *> *realItems;
@property (nonatomic, strong) NSNumber *paddingNumber;
@property (nonatomic, strong) NSNumber *heightNumber;
@end

@implementation SYToolbar

- (CGFloat)height
{
    return self.heightNumber ? self.heightNumber.doubleValue : self.bounds.size.height;
}

- (void)setHeight:(CGFloat)height
{
    if (self.height == height)
        return;
    
    self.heightNumber = height < 0.5 ? nil : @(height);
    [self.superview setNeedsLayout];
}

- (CGFloat)padding
{
    return self.paddingNumber ? self.paddingNumber.doubleValue : [self.class systemPadding];
}

- (void)setPadding:(CGFloat)padding
{
    self.paddingNumber = @(padding);
    [self setItems:self.realItems animated:NO];
}

- (CGSize)sizeThatFits:(CGSize)size
{
    if (!self.heightNumber)
        return [super sizeThatFits:size];
    return CGSizeMake([super sizeThatFits:size].width, self.height);
}

- (void)setItems:(NSArray<UIBarButtonItem *> *)items animated:(BOOL)animated
{
    self.realItems = items;
    [super setItems:items animated:animated];
    return;
    
    self.realItems = items;

    if (!items.count)
    {
        [super setItems:items animated:animated];
        return;
    }

    NSMutableArray *mutableItems = [items mutableCopy];

    CGFloat defaultPadding = [self.class systemPadding];
    if (self.paddingNumber)
    {
        UIBarButtonItem *spacerLeft  = [[UIBarButtonItem alloc] initWithBarButtonSystemItem:UIBarButtonSystemItemFixedSpace
                                                                                     target:nil action:nil];
        UIBarButtonItem *spacerRight = [[UIBarButtonItem alloc] initWithBarButtonSystemItem:UIBarButtonSystemItemFixedSpace
                                                                                     target:nil action:nil];
        spacerLeft.width  = -defaultPadding + self.padding;
        spacerRight.width = -defaultPadding + self.padding;
        
        [mutableItems insertObject:spacerLeft atIndex:0];
        [mutableItems addObject:spacerRight];
    }
    
    [super setItems:mutableItems animated:animated];
}

+ (CGFloat)systemPadding
{
    static CGFloat cachedValues[2] = {-1, -1};
    int index = 0;
    
    //if (cachedValues[index] < 0)
    {
        UIWindow *mainWindow = [[[UIApplication sharedApplication] delegate] window];
        
        UIViewController *viewController = [[UIViewController alloc] init];
        [viewController view];
        
        UINavigationController *navController = [[UINavigationController alloc] initWithRootViewController:viewController];
        [navController view];
        [navController setToolbarHidden:NO animated:NO];
        
        UIWindow *window = [[UIWindow alloc] init];
        [window setRootViewController:navController];
        [window setBackgroundColor:[UIColor clearColor]];
        [window setOpaque:NO];
        [window setHidden:NO];
        [mainWindow makeKeyAndVisible];
        
        UIView *subview = [[UIView alloc] init];
        UIBarButtonItem *barButton = [[UIBarButtonItem alloc] initWithCustomView:subview];
        [viewController setToolbarItems:@[barButton]];
        
        cachedValues[index] = subview.frame.origin.x;
    }
    
    return cachedValues[index];
}

@end
