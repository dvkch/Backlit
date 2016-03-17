//
//  SYGalleryManager.h
//  SaneScanner
//
//  Created by Stan Chevallier on 14/03/2016.
//  Copyright © 2016 Syan. All rights reserved.
//

#import <UIKit/UIKit.h>
@class MHGalleryItem;
@class SYGalleryManager;

@protocol SYGalleryManagerDelegate <NSObject>
- (void)gallerymanager:(SYGalleryManager *)gallerymanager didAddImage:(NSString *)imageName;
- (void)gallerymanager:(SYGalleryManager *)gallerymanager didUpdateImageList:(NSArray <NSString *> *)imageList;
@end

@interface SYGalleryManager : NSObject

@property (nonatomic, strong, readonly) NSArray <NSString *> *imageNames;

+ (SYGalleryManager *)shared;

- (void)addDelegate:(id<SYGalleryManagerDelegate>)delegate;
- (void)removeDelegate:(id<SYGalleryManagerDelegate>)delegate;

- (NSArray <MHGalleryItem *> *)galleryItems;
- (MHGalleryItem *)galleryItemForImageWithName:(NSString *)imageName;
- (UIImage *)thumbnailImageWithName:(NSString *)imageName;
- (UIImage *)thumbnailImageForGalleryItem:(MHGalleryItem *)galleryItem;

- (void)addImage:(UIImage *)image;
- (void)deleteImageWithName:(NSString *)imageName;

@end
