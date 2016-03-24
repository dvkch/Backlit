//
//  SYGalleryThumbsCell.h
//  SaneScanner
//
//  Created by Stan Chevallier on 18/03/2016.
//  Copyright © 2016 Syan. All rights reserved.
//

#import <UIKit/UIKit.h>

@class MHGalleryItem;
@class MHPresenterImageView;

@interface SYGalleryThumbsCell : UICollectionViewCell

@property (nonatomic, strong) MHPresenterImageView *imageView;

- (void)updateWithItems:(NSArray <MHGalleryItem *> *)items
                  index:(NSUInteger)index
       parentController:(UIViewController *)parentController
           dismissBlock:(UIImageView *(^)(NSUInteger index))dismissBlock;

@end
