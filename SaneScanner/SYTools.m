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
    BOOL isEmpty = rect.size.width < 0.5 || rect.size.height < 0.5;
    NSLog(@"Empty=%d", isEmpty);
    
    CGRect newRect = rect;
    
    CGRectSide sideX;
    CGRectSide sideY;
    
    switch (corner) {
        case CGRectCornerTopLeft:
            sideX = CGRectSideLeft;
            sideY = CGRectSideTop;
            break;
        case CGRectCornerTopRight:
            sideX = CGRectSideRight;
            sideY = CGRectSideTop;
            break;
        case CGRectCornerBottomLeft:
            sideX = CGRectSideLeft;
            sideY = CGRectSideBottom;
            break;
        case CGRectCornerBottomRight:
            sideX = CGRectSideRight;
            sideY = CGRectSideBottom;
            break;
    }
    
    newRect = CGRectByMovingSideOfCGRectByDeltaWithoutShrinking(sideX, newRect, delta.width, preventWidthShrinking);
    newRect = CGRectByMovingSideOfCGRectByDeltaWithoutShrinking(sideY, newRect, delta.height, preventHeightShrinking);
    
    return newRect;
}

CGRect CGRectByMovingSideOfCGRectByDeltaWithoutShrinking(CGRectSide side,
                                                         CGRect rect,
                                                         CGFloat delta,
                                                         BOOL preventShrinking)
{
    CGRect newRect = rect;
    switch (side) {
        case CGRectSideTop:
            newRect.origin.y    += delta;
            newRect.size.height -= delta;
            break;
        case CGRectSideLeft:
            newRect.origin.x    += delta;
            newRect.size.width  -= delta;
            break;
        case CGRectSideRight:
            newRect.size.width  += delta;
            break;
        case CGRectSideBottom:
            newRect.size.height += delta;
            break;
    }
    
    if (preventShrinking && newRect.size.width < rect.size.width)
    {
        newRect.origin.x = rect.origin.x;
        newRect.size.width = rect.size.width;
    }
    
    if (preventShrinking && newRect.size.height < rect.size.height)
    {
        newRect.origin.y = rect.origin.y;
        newRect.size.height = rect.size.height;
    }
    
    return newRect;
}

BOOL CGRectSideIsVertical(CGRectSide side)
{
    return (side == CGRectSideLeft) || (side == CGRectSideRight);
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
