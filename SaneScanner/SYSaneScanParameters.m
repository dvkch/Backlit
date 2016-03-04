//
//  SYSaneScanParameters.m
//  SaneScanner
//
//  Created by Stan Chevallier on 04/03/2016.
//  Copyright © 2016 Syan. All rights reserved.
//

#import "SYSaneScanParameters.h"

@implementation SYSaneScanParameters

- (instancetype)initWithCParams:(SANE_Parameters)params
{
    self = [super init];
    if (self)
    {
        self.currentlyAcquiredChannel   = params.format;
        self.acquiringLastChannel       = params.last_frame;
        self.bytesPerLine               = params.bytes_per_line;
        self.width                      = params.pixels_per_line;
        self.height                     = params.lines;
        self.depth                      = params.depth;
    }
    return self;
}

- (int)fileSize
{
    return self.bytesPerLine * self.height;
}

- (NSString *)description
{
    NSString *channel = @"unknown";
    switch (self.currentlyAcquiredChannel) {
        case SANE_FRAME_RGB:    channel = @"RGB";   break;
        case SANE_FRAME_RED:    channel = @"R";     break;
        case SANE_FRAME_GREEN:  channel = @"G";     break;
        case SANE_FRAME_BLUE:   channel = @"B";     break;
        case SANE_FRAME_GRAY:   channel = @"GREY";  break;
    }
    
    return [NSString stringWithFormat:@"<%@: %p, %d*%d*%d, channel: %@, isLastChannel: %d, bytesPerLine: %d>",
            self.class, self,
            self.width, self.height, self.depth,
            channel, self.acquiringLastChannel, self.bytesPerLine];
}

@end
