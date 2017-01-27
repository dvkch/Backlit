//
//  SYSaneScanParameters.m
//  SaneScanner
//
//  Created by Stan Chevallier on 04/03/2016.
//  Copyright © 2016 Syan. All rights reserved.
//

#import "SYSaneScanParameters.h"

@implementation SYSaneScanParameters

+ (SYSaneScanParameters *)parametersForIncompleteDataOfLength:(NSUInteger)length
                                           completeParameters:(SYSaneScanParameters *)completeParameters
{
    SYSaneScanParameters *parameters = [[SYSaneScanParameters alloc] init];
    parameters.currentlyAcquiredChannel = completeParameters.currentlyAcquiredChannel;
    parameters.acquiringLastChannel     = completeParameters.acquiringLastChannel;
    parameters.bytesPerLine             = completeParameters.bytesPerLine;
    parameters.depth                    = completeParameters.depth;
    parameters.width                    = completeParameters.width;
    parameters.height                   = length / parameters.bytesPerLine;
    return parameters;
}

- (instancetype)initWithCParams:(SANE_Parameters)params
{
    self = [super init];
    if (self)
    {
        self.currentlyAcquiredChannel   = params.format;
        self.acquiringLastChannel       = params.last_frame;
        self.bytesPerLine               = params.bytes_per_line;
        self.depth                      = params.depth;
        self.width                      = params.pixels_per_line;
        self.height                     = params.lines;
    }
    return self;
}

- (NSUInteger)fileSize
{
    return self.bytesPerLine * self.height;
}

- (NSUInteger)numberOfChannels
{
    return self.currentlyAcquiredChannel == SANE_FRAME_RGB ? 3 : 1;
}

- (CGSize)size
{
    return CGSizeMake(self.width, self.height);
}

- (NSString *)description
{    
    return [NSString stringWithFormat:$$("<%@: %p, %ld*%ld*%ld, channel: %@, isLastChannel: %d, bytesPerLine: %ld>"),
            self.class, self,
            (long)self.width, (long)self.height, (long)self.depth,
            NSStringFromSANE_Frame(self.currentlyAcquiredChannel),
            self.acquiringLastChannel,
            (long)self.bytesPerLine];
}

@end

NSString *NSStringFromSANE_Frame(SANE_Frame frame)
{
    NSString *string = $$("unknown");
    switch (frame) {
        case SANE_FRAME_RGB:    string = $$("RGB");  break;
        case SANE_FRAME_RED:    string = $$("R");    break;
        case SANE_FRAME_GREEN:  string = $$("G");    break;
        case SANE_FRAME_BLUE:   string = $$("B");    break;
        case SANE_FRAME_GRAY:   string = $$("GREY"); break;
    }
    return string;
}
