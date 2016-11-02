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
#import "UIColor+SY.h"
#import "MHWDirectoryWatcher.h"
#import "UIImage+SY.h"
#import "MHGalleryItem+SY.h"
#import <SDImageCache.h>
#import <SYOperationQueue.h>
#import <NSData+SYKit.h>

@interface SYGalleryManagerWeakDelegate : NSObject
@property (atomic, weak) id<SYGalleryManagerDelegate> delegate;
+ (instancetype)weakDelegateWithDelegate:(id<SYGalleryManagerDelegate>)delegate;
+ (NSPredicate *)nonEmptyPredicate;
@end

@implementation SYGalleryManagerWeakDelegate
+ (instancetype)weakDelegateWithDelegate:(id<SYGalleryManagerDelegate>)delegate
{
    SYGalleryManagerWeakDelegate *weakDelegate = [[self alloc] init];
    [weakDelegate setDelegate:delegate];
    return weakDelegate;
}
- (NSUInteger)hash
{
    return [self.delegate hash];
}
- (BOOL)isEqual:(id)object
{
    if (![object isKindOfClass:[SYGalleryManagerWeakDelegate class]])
        return NO;
    
    if (!self.delegate && ![object delegate])
        return YES;
    
    return [self.delegate isEqual:[object delegate]];
}
-(NSString *)description
{
    return [NSString stringWithFormat:@"<%@: %p, delegate: %@>", self.class, self, self.delegate];
}
+ (NSPredicate *)nonEmptyPredicate
{
    return [NSPredicate predicateWithFormat:@"SELF.delegate != nil"];
}
@end

@interface SYGalleryManager ()
@property (nonatomic, strong) NSCache <NSString *, UIImage *> *thumbnailCache;
@property (nonatomic, strong) NSCache <NSString *, NSValue *> *imageSizeCache;
@property (nonatomic, strong) NSMutableArray <SYGalleryManagerWeakDelegate *> *delegates;
@property (nonatomic, strong) MHWDirectoryWatcher *directoryWatcher;
@property (nonatomic, strong) NSArray <NSString *> *imageNames;
@property (nonatomic, strong) NSMutableArray <NSString *> *thumbsBeingCreated;
@property (nonatomic, strong) SYOperationQueue *thumbsQueue;
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
        // donnot use a set as it would work only with a collection of non mutable objects. here the pointer to delegate can change
        self.delegates = [NSMutableArray array];
        
        self.thumbnailCache = [[NSCache alloc] init];
        self.imageSizeCache = [[NSCache alloc] init];
        self.imageNames = [self listImageNames];
        self.thumbsBeingCreated = [NSMutableArray array];
        self.thumbsQueue = [[SYOperationQueue alloc] init];
        [self.thumbsQueue setMaxConcurrentOperationCount:1];
        [self.thumbsQueue setMaxSurvivingOperations:0];
        [self.thumbsQueue setMode:SYOperationQueueModeLIFO];

        NSPredicate *pngPredicate = [NSPredicate predicateWithFormat:@"pathExtension IN %@", @[@"png"]];
        
        self.directoryWatcher =
        [MHWDirectoryWatcher directoryWatcherAtPath:[SYTools documentsPath]
                                   startImmediately:YES
                                changesStartedBlock:nil
                                    filesAddedBlock:^(NSArray<NSString *> *addedFiles) {
                                        if ([addedFiles filteredArrayUsingPredicate:pngPredicate])
                                            self.imageNames = [self listImageNames];
                                    }
                                  filesRemovedBlock:^(NSArray<NSString *> *removedFiles) {
                                      for (NSString *file in removedFiles)
                                      {
                                          [self.thumbnailCache removeObjectForKey:file];
                                          [self.imageSizeCache removeObjectForKey:file];
                                      }
                                      
                                      if ([removedFiles filteredArrayUsingPredicate:pngPredicate])
                                          self.imageNames = [self listImageNames];
                                  }
                                  changesEndedBlock:^(MHWDirectoryChanges *changes){
                                      if ([changes.addedFiles   filteredArrayUsingPredicate:pngPredicate] ||
                                          [changes.removedFiles filteredArrayUsingPredicate:pngPredicate])
                                          self.imageNames = [self listImageNames];
                                  }];
        
        [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(receivedMemoryWarning:) name:UIApplicationDidReceiveMemoryWarningNotification object:nil];
    }
    return self;
}

- (void)receivedMemoryWarning:(NSNotification *)notification
{
    [self.thumbnailCache removeAllObjects];
}

#pragma mark - Private

#pragma mark Conversions

