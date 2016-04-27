//
//  UIImageView+MHGallery.m
//  MHVideoPhotoGallery
//
//  Created by Mario Hahn on 06.02.14.
//  Copyright (c) 2014 Mario Hahn. All rights reserved.
//

#import "UIImageView+MHGallery.h"
#import "MHGallery.h"
#import "UIImageView+WebCache.h"

@implementation UIImageView (MHGallery)

-(void)setThumbWithURL:(NSURL*)URL
          successBlock:(void (^)(UIImage *image,NSUInteger videoDuration,NSError *error))succeedBlock{
    
    __weak typeof(self) weakSelf = self;
    
    [MHGallerySharedManager.sharedManager startDownloadingThumbImage:URL
                                                        successBlock:^(UIImage *image, NSUInteger videoDuration, NSError *error) {
                                                            
                                                            if (!weakSelf) return;
                                                            dispatch_main_sync_safe(^{
                                                                if (!weakSelf) return;
                                                                if (image){
                                                                    weakSelf.image = image;
                                                                    [weakSelf setNeedsLayout];
                                                                }
                                                                if (succeedBlock) {
                                                                    succeedBlock(image,videoDuration,error);
                                                                }
                                                            });
                                                        }];
}

-(void)setImageForMHGalleryItem:(MHGalleryItem*)item
                      imageType:(MHImageType)imageType
                   successBlock:(void (^)(UIImage *image,NSError *error))succeedBlock{
    
    __weak typeof(self) weakSelf = self;
    
    if ([item.URL.scheme isEqualToString:MHAssetLibrary]) {
        
        MHAssetImageType assetType = MHAssetImageTypeThumb;
        if (imageType == MHImageTypeFull) {
            assetType = MHAssetImageTypeFull;
        }
        
        [MHGallerySharedManager.sharedManager getImageFromAssetLibrary:item.URL
                                                             assetType:assetType
                                                          successBlock:^(UIImage *image, NSError *error) {
                                                              [weakSelf setImageForImageView:image successBlock:succeedBlock];
                                                          }];
    }else if(item.image){
        [self setImageForImageView:item.image successBlock:succeedBlock];
    }else{
        
        NSURL *placeholderURL = item.thumbnailURL;
        NSURL *toLoadURL = item.URL;
        
        if (imageType == MHImageTypeThumb) {
            toLoadURL = item.thumbnailURL;
            placeholderURL = item.URL;
        }
        
        SDWebImageOptions options = 0;
        
        // prevents storing a copy of a file that's already available on disk, only keep it in memory
        if ([toLoadURL isFileURL])
            options |= SDWebImageCacheMemoryOnly;
        
        [self sd_setImageWithURL:toLoadURL
                placeholderImage:[SDImageCache.sharedImageCache imageFromDiskCacheForKey:placeholderURL.absoluteString]
                         options:options
                       completed:^(UIImage *image, NSError *error, SDImageCacheType cacheType, NSURL *imageURL) {
                           if (succeedBlock) {
                               succeedBlock (image,error);
                           }
                       }];
    }
}


-(void)setImageForImageView:(UIImage*)image
               successBlock:(void (^)(UIImage *image,NSError *error))succeedBlock{
    
    __weak typeof(self) weakSelf = self;
    
    if (!weakSelf) return;
    dispatch_main_sync_safe(^{
        weakSelf.image = image;
        [weakSelf setNeedsLayout];
        if (succeedBlock) {
            succeedBlock(image,nil);
        }
    });
}



@end
