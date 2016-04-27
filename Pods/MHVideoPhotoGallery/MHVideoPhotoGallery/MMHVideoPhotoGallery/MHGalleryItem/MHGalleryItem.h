//
//  MHGalleryItem.h
//  MHVideoPhotoGallery
//
//  Created by Mario Hahn on 01.04.14.
//  Copyright (c) 2014 Mario Hahn. All rights reserved.
//

#import <Foundation/Foundation.h>
#import "MHGallery.h"

typedef NS_ENUM(NSUInteger, MHGalleryType) {
    MHGalleryTypeImage,
    MHGalleryTypeVideo
};

@interface MHGalleryItem : NSObject

@property (nonatomic,strong) UIImage            *image;
@property (nonatomic,strong) NSURL              *URL;
/**
 *  Thumbs are automatically generated for Videos. But you can set Thumb Images for GalleryTypeImage.
 */
@property (nonatomic,strong) NSURL              *thumbnailURL;
@property (nonatomic,strong) NSString           *titleString;
@property (nonatomic,strong) NSAttributedString *attributedTitle;
@property (nonatomic,strong) NSString           *descriptionString;
@property (nonatomic,strong) NSAttributedString *attributedString;
@property (nonatomic,assign) MHGalleryType       galleryType;
/**
 *  MHGalleryItem initWithURL:galleryType
 *
 *  @param urlString   the URL of the image or Video as a String
 *  @param galleryType select to Type, video or image
 *
 */

- (instancetype)initWithURL:(NSURL *)URL
               thumbnailURL:(NSURL *)thumbnailURL;

+ (instancetype)itemWithURL:(NSURL *)URL
               thumbnailURL:(NSURL *)thumbnailURL;

- (instancetype)initWithURL:(NSURL *)URL
                galleryType:(MHGalleryType)galleryType;

- (instancetype)initWithURLString:(NSString *)URLString
                      galleryType:(MHGalleryType)galleryType;

+ (instancetype)itemWithURL:(NSURL *)URL
                galleryType:(MHGalleryType)galleryType;

/**
 *  MHGalleryItem itemWithYoutubeVideoID:
 *
 *  @param ID  Example: http://www.youtube.com/watch?v=YSdJtNen-EA - YSdJtNen-EA is the ID
 *
 */
+ (instancetype)itemWithYoutubeVideoID:(NSString*)ID;
/**
 *  MHGalleryItem itemWithVimeoVideoID:
 *
 *  @param ID Example: http://vimeo.com/35515926 - 35515926 is the ID
 *
 */
+ (instancetype)itemWithVimeoVideoID:(NSString*)ID;

/**
 *  MHGalleryItem initWithImage
 *
 *  @param image to Display
 *
 */
- (instancetype)initWithImage:(UIImage*)image;
+ (instancetype)itemWithImage:(UIImage*)image;

@end
