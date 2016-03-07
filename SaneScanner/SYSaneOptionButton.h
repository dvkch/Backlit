//
//  SYSaneOptionButton.h
//  SaneScanner
//
//  Created by rominet on 06/05/15.
//  Copyright (c) 2015 Syan. All rights reserved.
//

#import "SYSaneOption.h"

@interface SYSaneOptionButton : SYSaneOption

- (void)press:(void(^)(NSString *error))block;

@end
