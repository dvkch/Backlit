//
//  NSAttributedString+SYKit.m
//  SYKit
//
//  Created by Stan Chevallier on 07/09/2016.
//  Copyright © 2016 Syan. All rights reserved.
//

#import "NSAttributedString+SYKit.h"

@implementation NSAttributedString (SYKit)

+ (instancetype)sy_stringWithText:(NSString *)text font:(UIFont *)font color:(UIColor *)color
{
    if (!text) return nil;
    
    NSMutableDictionary *attributes = [NSMutableDictionary dictionary];
    
    if (font)   [attributes setObject:font  forKey:NSFontAttributeName];
    if (color)  [attributes setObject:color forKey:NSForegroundColorAttributeName];
    
    return [[self alloc] initWithString:text attributes:attributes];
}

+ (instancetype)sy_stringWithStrings:(NSArray<NSAttributedString *> *)strings addLineBreak:(BOOL)addLineBreak
{
    if (!strings.count) return nil;
    if (strings.count == 1) return [strings firstObject];
    
    NSMutableAttributedString *result = [[NSMutableAttributedString alloc] init];
    for (NSUInteger i = 0; i < [strings count]; ++i)
    {
        NSAttributedString *string = strings[i];
        [result appendAttributedString:string];
        
        if (addLineBreak && i < [strings count] -1)
        {
            NSAttributedString *returnAttrStr = [self sy_stringWithText:@"\n" font:[UIFont systemFontOfSize:1.] color:nil];
            [result appendAttributedString:returnAttrStr];
        }
    }
    
    return [[self alloc] initWithAttributedString:result];
}

+ (instancetype)sy_stringWithStrings:(NSArray<NSAttributedString *> *)strings
{
    return [self sy_stringWithStrings:strings addLineBreak:NO];
}

+ (NSAttributedString *)sy_stringWithImage:(UIImage *)image verticalOffset:(CGFloat)verticalOffset
{
    NSTextAttachment *attachment = [[NSTextAttachment alloc] init];
    [attachment setImage:image];
    [attachment setBounds:CGRectMake(0, -verticalOffset, image.size.width, image.size.height)];
    return [self attributedStringWithAttachment:attachment];
}

static UILabel *SYKit_NSAttributedString_sizingLabel;

- (CGSize)sy_sizeInBoundingSize:(CGSize)size
{
    if (![[NSThread currentThread] isMainThread])
    {
        NSLog(@"Using -[%@ %@] in thread other than main thread (%@)",
              NSStringFromClass([self class]),
              NSStringFromSelector(_cmd),
              [[NSThread currentThread] name]);
        return CGSizeZero;
    }
    
    if (!SYKit_NSAttributedString_sizingLabel)
    {
        SYKit_NSAttributedString_sizingLabel = [[UILabel alloc] init];
        [SYKit_NSAttributedString_sizingLabel setNumberOfLines:0];
    }
    
    [SYKit_NSAttributedString_sizingLabel setAttributedText:self];
    CGSize s = [SYKit_NSAttributedString_sizingLabel sizeThatFits:size];
    return CGSizeMake(ceil(s.width), ceil(s.height));
}

- (CGSize)sy_sizeInBoundingWidth:(CGFloat)width
{
    return [self sy_sizeInBoundingSize:CGSizeMake(width, CGFLOAT_MAX)];
}

@end

@implementation NSMutableAttributedString (SYKit)

- (void)sy_append:(NSAttributedString *)string
{
    if (string)
        [self appendAttributedString:string];
}

- (void)sy_appendString:(NSString *)string font:(UIFont *)font color:(UIColor *)color
{
    NSAttributedString *attr = [NSAttributedString sy_stringWithText:string font:font color:color];
    [self sy_append:attr];
}

- (void)sy_setAlignment:(NSTextAlignment)alignment paragraphSpacing:(CGFloat)paragraphSpacing
{
    NSMutableParagraphStyle *paragraph = [[NSParagraphStyle defaultParagraphStyle] mutableCopy];
    [paragraph setAlignment:alignment];
    [paragraph setParagraphSpacing:paragraphSpacing];
    [self addAttribute:NSParagraphStyleAttributeName value:paragraph range:NSMakeRange(0, self.length)];
}

@end


