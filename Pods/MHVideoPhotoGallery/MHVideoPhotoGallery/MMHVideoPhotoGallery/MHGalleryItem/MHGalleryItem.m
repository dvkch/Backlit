//
//  MHGalleryItem.m
//  MHVideoPhotoGallery
//
//  Created by Mario Hahn on 01.04.14.
//  Copyright (c) 2014 Mario Hahn. All rights reserved.
//

#import "MHGalleryItem.h"
#import <SDWebImageManager.h>

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

- (void)getImageWithCompletion:(void (^)(UIImage *, NSError *))completion{
    
    if (self.galleryType == MHGalleryTypeVideo) {
        completion(nil, nil);
        return;
    }
    
    if ([self.URL.scheme isEqualToString:MHAssetLibrary]) {
        
        MHAssetImageType assetType = MHAssetImageTypeFull;
        
        [MHGallerySharedManager.sharedManager getImageFromAssetLibrary:self.URL
                                                             assetType:assetType
                                                          successBlock:^(UIImage *image, NSError *error) {
                                                              completion(image, error);
                                                          }];
    }else if(self.image){
        completion(self.image, nil);
    }else if(self.URL){
        
        SDWebImageOptions options = 0;
        
        // prevents storing a copy of a file that's already available on disk, only keep it in memory
        if ([self.URL isFileURL])
            options |= SDWebImageCacheMemoryOnly;
        
        [SDWebImageManager.sharedManager downloadImageWithURL:self.URL
                                                      options:options
                                                     progress:nil
                                                    completed:^(UIImage *image, NSError *error, SDImageCacheType cacheType, BOOL finished, NSURL *imageURL)
        {
            completion(image, error);
        }];
        
    }else{
        completion(nil, nil);
    }
    
}

+ (void)getImagesForItems:(NSArray <MHGalleryItem *> *)items
               orFileURLs:(BOOL)useFileURLs
               completion:(void(^)(NSArray *images, NSArray <NSError *> *errors))completion
{
    NSMutableDictionary <NSNumber *, id> *orderedImages = [NSMutableDictionary dictionary];
    NSMutableDictionary <NSNumber *, id> *orderedErrors = [NSMutableDictionary dictionary];
    
    dispatch_group_t group = dispatch_group_create();
    NSUInteger groupCount = 0;
    
    for (NSUInteger i = 0; i < items.count; ++i)
    {
        MHGalleryItem *item = items[i];
        
        if (useFileURLs && item.URL.isFileURL)
        {
            orderedImages[@(i)] = item.URL;
            continue;
        }
        
        ++groupCount;
        dispatch_group_enter(group);
        [item getImageWithCompletion:^(UIImage *image, NSError *error) {
            if (image) orderedImages[@(i)] = image;
            if (error) orderedErrors[@(i)] = error;
            dispatch_group_leave(group);
        }];
    }
    
    void(^block)(void) = ^
    {
        NSArray <NSNumber *> *sortedImageKeys = [[orderedImages allKeys] sortedArrayUsingSelector:@selector(compare:)];
        NSArray <NSNumber *> *sortedErrorKeys = [[orderedErrors allKeys] sortedArrayUsingSelector:@selector(compare:)];
        
        NSMutableArray *images = [[orderedImages objectsForKeys:sortedImageKeys notFoundMarker:[NSNull null]] mutableCopy];
        NSMutableArray *errors = [[orderedErrors objectsForKeys:sortedErrorKeys notFoundMarker:[NSNull null]] mutableCopy];
        
        [images removeObject:[NSNull null]];
        [errors removeObject:[NSNull null]];
        
        if (completion)
            completion(images.copy, errors.copy);
    };
    
    if (groupCount)
    {
        dispatch_notify(group, dispatch_get_main_queue(), ^{
            block();
        });
    }
    else
    {
        block();
    }
}

+ (void)getImagesForItems:(NSArray <MHGalleryItem *> *)items completion:(void (^)(NSArray<UIImage *> *, NSArray<NSError *> *))completion
{
    [self getImagesForItems:items orFileURLs:NO completion:completion];
}

+ (void)getImagesOrFileURLsForItems:(NSArray <MHGalleryItem *> *)items completion:(void (^)(NSArray *, NSArray<NSError *> *))completion
{
    [self getImagesForItems:items orFileURLs:YES completion:completion];
}

@end

