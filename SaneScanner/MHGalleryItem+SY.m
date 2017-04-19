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
    return (self.URL.isFileURL ? self.URL.path : nil);
}

- (NSString *)thumbnailPath
{
    return (self.thumbnailURL.isFileURL ? self.thumbnailURL.path : nil);
}

@end