- (MHGalleryItem *)galleryItemForImageWithName:(NSString *)imageName
{
    if (!imageName)
        return nil;
    
    NSString *imagePath = [self pathForImageWithName:imageName thumbnail:NO];
    NSString *thumbPath = [self pathForImageWithName:imageName thumbnail:YES];
    
    MHGalleryItem *item = [MHGalleryItem itemWithURL:[NSURL fileURLWithPath:imagePath]
                                        thumbnailURL:[NSURL fileURLWithPath:thumbPath]];
    [item setImageName:imageName];
    
    return item;
}

#pragma mark Paths

- (NSString *)pathForImageWithName:(NSString *)imageName thumbnail:(BOOL)thumbnail
{
    NSString *filename = imageName;
    if (thumbnail)
        filename = [[filename stringByDeletingPathExtension] stringByAppendingPathExtension:@"thumb.jpg"];
    
    return [[SYTools documentsPath] stringByAppendingPathComponent:filename];
}

#pragma mark Listing

- (NSArray <NSString *> *)listImageNames
{
    NSMutableDictionary <NSString *, NSDate *> *imageNames = [NSMutableDictionary dictionary];
    
    NSArray<NSURL *> *items =
    [[NSFileManager defaultManager] contentsOfDirectoryAtURL:[NSURL fileURLWithPath:[SYTools documentsPath]]
                                  includingPropertiesForKeys:@[NSURLIsDirectoryKey, NSURLCreationDateKey]
                                                     options:NSDirectoryEnumerationSkipsSubdirectoryDescendants
                                                       error:NULL];
    
    NSNumber *isDir = nil;
    NSDate *dateCreated = nil;
    
    for (NSURL *item in items)
    {
        if (![item.pathExtension isEqualToString:@"png"])
            continue;
        
        if (![item getResourceValue:&isDir forKey:NSURLIsDirectoryKey error:NULL])
            continue;
        
        if (![item getResourceValue:&dateCreated forKey:NSURLCreationDateKey error:NULL])
            continue;
        
        if (isDir.boolValue || !dateCreated)
            continue;
        
        [imageNames setObject:dateCreated forKey:item.path.lastPathComponent];
    }
    
    NSArray <NSString *> *sortedImageNames = [imageNames keysSortedByValueUsingComparator:^NSComparisonResult(NSDate *obj1, NSDate *obj2) {
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
    
    if (!imageNames.count) {
        [self.thumbnailCache removeAllObjects];
        [self.imageSizeCache removeAllObjects];
    }
    
    NSString *addedImage;
    {
        NSMutableArray *newImageNames = [imageNames mutableCopy];
        [newImageNames removeObjectsInArray:oldImageNames];
        addedImage = newImageNames.firstObject;
    }
    
    NSString *removedImage;
    {
        NSMutableArray *newImageNames = [oldImageNames mutableCopy];
        [newImageNames removeObjectsInArray:imageNames];
        removedImage = newImageNames.firstObject;
    }
    
    for (SYGalleryManagerWeakDelegate *weakDelegate in self.delegates)
    {
        id <SYGalleryManagerDelegate> delegate = weakDelegate.delegate;
        if ([delegate respondsToSelector:@selector(gallerymanager:didUpdateGalleryItems:newItem:removedItem:)])
            [delegate gallerymanager:self
               didUpdateGalleryItems:self.galleryItems
                             newItem:[self galleryItemForImageWithName:addedImage]
                         removedItem:[self galleryItemForImageWithName:removedImage]];
    }
    
    //[self checkThumbs];
}

#pragma mark Thumbs

- (void)checkThumbs
{
    NSArray <NSString *> *imageNames = [self listImageNames];
    dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_BACKGROUND, 0), ^{
        for (NSString *imageName in imageNames)
            [self generateThumbAsyncForItem:[self galleryItemForImageWithName:imageName] fullImage:nil tellDelegates:YES];
    });
}

