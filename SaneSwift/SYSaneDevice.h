//
//  SYSaneDevice.h
//  SaneScanner
//
//  Created by rominet on 06/05/15.
//  Copyright (c) 2015 Syan. All rights reserved.
//

#import <UIKit/UIKit.h>
#import "sane.h"
#import "SYSaneOptionGroup.h"

typedef NS_ENUM(NSUInteger, SYSaneStandardOption) {
    SYSaneStandardOptionUnknown,
    SYSaneStandardOptionPreview,
    SYSaneStandardOptionResolution,
    SYSaneStandardOptionResolutionX,
    SYSaneStandardOptionResolutionY,
    SYSaneStandardOptionColorMode,
    SYSaneStandardOptionAreaTopLeftX,
    SYSaneStandardOptionAreaTopLeftY,
    SYSaneStandardOptionAreaBottomRightX,
    SYSaneStandardOptionAreaBottomRightY,
};

typedef NS_ENUM(NSUInteger, SYOptionValue) {
    SYOptionValueAuto,
    SYOptionValueMin,
    SYOptionValueMax,
};

NSString *NSStringFromSYSaneStandardOption(SYSaneStandardOption stdOption);
SYSaneStandardOption SYSaneStandardOptionFromNSString(NSString *stdOption);
SYOptionValue SYBestValueForPreviewValueForOption(SYSaneStandardOption stdOption);

@interface SYSaneDevice : NSObject

@property (nonatomic, strong, readonly) NSString *name;
@property (nonatomic, strong, readonly) NSString *type;
@property (nonatomic, strong, readonly) NSString *vendor;
@property (nonatomic, strong, readonly) NSString *model;
@property (nonatomic, strong, readonly) NSArray <SYSaneOption *> *allOptions;
@property (nonatomic, assign, readonly) BOOL canCrop;
@property (nonatomic, assign, readonly) CGRect maxCropArea;
@property (nonatomic, strong) UIImage *lastPreviewImage;
@property (nonatomic, assign) CGRect cropArea;

- (instancetype)initWithCDevice:(const SANE_Device *)device;

- (NSString *)host;

- (CGFloat)previewImageRatio;

- (NSArray <SYSaneOptionGroup *> *)filteredGroupedOptionsWithoutAdvanced:(BOOL)removeAdvanced;

- (NSArray <SYSaneOption *> *)standardOptions:(NSArray <NSNumber *> *)stdOptions;
- (SYSaneOption *)standardOption:(SYSaneStandardOption)stdOption;
- (SYSaneOption *)optionWithIdentifier:(NSString *)identifier;

- (void)setOptions:(NSArray <SYSaneOption *> *)options;

@end
