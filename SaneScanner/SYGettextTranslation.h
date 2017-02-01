//
//  SYGettextTranslation.h
//  SaneScanner
//
//  Created by Stanislas Chevallier on 01/02/2017.
//  Copyright © 2017 Syan. All rights reserved.
//

#import <Foundation/Foundation.h>

@interface SYGettextTranslation : NSObject

+ (instancetype)gettextTranslationWithContentsOfFile:(NSString *)filename;

- (NSString *)translationForKey:(NSString *)key;

@end
