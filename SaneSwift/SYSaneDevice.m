//
//  SYSaneDevice.m
//  SaneScanner
//
//  Created by rominet on 06/05/15.
//  Copyright (c) 2015 Syan. All rights reserved.
//

#import "SYSaneDevice.h"
#import "SYSaneOptionNumber.h"
#import "saneopts.h"
#import "SYSaneHelper.h"

#define NSStringFromDef(string) ([NSString stringWithCString:string encoding:NSUTF8StringEncoding])

@interface SYSaneDevice ()
@property (nonatomic, strong, readwrite) NSString *name;
@property (nonatomic, strong, readwrite) NSString *type;
@property (nonatomic, strong, readwrite) NSString *vendor;
@property (nonatomic, strong, readwrite) NSString *model;
@property (nonatomic, strong, readwrite) NSArray <SYSaneOption *> *allOptions;
@property (nonatomic, assign, readwrite) BOOL canCrop;
@property (nonatomic, assign, readwrite) CGRect maxCropArea;
@property (nonatomic, strong) NSNumber *previewImageRatioN;
@end

@implementation SYSaneDevice

- (instancetype)initWithCDevice:(const SANE_Device *)device
{
    if (!device)
        return nil;
    
    self = [super init];
    if (self)
    {
        self.name   = [NSString stringWithCString:(device->name   ?: "") encoding:NSUTF8StringEncoding];
        self.model  = [NSString stringWithCString:(device->model  ?: "") encoding:NSUTF8StringEncoding];
        self.vendor = [NSString stringWithCString:(device->vendor ?: "") encoding:NSUTF8StringEncoding];
        self.type   = [NSString stringWithCString:(device->type   ?: "") encoding:NSUTF8StringEncoding];
        
        self.type   = [[SYSaneHelper shared] translationForKey:self.type];
    }
    return self;
}

- (NSString *)host
{
    // seems to be working that way, can change
    return [self.name componentsSeparatedByString:@":"].firstObject;
}

- (CGFloat)previewImageRatio
{
    if (!self.allOptions)
        return 0.;
    
    if (!self.previewImageRatioN)
    {
        if (!CGRectEqualToRect(self.maxCropArea, CGRectZero))
        {
            self.previewImageRatioN = @(self.maxCropArea.size.width / self.maxCropArea.size.height);
        }
        else if (self.lastPreviewImage)
        {
            self.previewImageRatioN = @(self.lastPreviewImage.size.width / self.lastPreviewImage.size.height);
        }
    }
    
    return self.previewImageRatioN.doubleValue;
}

- (NSArray <SYSaneOption *> *)standardOptions:(NSArray <NSNumber *> *)stdOptions
{
    NSMutableArray <SYSaneOption *> *options = [NSMutableArray array];
    for (NSNumber *stdOptionN in stdOptions)
    {
        SYSaneOption *option = [self standardOption:(SYSaneStandardOption)[stdOptionN unsignedIntegerValue]];
        if (option)
            [options addObject:option];
    }
    return [options copy];
}

- (SYSaneOption *)standardOption:(SYSaneStandardOption)stdOption
{
    return [self optionWithIdentifier:NSStringFromSYSaneStandardOption(stdOption)];
}

- (SYSaneOption *)optionWithIdentifier:(NSString *)identifier
{
    for (SYSaneOption *option in self.allOptions)
        if ([option.identifier isEqualToString:identifier])
            return option;
    return nil;
}

- (void)setOptions:(NSArray <SYSaneOption *> *)options
{
    self.allOptions = options;
    
    NSArray <SYSaneOption *> *cropOptions = [self standardOptions:@[@(SYSaneStandardOptionAreaTopLeftX),
                                                                    @(SYSaneStandardOptionAreaTopLeftY),
                                                                    @(SYSaneStandardOptionAreaBottomRightX),
                                                                    @(SYSaneStandardOptionAreaBottomRightY)]];
    
    BOOL allSettable = YES;
    for (SYSaneOption *option in cropOptions)
        if (!option.capSettableViaSoftware || option.capInactive)
            allSettable = NO;
    
    if (cropOptions.count == 4 && allSettable)
    {
        double tlX = 0, tlY = 0, brX = 0, brY = 0;
        
        for (SYSaneOption *option in cropOptions)
        {
            SYSaneOptionNumber *castedOption = (SYSaneOptionNumber *)option;
            SYSaneStandardOption stdOption = SYSaneStandardOptionFromNSString(option.identifier);
            
            if (stdOption == SYSaneStandardOptionAreaTopLeftX)
                tlX = castedOption.bestValueForPreview.doubleValue;
            
            if (stdOption == SYSaneStandardOptionAreaTopLeftY)
                tlY = castedOption.bestValueForPreview.doubleValue;
            
            if (stdOption == SYSaneStandardOptionAreaBottomRightX)
                brX = castedOption.bestValueForPreview.doubleValue;
            
            if (stdOption == SYSaneStandardOptionAreaBottomRightY)
                brY = castedOption.bestValueForPreview.doubleValue;
        }
        
        self.maxCropArea = CGRectMake(tlX, tlY, brX - tlX, brY - tlY);
        
        // keep old crop values if possible in case all options are refreshed
        self.cropArea = CGRectIntersection(self.maxCropArea, self.cropArea);
        
        // define default value if current crop area is not defined
        if (CGRectEqualToRect(self.cropArea, CGRectZero))
            self.cropArea = self.maxCropArea;
        
        self.canCrop = YES;
    }
}

