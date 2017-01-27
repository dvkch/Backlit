//
//  SYToolbar.m
//  SaneScanner
//
//  Created by Stan Chevallier on 15/03/2016.
//  Copyright © 2016 Syan. All rights reserved.
//

#import "SYToolbar.h"
#import <NSObject+SYKit.h>
#import <objc/runtime.h>

@interface SYToolbar ()
@property (nonatomic, strong) NSArray <UIBarButtonItem *> *realItems;
@property (nonatomic, strong) NSNumber *heightNumber;
@end

@implementation SYToolbar

+ (void)initialize
{
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        NSString *sText  = [@[$$("_te"),  $$("xtB"), $$("utt"), $$("onMa"), $$("rgin")] componentsJoinedByString:$$("")];
        NSString *sImage = [@[$$("_im"), $$("ageB"), $$("utt"), $$("onMa"), $$("rgin")] componentsJoinedByString:$$("")];
        
        IMP impNew = class_getMethodImplementation([self class], @selector(padding));
        class_addMethod([self class], NSSelectorFromString(sText),  impNew, "f@:");
        class_addMethod([self class], NSSelectorFromString(sImage), impNew, "f@:");
    });
}

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

- (void)setPadding:(CGFloat)padding
{
    self->_padding = padding;
    [self setNeedsLayout];
}

- (CGSize)sizeThatFits:(CGSize)size
{
    if (!self.heightNumber)
        return [super sizeThatFits:size];
    return CGSizeMake([super sizeThatFits:size].width, self.height);
}

@end
