//
//  SYTools.h
//  SaneScanner
//
//  Created by rominet on 06/05/15.
//  Copyright (c) 2015 Syan. All rights reserved.
//

#import <UIKit/UIKit.h>

BOOL NSObjectIsBlock(id object);

CGRect CGRectAsPercentsInCGRect(CGRect rect, CGRect containingRect);
CGRect CGRectFromPercentsInCGRect(CGRect percents, CGRect containingRect);

typedef enum : NSUInteger {
    CGRectCornerTopLeft,
    CGRectCornerTopRight,
    CGRectCornerBottomLeft,
    CGRectCornerBottomRight
} CGRectCorner;

CGPoint CGPointForCornerOfCGRect(CGRectCorner corner, CGRect rect);
CGRect CGRectByMovingCornerOfCGRectByDelta(CGRectCorner corner, CGRect rect, CGSize delta);
CGRect CGRectByMovingCornerOfCGRectByDeltaWithoutShrinking(CGRectCorner corner,
                                                           CGRect rect,
                                                           CGSize delta,
                                                           BOOL preventWidthShrinking,
                                                           BOOL preventHeightShrinking);

@interface SYTools : NSObject

+ (NSString *)documentsPath;
+ (NSString *)pathForFile:(NSString *)filename;
+ (NSString *)hostsFile;

@end
