//
//  SYSaneOptionUI.h
//  SaneScanner
//
//  Created by Stan Chevallier on 06/03/2016.
//  Copyright © 2016 Syan. All rights reserved.
//

#import <Foundation/Foundation.h>

@class SYSaneOption;

@interface SYSaneOptionUI : NSObject

+ (void)showDetailsForOption:(SYSaneOption *)option;
+ (void)showDetailsAndInputForOption:(SYSaneOption *)option
                               block:(void(^)(BOOL reloadAllOptions, NSString *error))block;

@end
