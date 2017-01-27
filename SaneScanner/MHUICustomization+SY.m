//
//  MHUICustomization+SY.m
//  SaneScanner
//
//  Created by Stan Chevallier on 22/04/2016.
//  Copyright © 2016 Syan. All rights reserved.
//

#import "MHUICustomization+SY.h"
#import "SYGridLayout.h"
#import "SYMediaPreviewCollectionViewCell.h"

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
    [theme setOverviewCollectionViewCellClass:SYMediaPreviewCollectionViewCell.class];
    
    SYGridLayout *layout = [[SYGridLayout alloc] init];
    [layout setMaxSize:320/3];
    [layout setMargin:2.];
    [theme setOverviewCollectionViewLayout:layout];
    
    return theme;
}

@end
