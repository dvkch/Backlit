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
    CGRectSideTop,
    CGRectSideLeft,
    CGRectSideRight,
    CGRectSideBottom,
} CGRectSide;

typedef enum : NSUInteger {
    CGRectCornerTopLeft,
    CGRectCornerTopRight,
    CGRectCornerBottomLeft,
    CGRectCornerBottomRight
} CGRectCorner;

CGPoint CGPointForCornerOfCGRect(CGRectCorner corner, CGRect rect);
CGRect CGRectByMovingCornerOfCGRectByDelta(CGRectCorner corner, CGRect rect, CGSize delta, CGRect maxRect);
CGRect CGRectByMovingSideOfCGRectByDelta(CGRectSide side, CGRect rect, CGFloat delta, CGRect maxRect);

BOOL CGRectSideIsVertical(CGRectSide side);

void logMemUsage(void);

@interface SYTools : NSObject

+ (NSString *)documentsPath;
+ (NSString *)cachePath;

+ (NSString *)appSupportPath:(BOOL)create;
+ (NSString *)pathForFile:(NSString *)filename;
+ (void)createTestImages:(NSUInteger)count;
+ (NSDateFormatter *)logDateFormatter;


@end