- (NSArray <SYSaneOptionGroup *> *)filteredGroupedOptionsWithoutAdvanced:(BOOL)removeAdvanced
{
    NSMutableArray <SYSaneOption *> *filteredOptions = [self.allOptions mutableCopy];
    [filteredOptions removeObject:[self standardOption:SYSaneStandardOptionPreview]];
    
    if (self.canCrop)
    {
        NSArray <SYSaneOption *> *cropOptions = [self standardOptions:@[@(SYSaneStandardOptionAreaTopLeftX),
                                                                        @(SYSaneStandardOptionAreaTopLeftY),
                                                                        @(SYSaneStandardOptionAreaBottomRightX),
                                                                        @(SYSaneStandardOptionAreaBottomRightY)]];
        [filteredOptions removeObjectsInArray:cropOptions];
    }
    
    if (removeAdvanced)
    {
        for (SYSaneOption *option in self.allOptions)
            if (option.capAdvanced)
                [filteredOptions removeObject:option];
    }
    
    return [SYSaneOption groupedElements:filteredOptions removeEmptyGroups:YES];
}

@end


NSString *NSStringFromSYSaneStandardOption(SYSaneStandardOption stdOption)
{
    NSDictionary <NSNumber *, NSString *> *dic = @{@(SYSaneStandardOptionPreview):          NSStringFromDef(SANE_NAME_PREVIEW),
                                                   @(SYSaneStandardOptionResolution):       NSStringFromDef(SANE_NAME_SCAN_RESOLUTION),
                                                   @(SYSaneStandardOptionResolutionX):      NSStringFromDef(SANE_NAME_SCAN_X_RESOLUTION),
                                                   @(SYSaneStandardOptionResolutionY):      NSStringFromDef(SANE_NAME_SCAN_Y_RESOLUTION),
                                                   @(SYSaneStandardOptionColorMode):        NSStringFromDef(SANE_NAME_SCAN_MODE),
                                                   @(SYSaneStandardOptionAreaTopLeftX):     NSStringFromDef(SANE_NAME_SCAN_TL_X),
                                                   @(SYSaneStandardOptionAreaTopLeftY):     NSStringFromDef(SANE_NAME_SCAN_TL_Y),
                                                   @(SYSaneStandardOptionAreaBottomRightX): NSStringFromDef(SANE_NAME_SCAN_BR_X),
                                                   @(SYSaneStandardOptionAreaBottomRightY): NSStringFromDef(SANE_NAME_SCAN_BR_Y),
                                                   };
    
    return dic[@(stdOption)];
}

SYSaneStandardOption SYSaneStandardOptionFromNSString(NSString *stdOption)
{
    NSDictionary <NSNumber *, NSString *> *dic = @{@(SYSaneStandardOptionPreview):          NSStringFromDef(SANE_NAME_PREVIEW),
                                                   @(SYSaneStandardOptionResolution):       NSStringFromDef(SANE_NAME_SCAN_RESOLUTION),
                                                   @(SYSaneStandardOptionResolutionX):      NSStringFromDef(SANE_NAME_SCAN_X_RESOLUTION),
                                                   @(SYSaneStandardOptionResolutionY):      NSStringFromDef(SANE_NAME_SCAN_Y_RESOLUTION),
                                                   @(SYSaneStandardOptionColorMode):        NSStringFromDef(SANE_NAME_SCAN_MODE),
                                                   @(SYSaneStandardOptionAreaTopLeftX):     NSStringFromDef(SANE_NAME_SCAN_TL_X),
                                                   @(SYSaneStandardOptionAreaTopLeftY):     NSStringFromDef(SANE_NAME_SCAN_TL_Y),
                                                   @(SYSaneStandardOptionAreaBottomRightX): NSStringFromDef(SANE_NAME_SCAN_BR_X),
                                                   @(SYSaneStandardOptionAreaBottomRightY): NSStringFromDef(SANE_NAME_SCAN_BR_Y),
                                                   };
    
    NSArray <NSNumber *> *keys = [dic allKeysForObject:stdOption];
    return keys.firstObject ? keys.firstObject.unsignedIntegerValue : SYSaneStandardOptionUnknown;
}

SYOptionValue SYBestValueForPreviewValueForOption(SYSaneStandardOption stdOption)
{
    switch (stdOption) {
        case SYSaneStandardOptionColorMode:         return SYOptionValueAuto;
        case SYSaneStandardOptionResolution:        return SYOptionValueMin;
        case SYSaneStandardOptionResolutionX:       return SYOptionValueMin;
        case SYSaneStandardOptionResolutionY:       return SYOptionValueMin;
        case SYSaneStandardOptionAreaTopLeftX:      return SYOptionValueMin;
        case SYSaneStandardOptionAreaTopLeftY:      return SYOptionValueMin;
        case SYSaneStandardOptionAreaBottomRightX:  return SYOptionValueMax;
        case SYSaneStandardOptionAreaBottomRightY:  return SYOptionValueMax;
        default:
            return SYOptionValueAuto;
    }
}
