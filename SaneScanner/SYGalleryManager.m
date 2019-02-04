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
#import "MHGalleryItem+SY.h"
#import <SDImageCache.h>
#import <SYOperationQueue.h>
#import <NSData+SYKit.h>
#import <WeakUniqueCollection.h>
#import <SYMetadata.h>
#import "SaneScanner-Swift.h"

static NSString * const kImageExtensionPNG  = $$("png");
static NSString * const kImageExtensionJPG  = $$("jpg");
static NSString * const kImageThumbsSuffix  = $$("thumbs.jpg");
static NSString * const kImageThumbsFolder  = $$("thumbs");
static NSString * const kImagePDFFolder     = $$("PDF");
static NSString * const kImagePDFPrefix     = $$("SaneScanner_");
static NSString * const kImageExtensionPDF  = $$("pdf");


@interface SYGalleryManager ()
@property (nonatomic, strong) NSCache <NSString *, UIImage *> *thumbnailCache;
@property (nonatomic, strong) NSCache <NSString *, NSValue *> *imageSizeCache;
@property (nonatomic, strong) WeakUniqueCollection <id <SYGalleryManagerDelegate>> *delegates;
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
        // create folder for thumbanils if needed
        NSError *error;
        
        [[NSFileManager defaultManager] createDirectoryAtPath:[self thumbnailsFolderPath]
                                  withIntermediateDirectories:YES
                                                   attributes:nil
                                                        error:&error];
        
        [[NSFileManager defaultManager] createDirectoryAtPath:[self pdfFolderPath]
                                  withIntermediateDirectories:YES
                                                   attributes:nil
                                                        error:&error];
        
        [self deleteTempPDFs];
        
        // donnot use a set as it would work only with a collection of non mutable objects. here the pointer to delegate can change
        self.delegates = [[WeakUniqueCollection alloc] init];
        
        self.thumbnailCache = [[NSCache alloc] init];
        self.imageSizeCache = [[NSCache alloc] init];
        self.imageNames = [self listImageNames];
        self.thumbsBeingCreated = [NSMutableArray array];
        self.thumbsQueue = [[SYOperationQueue alloc] init];
        [self.thumbsQueue setMaxConcurrentOperationCount:1];
        [self.thumbsQueue setMaxSurvivingOperations:0];
        [self.thumbsQueue setMode:SYOperationQueueModeLIFO];

        NSPredicate *imgPredicate = [NSPredicate predicateWithFormat:$$("pathExtension IN[cd] %@"), @[kImageExtensionPNG, kImageExtensionJPG]];
        
        self.directoryWatcher =
        [MHWDirectoryWatcher directoryWatcherAtPath:[SYTools documentsPath]
                                   startImmediately:YES
                                changesStartedBlock:nil
                                    filesAddedBlock:^(NSArray<NSString *> *addedFiles) {
                                        if ([addedFiles filteredArrayUsingPredicate:imgPredicate])
                                            self.imageNames = [self listImageNames];
                                    }
                                  filesRemovedBlock:^(NSArray<NSString *> *removedFiles) {
                                      for (NSString *file in removedFiles)
                                      {
                                          [self.thumbnailCache removeObjectForKey:file];
                                          [self.imageSizeCache removeObjectForKey:file];
                                      }
                                      
                                      if ([removedFiles filteredArrayUsingPredicate:imgPredicate])
                                          self.imageNames = [self listImageNames];
                                  }
                                  changesEndedBlock:^(MHWDirectoryChanges *changes){
                                      if ([changes.addedFiles   filteredArrayUsingPredicate:imgPredicate] ||
                                          [changes.removedFiles filteredArrayUsingPredicate:imgPredicate])
                                          self.imageNames = [self listImageNames];
                                  }];
        
        [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(receivedMemoryWarning:) name:UIApplicationDidReceiveMemoryWarningNotification object:nil];
    }
    return self;
}