- (void)generateThumbAsyncForItem:(MHGalleryItem *)item fullImage:(UIImage *)fullImage tellDelegates:(BOOL)tellDelegates
{
    if ([self.thumbsBeingCreated containsObject:item.imageName])
        return;
    
    [self.thumbsBeingCreated addObject:item.imageName];
    
    if ([self.directoryWatcher.filesBeingWrittenTo containsObject:item.imageName] ||
        [[NSFileManager defaultManager] fileExistsAtPath:item.thumbnailPath] ||
        ![[NSFileManager defaultManager] fileExistsAtPath:item.path])
    {
        [self.thumbsBeingCreated removeObject:item.imageName];
        return;
    }
    
    [self.thumbsQueue addOperationWithBlock:^{
        UIImage *thumb;
        
        @autoreleasepool {
            UIImage *image = fullImage;
            if (!image)
            {
                NSData *data = [NSData dataWithContentsOfFile:item.path options:(NSDataReadingMappedIfSafe) error:NULL];
                
                if (![data sy_imageDataIsValidPNG]) {
                    [self.thumbsBeingCreated removeObject:item.imageName];
                    return;
                }
                
                image = [UIImage imageWithData:data];
            }
            
            if (image.size.width > image.size.height)
                thumb = [image sy_imageResizedHeightTo:200];
            else
                thumb = [image sy_imageResizedWidthTo:200];
            
            [UIImageJPEGRepresentation(thumb, 0.6) writeToURL:item.thumbnailURL atomically:YES];
        }
        
        if (!thumb) {
            [self.thumbsBeingCreated removeObject:item.imageName];
            return;
        }
        
        [self.thumbnailCache setObject:thumb forKey:item.imageName];
        
        dispatch_async(dispatch_get_main_queue(), ^{
            [self.thumbsBeingCreated removeObject:item.imageName];
            
            if (!tellDelegates)
                return;
            
            for (SYGalleryManagerWeakDelegate *weakDelegate in self.delegates)
            {
                id <SYGalleryManagerDelegate> delegate = weakDelegate.delegate;
                if ([delegate respondsToSelector:@selector(gallerymanager:didCreatedThumb:forItem:)])
                    [delegate gallerymanager:self didCreatedThumb:thumb forItem:item];
            }
        });
    }];
}

#pragma mark - Public

- (void)addDelegate:(id<SYGalleryManagerDelegate>)delegate
{
    if ([delegate respondsToSelector:@selector(gallerymanager:didUpdateGalleryItems:newItem:removedItem:)])
        [delegate gallerymanager:self didUpdateGalleryItems:self.galleryItems newItem:nil removedItem:nil];
    
    [self.delegates addObject:[SYGalleryManagerWeakDelegate weakDelegateWithDelegate:delegate]];
    [self cleanUpDelegates];
}

- (void)removeDelegate:(id<SYGalleryManagerDelegate>)delegate
{
    [self.delegates removeObject:[SYGalleryManagerWeakDelegate weakDelegateWithDelegate:delegate]];
    [self cleanUpDelegates];
}

- (void)cleanUpDelegates
{
    [self.delegates removeObject:[SYGalleryManagerWeakDelegate weakDelegateWithDelegate:nil]];
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
    UIImage *thumb = [self.thumbnailCache objectForKey:item.imageName];
    if (thumb)
        return thumb;
    
    thumb = [UIImage imageWithContentsOfFile:item.thumbnailPath];
    if (thumb) {
        [self.thumbnailCache setObject:thumb forKey:item.imageName];
        return thumb;
    }
    
    [self generateThumbAsyncForItem:item fullImage:nil tellDelegates:YES];
    return nil;
}

- (NSString *)dateStringForItem:(MHGalleryItem *)item
{
    NSDictionary *attributes = [[NSFileManager defaultManager] attributesOfItemAtPath:item.path error:NULL];
    NSDate *date = attributes[NSFileCreationDate];
    
    if (!date)
        return nil;
    
    NSDateFormatter *dateFormatter = [[NSDateFormatter alloc] init];
    [dateFormatter setDateStyle:NSDateFormatterLongStyle];
    [dateFormatter setTimeStyle:NSDateFormatterShortStyle];
    return [dateFormatter stringFromDate:date];
}

- (void)addImage:(UIImage *)image
{
    NSDateFormatter *formatter = [[NSDateFormatter alloc] init];
    [formatter setDateFormat:@"yyyy-MM-dd_HH-mm-ss"];
    [formatter setLocale:[NSLocale localeWithLocaleIdentifier:@"en_US_POSIX"]];
    
    NSString *imageName = [[formatter stringFromDate:[NSDate date]] stringByAppendingPathExtension:@"png"];
    
    MHGalleryItem *item = [self galleryItemForImageWithName:imageName];
    
    [UIImagePNGRepresentation(image) writeToFile:item.path atomically:YES];
    [self generateThumbAsyncForItem:item fullImage:image tellDelegates:YES];
}

- (void)deleteItem:(MHGalleryItem *)item
{
    [[NSFileManager defaultManager] removeItemAtPath:item.path          error:NULL];
    [[NSFileManager defaultManager] removeItemAtPath:item.thumbnailPath error:NULL];
}

- (CGSize)sizeOfItem:(MHGalleryItem *)item
{
    NSValue *sizeV = [self.imageSizeCache objectForKey:item.imageName];
    if (sizeV)
        return [sizeV CGSizeValue];
    
    CGSize imageSize = [UIImage sy_sizeOfImageAtURL:item.URL];
    if (!CGSizeEqualToSize(CGSizeZero, imageSize))
        [self.imageSizeCache setObject:[NSValue valueWithCGSize:imageSize] forKey:item.imageName];
    
    return imageSize;
}

@end
