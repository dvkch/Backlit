//
//  SYCropMaskView.h
//  SaneScanner
//
//  Created by Stan Chevallier on 08/03/2016.
//  Copyright © 2016 Syan. All rights reserved.
//

#import <UIKit/UIKit.h>

@interface SYCropMaskView : UIView

@property (nonatomic, assign, readonly) CGRect maxCropArea;
@property (nonatomic, assign, readonly) CGRect cropArea;
@property (nonatomic, copy) void(^cropAreaDidChangeBlock)(CGRect newCropArea);

- (void)setCropArea:(CGRect)cropArea andMaxCropArea:(CGRect)maxCropArea;

@end
