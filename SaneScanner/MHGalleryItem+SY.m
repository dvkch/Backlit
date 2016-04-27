//
//  MHGalleryItem+SY.m
//  SaneScanner
//
//  Created by Stan Chevallier on 27/04/2016.
//  Copyright © 2016 Syan. All rights reserved.
//

#import "MHGalleryItem+SY.h"
#import <objc/runtime.h>

@implementation MHGalleryItem (SY)

@dynamic imageName;
@dynamic path;
@dynamic thumbnailPath;

- (NSString *)imageName
{
    return objc_getAssociatedObject(self, @selector(imageName));
}

- (void)setImageName:(NSString *)imageName
{
    objc_setAssociatedObject(self, @selector(imageName), imageName, OBJC_ASSOCIATION_COPY_NONATOMIC);
}

- (NSString *)path
{
    NSString *path = objc_getAssociatedObject(self, @selector(path));
    if (!path && self.URL.isFileURL)
    {
        path = self.URL.path;
        objc_setAssociatedObject(self, @selector(path), path, OBJC_ASSOCIATION_COPY_NONATOMIC);
    }
    return path;
}

- (NSString *)thumbnailPath
{
    NSString *thumbnailPath = objc_getAssociatedObject(self, @selector(thumbnailPath));
    if (!thumbnailPath && self.thumbnailURL.isFileURL)
    {
        thumbnailPath = self.thumbnailURL.path;
        objc_setAssociatedObject(self, @selector(thumbnailPath), thumbnailPath, OBJC_ASSOCIATION_COPY_NONATOMIC);
    }
    return thumbnailPath;
}

@end
