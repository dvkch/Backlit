//
//  NSMutableData+SY.m
//  SaneScanner
//
//  Created by Stan Chevallier on 07/03/2016.
//  Copyright © 2016 Syan. All rights reserved.
//

#import "NSMutableData+SY.h"

@implementation NSMutableData (SY)

- (void)invertBitByBit
{
    uint8_t *bytes = self.mutableBytes;
    
    for (NSUInteger i = 0; i < self.length; ++i)
        bytes[i] = ~bytes[i];
}

@end
