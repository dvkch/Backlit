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
- (void)gallerymanager:(SYGalleryManager *)gallerymanager
 didUpdateGalleryItems:(NSArray <MHGalleryItem *> *)items
               newItem:(MHGalleryItem *)newItem
           removedItem:(MHGalleryItem *)removedItem;
@end

@interface SYGalleryManager : NSObject

+ (SYGalleryManager *)shared;

- (void)addDelegate:(id<SYGalleryManagerDelegate>)delegate;
- (void)removeDelegate:(id<SYGalleryManagerDelegate>)delegate;

- (NSArray <MHGalleryItem *> *)galleryItems;

- (UIImage *)thumbnailForItem:(MHGalleryItem *)item;
- (NSString *)dateStringForItem:(MHGalleryItem *)item;

- (void)addImage:(UIImage *)image;
- (void)deleteItem:(MHGalleryItem *)item;

@end