- (void)receivedMemoryWarning:(NSNotification *)notification
{
    self.thumbnailCache = [[NSCache alloc] init];
    self.imageSizeCache = [[NSCache alloc] init];
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

- (NSString *)thumbnailsFolderPath
{
    return [[SYTools cachePath] stringByAppendingPathComponent:kImageThumbsFolder];
}

- (NSString *)pdfFolderPath
{
    return [[SYTools cachePath] stringByAppendingPathComponent:kImagePDFFolder];
}

- (NSString *)pathForImageWithName:(NSString *)imageName thumbnail:(BOOL)thumbnail
{
    if (thumbnail)
    {
        NSString *filename = [[imageName stringByDeletingPathExtension] stringByAppendingPathExtension:kImageThumbsSuffix];
        return [[self thumbnailsFolderPath] stringByAppendingPathComponent:filename];
    }
    
    return [[SYTools documentsPath] stringByAppendingPathComponent:imageName];
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
        if (![@[kImageExtensionPNG.uppercaseString, kImageExtensionJPG.uppercaseString] containsObject:item.pathExtension.uppercaseString])
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
    
    for (id <SYGalleryManagerDelegate> delegate in self.delegates.allObjects)
    {
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
        
        // this first method is a bit longer to generate images, but uses far less memory on the device
        UIImage *thumb = [UIImage sy_imageThumbnailForFileAtPath:item.URL.path maxEdgeSize:200];
        
        // in case the first method fails we do it the old way
        if (!thumb)
        {
            @autoreleasepool {
                UIImage *image = fullImage;
                if (!image)
                {
                    NSData *data = [NSData dataWithContentsOfFile:item.path options:(NSDataReadingMappedIfSafe) error:NULL];
                    
                    if (![data sy_imageDataIsValidPNG] && ![data sy_imageDataIsValidJPEG]) {
                        [self.thumbsBeingCreated removeObject:item.imageName];
                        return;
                    }
                    
                    image = [UIImage imageWithData:data];
                }
                
                if (image.size.width > image.size.height)
                    thumb = [image sy_imageResizedHeightTo:200];
                else
                    thumb = [image sy_imageResizedWidthTo:200];
            }
        }
        
        if (thumb)
            [UIImageJPEGRepresentation(thumb, 0.6) writeToURL:item.thumbnailURL atomically:YES];
        
        if (!thumb) {
            [self.thumbsBeingCreated removeObject:item.imageName];
            return;
        }
        
        [self.thumbnailCache setObject:thumb forKey:item.imageName];
        
        dispatch_async(dispatch_get_main_queue(), ^{
            [self.thumbsBeingCreated removeObject:item.imageName];
            
            if (!tellDelegates)
                return;
            
            for (id <SYGalleryManagerDelegate> delegate in self.delegates.allObjects)
            {
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
    
    [self.delegates addObject:delegate];
}

- (void)removeDelegate:(id<SYGalleryManagerDelegate>)delegate
{
    // keep the delegate object valid as long as we need it
    __strong id <SYGalleryManagerDelegate> sDelegate = delegate;
    
    [self.delegates removeObject:sDelegate];
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

- (void)deleteTempPDFs
{
    // remove pdf files in cache / PDF
    NSArray <NSString *> *files = [[NSFileManager defaultManager] contentsOfDirectoryAtPath:[self pdfFolderPath] error:NULL];
    
    for (NSString *file in files)
    {
        NSString *fullPath = [[self pdfFolderPath] stringByAppendingPathComponent:file];
        [[NSFileManager defaultManager] removeItemAtPath:fullPath error:NULL];
    }
}

- (NSString *)tempPDFPath
{
    NSDateFormatter *formatter = [[NSDateFormatter alloc] init];
    [formatter setDateFormat:$$("yyyy-MM-dd_HH-mm-ss")];
    [formatter setLocale:[NSLocale localeWithLocaleIdentifier:$$("en_US_POSIX")]];
    
    NSString *imageName = [NSString stringWithFormat:$$("%@%@.%@"),
                           kImagePDFPrefix,
                           [formatter stringFromDate:[NSDate date]],
                           kImageExtensionPDF];
    
    return [[self pdfFolderPath] stringByAppendingPathComponent:imageName];
}

- (MHGalleryItem *)addImage:(UIImage *)image metadata:(SYMetadata *)metadata
{
    NSDateFormatter *formatter = [[NSDateFormatter alloc] init];
    [formatter setDateFormat:$$("yyyy-MM-dd_HH-mm-ss")];
    [formatter setLocale:[NSLocale localeWithLocaleIdentifier:$$("en_US_POSIX")]];
    
    NSString *imageName = [[formatter stringFromDate:[NSDate date]] stringByAppendingPathExtension:kImageExtensionJPG];
    
    MHGalleryItem *item = [self galleryItemForImageWithName:imageName];
    
    NSData *imageData;
    if (Preferences.shared.saveAsPNG)
        imageData = UIImagePNGRepresentation(image);
    else
        imageData = UIImageJPEGRepresentation(image, 0.9);
    
    NSData *imageDataWithMetadata;
    if (metadata)
        imageDataWithMetadata = [SYMetadata dataWithImageData:imageData andMetadata:metadata];
    
    NSData *dataToWrite = imageDataWithMetadata ?: imageData;
    [dataToWrite writeToFile:item.path atomically:YES];
    [self generateThumbAsyncForItem:item fullImage:image tellDelegates:YES];
    
    return item;
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
