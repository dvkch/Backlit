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

- (int)numberOfChannels
{
    return self.currentlyAcquiredChannel == SANE_FRAME_RGB ? 3 : 1;
}

- (NSString *)description
{    
    return [NSString stringWithFormat:@"<%@: %p, %d*%d*%d, channel: %@, isLastChannel: %d, bytesPerLine: %d>",
            self.class, self,
            self.width, self.height, self.depth,
            NSStringFromSANE_Frame(self.currentlyAcquiredChannel),
            self.acquiringLastChannel,
            self.bytesPerLine];
}

@end

NSString *NSStringFromSANE_Frame(SANE_Frame frame)
{
    NSString *string = @"unknown";
    switch (frame) {
        case SANE_FRAME_RGB:    string = @"RGB";  break;
        case SANE_FRAME_RED:    string = @"R";    break;
        case SANE_FRAME_GREEN:  string = @"G";    break;
        case SANE_FRAME_BLUE:   string = @"B";    break;
        case SANE_FRAME_GRAY:   string = @"GREY"; break;
    }
    return string;
}
