//
//  SYSaneScanParameters.h
//  SaneScanner
//
//  Created by Stan Chevallier on 04/03/2016.
//  Copyright © 2016 Syan. All rights reserved.
//

#import <Foundation/Foundation.h>
#import "sane.h"

NSString *NSStringFromSANE_Frame(SANE_Frame frame);

@interface SYSaneScanParameters : NSObject

@property (nonatomic, assign) SANE_Frame currentlyAcquiredChannel;
@property (nonatomic, assign) BOOL acquiringLastChannel;
@property (nonatomic, assign) int bytesPerLine;
@property (nonatomic, assign) int width;
@property (nonatomic, assign) int height;
@property (nonatomic, assign) int depth;

- (instancetype)initWithCParams:(SANE_Parameters)params;

- (int)fileSize;
- (int)numberOfChannels;

@end
