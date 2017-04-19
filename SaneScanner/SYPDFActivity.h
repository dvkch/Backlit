//
//  SYPDFActivity.h
//  SaneScanner
//
//  Created by Stanislas Chevallier on 19/04/2017.
//  Copyright © 2017 Syan. All rights reserved.
//

#import <UIKit/UIKit.h>

@interface SYPDFActivity : UIActivity

@property (nonatomic, weak) UIViewController *presentingViewController;

- (void)configureWithOriginalActivityViewController:(UIActivityViewController *)originalActivityVC;

@end
