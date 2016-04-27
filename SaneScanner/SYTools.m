//
//  SYTools.m
//  SaneScanner
//
//  Created by rominet on 06/05/15.
//  Copyright (c) 2015 Syan. All rights reserved.
//

#import "SYTools.h"
#import <UIImage+SYKit.h>

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

CGRect CGRectByMovingCornerOfCGRectByDelta(CGRectCorner corner, CGRect rect, CGSize delta, CGRect maxRect)
{
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
    
    newRect = CGRectByMovingSideOfCGRectByDelta(sideX, newRect, delta.width,  CGRectNull);
    newRect = CGRectByMovingSideOfCGRectByDelta(sideY, newRect, delta.height, CGRectNull);
    
    if (!CGRectIsNull(maxRect))
        newRect = CGRectIntersection(newRect, maxRect);
    
    return newRect;
}


CGRect CGRectByMovingSideOfCGRectByDelta(CGRectSide side, CGRect rect, CGFloat delta, CGRect maxRect)
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
    
    if (!CGRectIsNull(maxRect))
        newRect = CGRectIntersection(newRect, maxRect);
    
    return newRect;
}

BOOL CGRectSideIsVertical(CGRectSide side)
{
    return (side == CGRectSideLeft) || (side == CGRectSideRight);
}

@implementation SYTools

+ (NSString *)documentsPath
{
    static dispatch_once_t onceToken;
    static NSString *path = nil;
    dispatch_once(&onceToken, ^{
        NSArray *paths = NSSearchPathForDirectoriesInDomains(NSDocumentDirectory, NSUserDomainMask, YES);
        path = paths.firstObject;
    });
    return path;
}

+ (NSString *)appSupportPath:(BOOL)create
{
    NSArray *paths = NSSearchPathForDirectoriesInDomains(NSApplicationSupportDirectory, NSUserDomainMask, YES);
    NSString *path = [paths.firstObject stringByAppendingPathComponent:@"SaneScanner"];

    if (create && ![[NSFileManager defaultManager] fileExistsAtPath:path isDirectory:NULL])
    {
        [[NSFileManager defaultManager] createDirectoryAtPath:path
                                  withIntermediateDirectories:YES
                                                   attributes:nil
                                                        error:NULL];
    }
    
    return path;
}

+ (NSString *)pathForFile:(NSString *)filename
{
    return [[self documentsPath] stringByAppendingPathComponent:filename];
}

+ (void)createTestImages:(NSUInteger)count
{
    for (NSUInteger i = 0; i < count; ++i)
    {
        // https://gist.github.com/kylefox/1689973
        CGFloat hue = ( arc4random() % 256 / 256.0 );  //  0.0 to 1.0
        CGFloat saturation = ( arc4random() % 128 / 256.0 ) + 0.5;  //  0.5 to 1.0, away from white
        CGFloat brightness = ( arc4random() % 128 / 256.0 ) + 0.5;  //  0.5 to 1.0, away from black
        UIColor *color = [UIColor colorWithHue:hue saturation:saturation brightness:brightness alpha:1];
        
        UIImage *image = [UIImage sy_imageWithColor:color size:CGSizeMake(10, 10) cornerRadius:0];
        NSString *path = [SYTools pathForFile:[NSString stringWithFormat:@"testimage-%@.png", [[NSUUID UUID] UUIDString]]];
        [UIImagePNGRepresentation(image) writeToFile:path atomically:YES];
    }
}

@end
