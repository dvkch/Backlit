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
@property (nonatomic, strong) NSArray <NSString *> *imageNames;
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

#pragma mark - Private

#pragma mark Conversions

- (MHGalleryItem *)galleryItemForImageWithName:(NSString *)imageName
{
    MHGalleryItem *item = [MHGalleryItem itemWithURL:[self urlForImageWithName:imageName thumbnail:NO].absoluteString
                                        thumbnailURL:[self urlForImageWithName:imageName thumbnail:YES].absoluteString];
    
    NSDateFormatter *dateFormatter = [[NSDateFormatter alloc] init];
    [dateFormatter setDateStyle:NSDateFormatterMediumStyle];
    [dateFormatter setTimeStyle:NSDateFormatterShortStyle];
    
    item.titleString = [dateFormatter stringFromDate:[NSDate date]];

    return item;
}

- (NSString *)imageNameForGalleryItem:(MHGalleryItem *)item
{
    return [item.URLString lastPathComponent];
}

#pragma mark Paths

- (NSString *)pathForImageWithName:(NSString *)imageName thumbnail:(BOOL)thumbnail
{
    NSString *filename = imageName;
    if (thumbnail)
        filename = [[filename stringByDeletingPathExtension] stringByAppendingPathExtension:@"thumb"];
    
    return [[SYTools documentsPath] stringByAppendingPathComponent:filename];
}

- (NSURL *)urlForImageWithName:(NSString *)imageName thumbnail:(BOOL)thumbnail
{
    return [NSURL fileURLWithPath:[self pathForImageWithName:imageName thumbnail:thumbnail]];
}

#pragma mark Listing

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

- (void)setImageNames:(NSArray<NSString *> *)imageNames
{
    NSArray <NSString *> *oldImageNames = self.imageNames;
    self->_imageNames = imageNames;
    
    if ([oldImageNames isEqualToArray:imageNames])
        return;
    
    [self checkThumbsExist];
    
    NSString *addedImage;
    {
        NSMutableArray *newImageNames = [imageNames mutableCopy];
        [newImageNames removeObjectsInArray:oldImageNames];
        
        if (newImageNames.count == 1 && [newImageNames.firstObject isEqualToString:imageNames.firstObject])
            addedImage = imageNames.firstObject;
    }
    
    for (NSValue *weakDelegate in self.delegates)
    {
        id <SYGalleryManagerDelegate> delegate = weakDelegate.nonretainedObjectValue;
        
        [delegate gallerymanager:self
           didUpdateGalleryItems:self.galleryItems
                         newItem:[self galleryItemForImageWithName:addedImage]
                     removedItem:nil];
    }
}

#pragma mark Thumbs

- (UIImage *)generateThumbnailFileForImageWithName:(NSString *)imageName withImage:(UIImage *)image
{
    NSString *thumbPath = [self pathForImageWithName:imageName thumbnail:YES];
    NSString *fullPath  = [self pathForImageWithName:imageName thumbnail:NO];
    
    if ([[NSFileManager defaultManager] fileExistsAtPath:thumbPath])
        return nil;
    
    UIImage *fullImage = (image ?: [UIImage imageWithContentsOfFile:fullPath]);
    
    UIImage *thumb;
    
    if (image.size.width > image.size.height)
        thumb = [fullImage sy_imageResizedHeightTo:300];
    else
        thumb = [fullImage sy_imageResizedWidthTo:300];
    
    [UIImageJPEGRepresentation(thumb, 0.7) writeToFile:thumbPath atomically:YES];
    
    return thumb;
}

- (void)checkThumbsExist
{
    NSArray <NSString *> *imageNames = [self imageNames];
    for (NSString *imageName in imageNames)
        [self generateThumbnailFileForImageWithName:imageName withImage:nil];
}

#pragma mark - Public

- (void)addDelegate:(id<SYGalleryManagerDelegate>)delegate
{
    [delegate gallerymanager:self didUpdateGalleryItems:self.galleryItems newItem:nil removedItem:nil];
    [self.delegates addObject:[NSValue valueWithNonretainedObject:delegate]];
}

- (void)removeDelegate:(id<SYGalleryManagerDelegate>)delegate
{
    [self.delegates removeObject:[NSValue valueWithNonretainedObject:delegate]];
}

- (NSArray<MHGalleryItem *> *)galleryItems
{
    NSMutableArray <MHGalleryItem *> *items = [NSMutableArray arrayWithCapacity:self.imageNames.count];
    for (NSString *imageName in self.imageNames)
        [items addObject:[self galleryItemForImageWithName:imageName]];
    return [items copy];
}

- (UIImage *)thumbnailForItem:(MHGalleryItem *)item
{
    NSString *imageName = [self imageNameForGalleryItem:item];
    UIImage *thumb = [self.thumbanilCache objectForKey:imageName];
    if (thumb)
        return thumb;
    
    thumb = [UIImage imageWithContentsOfFile:[self pathForImageWithName:imageName thumbnail:YES]];
    if (!thumb)
        thumb = [self generateThumbnailFileForImageWithName:imageName withImage:nil];
    
    [self.thumbanilCache setObject:thumb forKey:imageName];
    return thumb;
}

- (void)addImage:(UIImage *)image
{
    NSDateFormatter *formatter = [[NSDateFormatter alloc] init];
    [formatter setDateFormat:@"yyyy-MM-dd_HH-mm-ss"];
    [formatter setLocale:[NSLocale localeWithLocaleIdentifier:@"en_US_POSIX"]];
    
    NSString *imageName = [[formatter stringFromDate:[NSDate date]] stringByAppendingPathExtension:@"png"];
    
    [UIImagePNGRepresentation(image) writeToFile:[self pathForImageWithName:imageName thumbnail:NO] atomically:YES];
    [self generateThumbnailFileForImageWithName:imageName withImage:image];
}

- (void)deleteItem:(MHGalleryItem *)item
{
    NSString *imageName = [self imageNameForGalleryItem:item];
    NSString *thumbPath = [self pathForImageWithName:imageName thumbnail:YES];
    NSString *fullPath  = [self pathForImageWithName:imageName thumbnail:NO];
    [[NSFileManager defaultManager] removeItemAtPath:thumbPath error:NULL];
    [[NSFileManager defaultManager] removeItemAtPath:fullPath  error:NULL];
}

@end