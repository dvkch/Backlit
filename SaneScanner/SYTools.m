//
//  SYTools.m
//  SaneScanner
//
//  Created by rominet on 06/05/15.
//  Copyright (c) 2015 Syan. All rights reserved.
//

#import "SYTools.h"

// https://gist.github.com/steipete/6ee378bd7d87f276f6e0
BOOL NSObjectIsBlock(id object)
{
    static Class blockClass;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        blockClass = [^{} class];
        while ([blockClass superclass] != NSObject.class) {
            blockClass = [blockClass superclass];
        }
    });
    
    return [object isKindOfClass:blockClass];
}

CGRect CGRectAsPercentsInCGRect(CGRect rect, CGRect containingRect)
{
    CGFloat spanX = CGRectGetMaxX(containingRect);
    CGFloat spanY = CGRectGetMaxY(containingRect);
    
    return CGRectMake(rect.origin.x / spanX,
                      rect.origin.y / spanY,
                      rect.size.width  / spanX,
                      rect.size.height / spanY);
}

CGRect CGRectFromPercentsInCGRect(CGRect percents, CGRect containingRect)
{
    CGFloat spanX = CGRectGetMaxX(containingRect);
    CGFloat spanY = CGRectGetMaxY(containingRect);
    
    return CGRectMake(percents.origin.x * spanX,
                      percents.origin.y * spanY,
                      percents.size.width  * spanX,
                      percents.size.height * spanY);
}

CGPoint CGPointForCornerOfCGRect(CGRectCorner corner, CGRect rect)
{
    switch (corner) {
        case CGRectCornerTopLeft:     return CGPointMake(CGRectGetMinX(rect), CGRectGetMinY(rect));
        case CGRectCornerTopRight:    return CGPointMake(CGRectGetMaxX(rect), CGRectGetMinY(rect));
        case CGRectCornerBottomLeft:  return CGPointMake(CGRectGetMinX(rect), CGRectGetMaxY(rect));
        case CGRectCornerBottomRight: return CGPointMake(CGRectGetMaxX(rect), CGRectGetMaxY(rect));
    }
    return CGPointZero;
}

CGRect CGRectByMovingCornerOfCGRectByDelta(CGRectCorner corner, CGRect rect, CGSize delta)
{
    return CGRectByMovingCornerOfCGRectByDeltaWithoutShrinking(corner, rect, delta, NO, NO);
}

CGRect CGRectByMovingCornerOfCGRectByDeltaWithoutShrinking(CGRectCorner corner,
                                                           CGRect rect,
                                                           CGSize delta,
                                                           BOOL preventWidthShrinking,
                                                           BOOL preventHeightShrinking)
{
    CGRect newRect = rect;
    switch (corner) {
        case CGRectCornerTopLeft:
            newRect.origin.x += delta.width;
            newRect.origin.y += delta.height;
            newRect.size.width  -= delta.width;
            newRect.size.height -= delta.height;
            break;
        case CGRectCornerTopRight:
            newRect.origin.y += delta.height;
            newRect.size.width  += delta.width;
            newRect.size.height -= delta.height;
            break;
        case CGRectCornerBottomLeft:
            newRect.origin.x += delta.width;
            newRect.size.width  -= delta.width;
            newRect.size.height += delta.height;
            break;
        case CGRectCornerBottomRight:
            newRect.size.width  += delta.width;
            newRect.size.height += delta.height;
            break;
    }
    
    if (preventWidthShrinking && newRect.size.width < rect.size.width)
    {
        newRect.origin.x = rect.origin.x;
        newRect.size.width = rect.size.width;
    }
    
    if (preventHeightShrinking && newRect.size.height < rect.size.height)
    {
        newRect.origin.y = rect.origin.y;
        newRect.size.height = rect.size.height;
    }
    
    return newRect;
}

@implementation SYTools

+ (NSString *)documentsPath
{
    NSArray *paths = NSSearchPathForDirectoriesInDomains(NSDocumentDirectory, NSUserDomainMask, YES);
    return ([paths count] ? paths[0] : nil);
}

+ (NSString *)pathForFile:(NSString *)filename
{
    return [[self documentsPath] stringByAppendingPathComponent:filename];
}

+ (NSString *)hostsFile
{
    return [self pathForFile:@"hosts.cfg"];
}

@end
