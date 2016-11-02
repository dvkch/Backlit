//
//  SYMediaPreviewCollectionViewCell.m
//  SaneScanner
//
//  Created by Stan Chevallier on 03/05/2016.
//  Copyright © 2016 Syan. All rights reserved.
//

#import "SYMediaPreviewCollectionViewCell.h"
#import "SYGalleryManager.h"
#import "MHGalleryItem+SY.h"

@interface SYMediaPreviewCollectionViewCell () <SYGalleryManagerDelegate>
@property (nonatomic, strong) MHGalleryItem *item;
@end

@implementation SYMediaPreviewCollectionViewCell

- (instancetype)initWithFrame:(CGRect)frame
{
    self = [super initWithFrame:frame];
    if (self)
    {
        [[SYGalleryManager shared] addDelegate:self];
    }
    return self;
}

- (MHGalleryItem *)galleryItem
{
    return self.item;
}

-(void)setGalleryItem:(MHGalleryItem *)galleryItem{
    
    self.item = galleryItem;
    
    UIImage *thumb = [[SYGalleryManager shared] thumbnailForItem:self.galleryItem];
    [self.thumbnail setImage:thumb];
    [self.thumbnail setNeedsLayout];
    
    if (thumb)
    {
        [self.activityIndicator stopAnimating];
        [self.thumbnail setBackgroundColor:[UIColor whiteColor]];
    }
    else
    {
        [self.activityIndicator startAnimating];
        [self.thumbnail setBackgroundColor:[UIColor lightGrayColor]];
    }
}

- (void)gallerymanager:(SYGalleryManager *)gallerymanager
       didCreatedThumb:(UIImage *)thumb
               forItem:(MHGalleryItem *)item
{
    if ([self.item isEqual:item])
    {
        [self.thumbnail setImage:thumb];
        [self.thumbnail setNeedsLayout];
        [self.activityIndicator stopAnimating];
        [self.thumbnail setBackgroundColor:[UIColor whiteColor]];
    }
}

@end
