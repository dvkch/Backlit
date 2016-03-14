//
//  SYGalleryManager.h
//  SaneScanner
//
//  Created by Stan Chevallier on 14/03/2016.
//  Copyright © 2016 Syan. All rights reserved.
//

#import <UIKit/UIKit.h>
@class MHGalleryItem;

@interface SYGalleryManager : NSObject

+ (SYGalleryManager *)shared;

- (NSArray <NSString *> *)allImageNames;
- (MHGalleryItem *)galleryItemForImageWithName:(NSString *)imageName;
- (void)addImage:(UIImage *)image;
- (void)deleteImageWithName:(NSString *)imageName;

@end
