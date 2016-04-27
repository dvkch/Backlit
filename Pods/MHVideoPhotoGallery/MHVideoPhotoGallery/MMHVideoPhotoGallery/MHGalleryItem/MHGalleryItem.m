//
//  MHGalleryItem.m
//  MHVideoPhotoGallery
//
//  Created by Mario Hahn on 01.04.14.
//  Copyright (c) 2014 Mario Hahn. All rights reserved.
//

#import "MHGalleryItem.h"

@implementation MHGalleryItem

- (instancetype)initWithImage:(UIImage*)image{
    self = [super init];
    if (!self)
        return nil;
    self.galleryType = MHGalleryTypeImage;
    self.image = image;
    return self;
}

+ (instancetype)itemWithVimeoVideoID:(NSString*)ID{
    return [self.class.alloc initWithURLString:[NSString stringWithFormat:MHVimeoBaseURL, ID]
                                   galleryType:MHGalleryTypeVideo];
}

+ (instancetype)itemWithYoutubeVideoID:(NSString*)ID{
    return [self.class.alloc initWithURLString:[NSString stringWithFormat:MHYoutubeBaseURL, ID]
                                   galleryType:MHGalleryTypeVideo];
}

+(instancetype)itemWithURL:(NSURL *)URL
               galleryType:(MHGalleryType)galleryType{
    
    return [self.class.alloc initWithURL:URL
                             galleryType:galleryType];
}

- (instancetype)initWithURL:(NSURL *)URL
                galleryType:(MHGalleryType)galleryType{
    self = [super init];
    if (!self)
        return nil;
    self.URL = URL;
    self.thumbnailURL = URL;
    self.titleString = nil;
    self.attributedTitle = nil;
    self.descriptionString = nil;
    self.galleryType = galleryType;
    self.attributedString = nil;
    return self;
}

- (instancetype)initWithURLString:(NSString *)URLString
                      galleryType:(MHGalleryType)galleryType{
    self = [super init];
    if (!self)
        return nil;
    self.URL = [NSURL URLWithString:URLString];
    self.thumbnailURL = [NSURL URLWithString:URLString];
    self.titleString = nil;
    self.attributedTitle = nil;
    self.descriptionString = nil;
    self.galleryType = galleryType;
    self.attributedString = nil;
    return self;
}

+(instancetype)itemWithURL:(NSURL *)URL
              thumbnailURL:(NSURL *)thumbnailURL{
    
    return [self.class.alloc initWithURL:URL
                            thumbnailURL:thumbnailURL];
}


- (instancetype)initWithURL:(NSURL *)URL
               thumbnailURL:(NSURL *)thumbnailURL{
    self = [super init];
    if (!self)
        return nil;
    self.URL = URL;
    self.thumbnailURL = thumbnailURL;
    self.attributedTitle = nil;
    self.descriptionString = nil;
    self.descriptionString = nil;
    self.galleryType = MHGalleryTypeImage;
    self.attributedString = nil;
    return self;
}


+(instancetype)itemWithImage:(UIImage *)image{
    return [[self alloc] initWithImage:image];
}

- (NSUInteger)hash{
    if (self.URL.absoluteString.length)
        return self.URL.hash;
    
    if (self.image)
        return self.image.hash;
    
    return [super hash];
}

- (BOOL)isEqual:(id)object{
    if (![object isKindOfClass:[self class]])
        return NO;
    
    MHGalleryItem *castedObject = object;
    if (self.URL.absoluteString.length && castedObject.URL.absoluteString.length)
        return [self.URL.absoluteString isEqualToString:castedObject.URL.absoluteString];
    
    if (self.image && castedObject.image)
        return self.image == castedObject.image;
    
    return self == castedObject;
}

@end

