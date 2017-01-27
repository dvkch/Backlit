//
//  MHCustomization.m
//  MHVideoPhotoGallery
//
//  Created by Mario Hahn on 04.03.14.
//  Copyright (c) 2014 Mario Hahn. All rights reserved.
//

#import "MHCustomization.h"
#import "MHGallery.h"

@implementation MHTransitionCustomization

- (instancetype)init{
    self = [super init];
    if (self) {
        self.interactiveDismiss = YES;
		self.dismissWithScrollGestureOnFirstAndLastImage = YES;
        self.fixXValueForDismiss = NO;
    }
    return self;
}
@end

@interface MHUICustomization()
@property(nonatomic,strong)NSMutableDictionary *backgroundColorsForViewModes;
@property(nonatomic,strong)NSMutableDictionary *gradientColorsForDirection;

@end
@implementation MHUICustomization

- (instancetype)init{
    self = [super init];
    if (self) {
        
        self.descriptionLinkAttributes = @{NSForegroundColorAttributeName : UIApplication.sharedApplication.keyWindow.tintColor ? : UIColor.redColor};
        self.descriptionActiveLinkAttributes = @{NSForegroundColorAttributeName : UIColor.whiteColor};
        self.descriptionTruncationString = [self truncationString];
        self.barButtonsTintColor = nil;
        self.barStyle = UIBarStyleDefault;
        self.barTintColor = nil;
		self.showMHShareViewInsteadOfActivityViewController = YES;
        self.useCustomBackButtonImageOnImageViewer = YES;
        self.showOverView = YES;
        self.hideShare = NO;
        self.backButtonState = MHBackButtonStateWithBackArrow;
        self.videoProgressTintColor = UIColor.blackColor;
        self.allowMultipleSelection = YES;
        
        self.backgroundColorsForViewModes = [NSMutableDictionary  dictionaryWithDictionary:@{@"0":UIColor.blackColor,
                                                                                             @"1":UIColor.whiteColor,
                                                                                             @"2":UIColor.whiteColor}];
        
        
        
        
        self.gradientColorsForDirection = [NSMutableDictionary dictionaryWithDictionary:@{@"0":@[[[UIColor blackColor] colorWithAlphaComponent:0.85],
                                                                                                [[UIColor blackColor] colorWithAlphaComponent:0.70],
                                                                                                [[UIColor blackColor] colorWithAlphaComponent:0.0]],
                                                                                         @"1":@[[[UIColor blackColor] colorWithAlphaComponent:0.0],
                                                                                                [[UIColor blackColor] colorWithAlphaComponent:0.70],
                                                                                                [[UIColor blackColor] colorWithAlphaComponent:0.85]]}];
        
        self.hideDoneButton = NO;
        self.overviewTitle = MHGalleryLocalizedString(@"overview.title.current");
        
        CGSize screenSize = UIScreen.mainScreen.bounds.size;
        UICollectionViewFlowLayout *flowLayout = UICollectionViewFlowLayout.new;
        flowLayout.scrollDirection = UICollectionViewScrollDirectionVertical;
        flowLayout.sectionInset = UIEdgeInsetsMake(4, 0, 0, 0);
        flowLayout.minimumInteritemSpacing = 4;
        flowLayout.minimumLineSpacing = 10;
        flowLayout.itemSize = CGSizeMake(screenSize.width/3.1, screenSize.width/3.1);
        self.overviewCollectionViewLayout = flowLayout;
        
        self.overviewCollectionViewCellClass = MHMediaPreviewCollectionViewCell.class;
    }
    return self;
}

-(NSAttributedString*)truncationString{
    NSString *points = @"...";
    NSString *more = MHGalleryLocalizedString(@"truncate.more");
    NSString *wholeString = [points stringByAppendingString:more];
    
    NSMutableAttributedString *truncation = [NSMutableAttributedString.alloc initWithString:wholeString attributes:@{NSFontAttributeName : [UIFont systemFontOfSize:14],NSForegroundColorAttributeName : UIColor.whiteColor}];
    
    NSDictionary *attributes = @{NSFontAttributeName : [UIFont systemFontOfSize:14],
                                 NSForegroundColorAttributeName : UIApplication.sharedApplication.keyWindow.tintColor ? : UIColor.whiteColor};
    
    [truncation setAttributes:attributes range:NSMakeRange(points.length, more.length)];
    return truncation;
}

-(void)setMHGradients:(NSArray<UIColor*>*)colors forDirection:(MHGradientDirection)direction{
    [self.gradientColorsForDirection setObject:colors forKey:[NSString stringWithFormat:@"%@",@(direction)]];
}

-(NSArray<UIColor*>*)MHGradientColorsForDirection:(MHGradientDirection)direction{
    return self.gradientColorsForDirection[[NSString stringWithFormat:@"%@",@(direction)]];
}

-(void)setMHGalleryBackgroundColor:(UIColor *)color forViewMode:(MHGalleryViewMode)viewMode{
    [self.backgroundColorsForViewModes setObject:color forKey:[NSString stringWithFormat:@"%@",@(viewMode)]];
}

-(UIColor*)MHGalleryBackgroundColorForViewMode:(MHGalleryViewMode)viewMode{
    return self.backgroundColorsForViewModes[[NSString stringWithFormat:@"%@",@(viewMode)]];
}


@end
