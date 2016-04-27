//
//  MHGalleryItem+SY.h
//  SaneScanner
//
//  Created by Stan Chevallier on 27/04/2016.
//  Copyright © 2016 Syan. All rights reserved.
//

#import <MHGalleryItem.h>

@interface MHGalleryItem (SY)

@property (nonatomic, copy) NSString *imageName;
@property (nonatomic, readonly) NSString *path;
@property (nonatomic, readonly) NSString *thumbnailPath;

@end
