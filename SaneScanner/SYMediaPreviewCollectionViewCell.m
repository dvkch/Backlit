//
//  SYMediaPreviewCollectionViewCell.m
//  SaneScanner
//
//  Created by Stan Chevallier on 03/05/2016.
//  Copyright © 2016 Syan. All rights reserved.
//

#import "SYMediaPreviewCollectionViewCell.h"
#import "SaneScanner-Swift.h"
#import "MHGalleryItem+SY.h"

@interface SYMediaPreviewCollectionViewCell () <GalleryManagerDelegate>
@property (nonatomic, strong) MHGalleryItem *item;
@end

@implementation SYMediaPreviewCollectionViewCell

- (instancetype)initWithFrame:(CGRect)frame
{
    self = [super initWithFrame:frame];
    if (self)
    {
        [GalleryManager.shared addDelegate:self];
    }
    return self;
}

- (MHGalleryItem *)galleryItem
{
    return self.item;
}

-(void)setGalleryItem:(MHGalleryItem *)galleryItem{
    
    self.item = galleryItem;
    
    UIImage *thumb = [GalleryManager.shared thumbnailFor:self.galleryItem];
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

- (void)galleryManager:(GalleryManager *)manager
             didCreate:(UIImage *)thumbnail
                   for:(MHGalleryItem *)item
{
    if ([self.item isEqual:item])
    {
        [self.thumbnail setImage:thumbnail];
        [self.thumbnail setNeedsLayout];
        [self.activityIndicator stopAnimating];
        [self.thumbnail setBackgroundColor:[UIColor whiteColor]];
    }
}

@end
