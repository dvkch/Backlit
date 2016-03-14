//
//  SYGalleryManager.m
//  SaneScanner
//
//  Created by Stan Chevallier on 14/03/2016.
//  Copyright © 2016 Syan. All rights reserved.
//

#import "SYGalleryManager.h"
#import "SYTools.h"
#import "MHGalleryItem.h"
#import "UIImage+SYKit.h"

@interface SYGalleryManager ()
@property (nonatomic, strong) NSCache <NSString *, UIImage *> *thumbanilCache;
@end

@implementation SYGalleryManager

+ (SYGalleryManager *)shared
{
    static dispatch_once_t onceToken;
    static SYGalleryManager *sharedInstance;
    dispatch_once(&onceToken, ^{
        sharedInstance = [[self alloc] init];
    });
    return sharedInstance;
}

- (instancetype)init
{
    self = [super init];
    if (self)
    {
        self.thumbanilCache = [[NSCache alloc] init];
    }
    return self;
}

- (NSArray <NSString *> *)allImageNames
{
    NSString *path = [SYTools documentsPath];
    NSMutableDictionary <NSString *, NSDate *> *imageDates = [NSMutableDictionary dictionary];
    
    NSArray <NSString *> *allDocuments = [[NSFileManager defaultManager] contentsOfDirectoryAtPath:path error:NULL];
    for (NSString *documentName in allDocuments)
    {
        if (![documentName.pathExtension isEqualToString:@"png"])
            continue;
        
        NSDate *date = [[NSFileManager defaultManager] attributesOfItemAtPath:documentName error:NULL][NSFileCreationDate];
        [imageDates setObject:date forKey:documentName];
    }
    
    NSArray <NSString *> *sortedImageNames = [imageDates keysSortedByValueUsingComparator:^NSComparisonResult(NSDate *obj1, NSDate *obj2) {
        return [obj1 compare:obj2];
    }];
    
    return sortedImageNames;
}

- (NSString *)pathForImageWithName:(NSString *)imageName thumbnail:(BOOL)thumbnail
{
    NSString *filename = thumbnail ? imageName : [imageName stringByAppendingPathExtension:@"thumb"];
    return [[SYTools documentsPath] stringByAppendingPathComponent:filename];
}

- (MHGalleryItem *)galleryItemForImageWithName:(NSString *)imageName
{
    return [MHGalleryItem itemWithURL:[self pathForImageWithName:imageName thumbnail:NO]
                         thumbnailURL:[self pathForImageWithName:imageName thumbnail:YES]];
}

- (UIImage *)thumbnailImageWithName:(NSString *)imageName
{
    UIImage *thumb = [self.thumbanilCache objectForKey:imageName];
    if (thumb)
        return thumb;
    
    thumb = [UIImage imageWithContentsOfFile:[self pathForImageWithName:imageName thumbnail:YES]];
    if (!thumb)
    {
        thumb = [self generateThumbnailFileForImageWithName:imageName];
    }
    
    [self.thumbanilCache setObject:thumb forKey:imageName];
    return thumb;
}

- (UIImage *)generateThumbnailFileForImageWithName:(NSString *)imageName
{
    UIImage *image = [UIImage imageWithContentsOfFile:[self pathForImageWithName:imageName thumbnail:NO]];
    if (!image)
        return nil;
    
    UIImage *thumb = [image sy_imageResizedSquarreTo:100];
    [UIImagePNGRepresentation(thumb) writeToFile:[self pathForImageWithName:imageName thumbnail:YES] atomically:YES];
    
    return thumb;
}

- (void)addImage:(UIImage *)image
{
    NSDateFormatter *formatter = [[NSDateFormatter alloc] init];
    [formatter setDateFormat:@"yyyy-MM-dd_HH-mm-ss"];
    [formatter setLocale:[NSLocale localeWithLocaleIdentifier:@"en_US_POSIX"]];
    
    NSString *imageName = [formatter stringFromDate:[NSDate date]];
    
    [UIImagePNGRepresentation(image) writeToFile:[self pathForImageWithName:imageName thumbnail:NO] atomically:YES];
    [self generateThumbnailFileForImageWithName:imageName];
}

- (void)deleteImageWithName:(NSString *)imageName
{
    [[NSFileManager defaultManager] removeItemAtPath:[self pathForImageWithName:imageName thumbnail:YES] error:NULL];
    [[NSFileManager defaultManager] removeItemAtPath:[self pathForImageWithName:imageName thumbnail:NO]  error:NULL];
}

@end
