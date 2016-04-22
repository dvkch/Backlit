//
//  SYToolbar.m
//  SaneScanner
//
//  Created by Stan Chevallier on 15/03/2016.
//  Copyright © 2016 Syan. All rights reserved.
//

#import "SYToolbar.h"

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
    
    return subview.frame.origin.x;
}

@end
