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
    
    self.label = [[UILabel alloc] init];
    [self.label setText:@"No images yet. Scan a document with one of the devices on the left and it will appear here"];
    [self.label setTextColor:[UIColor darkGrayColor]];
    [self.view addSubview:self.label];

    [self.label mas_makeConstraints:^(MASConstraintMaker *make) {
        make.center.equalTo(@0);
        make.width.lessThanOrEqualTo(@300);
    }];
}

@end
