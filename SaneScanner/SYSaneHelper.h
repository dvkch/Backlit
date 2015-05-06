//
//  SYSaneHelper.h
//  SaneScanner
//
//  Created by rominet on 06/05/15.
//  Copyright (c) 2015 Syan. All rights reserved.
//

#import <Foundation/Foundation.h>

@interface SYSaneHelper : NSObject

+ (void)updateSaneNetConfig;
+ (void)listDevices:(void(^)(NSArray *devices, NSString *error))completion;

@end
