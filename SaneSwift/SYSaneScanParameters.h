//
//  SYSaneScanParameters.h
//  SaneScanner
//
//  Created by Stan Chevallier on 04/03/2016.
//  Copyright © 2016 Syan. All rights reserved.
//

#import <UIKit/UIKit.h>
#import "sane.h"

NSString *NSStringFromSANE_Frame(SANE_Frame frame);

@interface SYSaneScanParameters : NSObject

@property (nonatomic, assign) SANE_Frame currentlyAcquiredChannel;
@property (nonatomic, assign) BOOL acquiringLastChannel;
@property (nonatomic, assign) NSUInteger bytesPerLine;
@property (nonatomic, assign) NSUInteger width;
@property (nonatomic, assign) NSUInteger height;
@property (nonatomic, assign) NSUInteger depth;

+ (SYSaneScanParameters *)parametersForIncompleteDataOfLength:(NSUInteger)length
                                           completeParameters:(SYSaneScanParameters *)completeParameters;

- (instancetype)initWithCParams:(SANE_Parameters)params;

- (NSUInteger)fileSize;
- (NSUInteger)numberOfChannels;
- (CGSize)size;

@end
