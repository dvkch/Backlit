//
//  SYGalleryManager.m
//  SaneScanner
//
//  Created by Stan Chevallier on 14/03/2016.
//  Copyright © 2016 Syan. All rights reserved.
//

#import "SYGalleryManager.h"
#import "SYTools.h"
#import <MHGalleryItem.h>
#import <UIImage+SYKit.h>
#import <SGDirWatchdog.h>

@interface SYGalleryManager ()
@property (nonatomic, strong) NSCache <NSString *, UIImage *> *thumbanilCache;
@property (nonatomic, strong) NSMutableSet <NSValue *> *delegates;
@property (nonatomic, strong) SGDirWatchdog *filesystemObserver;
@property (nonatomic, strong, readwrite) NSArray <NSString *> *imageNames;
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
        self.delegates = [NSMutableSet set];
        self.thumbanilCache = [[NSCache alloc] init];
        self.imageNames = [self listImageNames];
        
        self.filesystemObserver = [[SGDirWatchdog alloc] initWithPath:[SYTools documentsPath] update:^{
            self.imageNames = [self listImageNames];
        }];
        [self.filesystemObserver start];
    }
    return self;
}

- (NSArray <NSString *> *)listImageNames
{
    NSString *path = [SYTools documentsPath];
    NSMutableDictionary <NSString *, NSDate *> *imageDates = [NSMutableDictionary dictionary];
    
    NSArray <NSString *> *allItems = [[NSFileManager defaultManager] contentsOfDirectoryAtPath:path error:NULL];
    for (NSString *itemName in allItems)
    {
        if (![itemName.pathExtension isEqualToString:@"png"])
            continue;
        
        NSString *itemPath = [path stringByAppendingPathComponent:itemName];
        NSDictionary *attributes = [[NSFileManager defaultManager] attributesOfItemAtPath:itemPath error:NULL];
        NSDate *date = attributes[NSFileCreationDate];
        [imageDates setObject:date forKey:itemName];
    }
    
    NSArray <NSString *> *sortedImageNames = [imageDates keysSortedByValueUsingComparator:^NSComparisonResult(NSDate *obj1, NSDate *obj2) {
        return [obj2 compare:obj1];
    }];
    
    return sortedImageNames;
}

- (void)addDelegate:(id<SYGalleryManagerDelegate>)delegate
{
    [delegate gallerymanager:self didUpdateImageList:self.imageNames];
    [self.delegates addObject:[NSValue valueWithNonretainedObject:delegate]];
}

- (void)removeDelegate:(id<SYGalleryManagerDelegate>)delegate
{
    [self.delegates removeObject:[NSValue valueWithNonretainedObject:delegate]];
}

- (void)setImageNames:(NSArray<NSString *> *)imageNames
{
    NSArray <NSString *> *oldImageNames = self.imageNames;
    self->_imageNames = imageNames;

    if ([oldImageNames isEqualToArray:imageNames])
        return;
    
    NSMutableArray *newImageNames = [imageNames mutableCopy];
    [newImageNames removeObjectsInArray:oldImageNames];
    
    if (newImageNames.count == 1 && [newImageNames.firstObject isEqualToString:imageNames.firstObject])
    {
        for (NSValue *weakDelegate in self.delegates)
        {
            id <SYGalleryManagerDelegate> delegate = weakDelegate.nonretainedObjectValue;
            [delegate gallerymanager:self didAddImage:imageNames.firstObject];
        }
    }
    else
    {
        for (NSValue *weakDelegate in self.delegates)
        {
            id <SYGalleryManagerDelegate> delegate = weakDelegate.nonretainedObjectValue;
            [delegate gallerymanager:self didUpdateImageList:imageNames];
        }
    }
}

- (NSString *)pathForImageWithName:(NSString *)imageName thumbnail:(BOOL)thumbnail
{
    NSString *filename = thumbnail ? imageName : [imageName stringByAppendingPathExtension:@"thumb"];
    return [[SYTools documentsPath] stringByAppendingPathComponent:filename];
}

- (NSURL *)urlForImageWithName:(NSString *)imageName thumbnail:(BOOL)thumbnail
{
    return [NSURL fileURLWithPath:[self pathForImageWithName:imageName thumbnail:thumbnail]];
}

- (NSArray<MHGalleryItem *> *)galleryItems
{
    NSMutableArray <MHGalleryItem *> *items = [NSMutableArray arrayWithCapacity:self.imageNames.count];
    for (NSString *imageName in self.imageNames)
        [items addObject:[self galleryItemForImageWithName:imageName]];
    return [items copy];
}

- (MHGalleryItem *)galleryItemForImageWithName:(NSString *)imageName
{
    return [MHGalleryItem itemWithURL:[self urlForImageWithName:imageName thumbnail:NO].absoluteString
                         thumbnailURL:[self urlForImageWithName:imageName thumbnail:YES].absoluteString];
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

- (UIImage *)thumbnailImageForGalleryItem:(MHGalleryItem *)galleryItem
{
    NSString *imageName = [galleryItem.URLString lastPathComponent];
    return [self thumbnailImageWithName:imageName];
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
    
    NSString *imageName = [[formatter stringFromDate:[NSDate date]] stringByAppendingPathExtension:@"png"];
    
    [UIImagePNGRepresentation(image) writeToFile:[self pathForImageWithName:imageName thumbnail:NO] atomically:YES];
    [self generateThumbnailFileForImageWithName:imageName];
}

- (void)deleteImageWithName:(NSString *)imageName
{
    [[NSFileManager defaultManager] removeItemAtPath:[self pathForImageWithName:imageName thumbnail:YES] error:NULL];
    [[NSFileManager defaultManager] removeItemAtPath:[self pathForImageWithName:imageName thumbnail:NO]  error:NULL];
}

@end
