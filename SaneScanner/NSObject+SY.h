//
//  NSObject+SY.h
//  SaneScanner
//
//  Created by Stan Chevallier on 03/03/2016.
//  Copyright © 2016 Syan. All rights reserved.
//

#import <Foundation/Foundation.h>

@interface NSObject (SY)

- (void)performSelector:(SEL)selector onThread:(NSThread *)thread withArguments:(NSUInteger)argumentsCount,...;

@end
