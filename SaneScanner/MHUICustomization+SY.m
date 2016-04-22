//
//  MHUICustomization+SY.m
//  SaneScanner
//
//  Created by Stan Chevallier on 22/04/2016.
//  Copyright © 2016 Syan. All rights reserved.
//

#import "MHUICustomization+SY.h"

@implementation MHUICustomization (SY)

+ (MHUICustomization *)sy_defaultTheme
{
    MHUICustomization *theme = [[self alloc] init];
    [theme setMHGalleryBackgroundColor:[UIColor groupTableViewBackgroundColor]
                                          forViewMode:MHGalleryViewModeImageViewerNavigationBarShown];
    [theme setShowMHShareViewInsteadOfActivityViewController:NO];
    [theme setShowOverView:YES];
    [theme setUseCustomBackButtonImageOnImageViewer:YES];
    [theme setBackButtonState:MHBackButtonStateWithBackArrow];
    [theme setOverviewTitle:@"All scans"];
    
    return theme;
}

@end
