//
//  SYSaneOptionBool.h
//  SaneScanner
//
//  Created by rominet on 06/05/15.
//  Copyright (c) 2015 Syan. All rights reserved.
//

#import "SYSaneOption.h"

NSString *NSStringFromSANE_Value_Type(SANE_Value_Type type);
NSString *NSStringFromSANE_Unit(SANE_Unit unit);

@interface SYSaneOptionBool : SYSaneOption

@property (nonatomic, assign) BOOL value;

@end
