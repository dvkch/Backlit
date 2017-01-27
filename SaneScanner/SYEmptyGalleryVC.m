//
//  SYEmptyGalleryVC.m
//  SaneScanner
//
//  Created by Stan Chevallier on 17/03/2016.
//  Copyright © 2016 Syan. All rights reserved.
//

#import "SYEmptyGalleryVC.h"
#import <Masonry.h>

@interface SYEmptyGalleryVC ()
@property (nonatomic, strong) UILabel *label;
@end

@implementation SYEmptyGalleryVC

- (void)viewDidLoad
{
    [super viewDidLoad];
    [self.view setBackgroundColor:[UIColor whiteColor]];
    
    NSMutableAttributedString *text = [[NSMutableAttributedString alloc] init];
    [text appendAttributedString:
     [[NSAttributedString alloc] initWithString:@"No images yet"
                                     attributes:@{NSFontAttributeName:[UIFont systemFontOfSize:17],
                                                  NSForegroundColorAttributeName:[UIColor darkGrayColor]}]];
    
    [text appendAttributedString:
     [[NSAttributedString alloc] initWithString:$$("\n\n")
                                     attributes:@{NSFontAttributeName:[UIFont systemFontOfSize:14],
                                                  NSForegroundColorAttributeName:[UIColor grayColor]}]];
    
    [text appendAttributedString:
     [[NSAttributedString alloc] initWithString:@"Scan a document with one of the devices on the left and it will appear here"
                                     attributes:@{NSFontAttributeName:[UIFont systemFontOfSize:14],
                                                  NSForegroundColorAttributeName:[UIColor grayColor]}]];
    
    self.label = [[UILabel alloc] init];
    [self.label setAttributedText:text];
    [self.label setNumberOfLines:0];
    [self.label setLineBreakMode:NSLineBreakByWordWrapping];
    [self.label setTextAlignment:NSTextAlignmentCenter];
    [self.view addSubview:self.label];

    [self.label mas_makeConstraints:^(MASConstraintMaker *make) {
        make.center.equalTo(@0);
        make.width.lessThanOrEqualTo(@300);
    }];
}

@end
