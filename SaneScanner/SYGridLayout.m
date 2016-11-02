//
//  SYGridLayout.m
//  SaneScanner
//
//  Created by Stan Chevallier on 24/04/2016.
//  Copyright © 2016 Syan. All rights reserved.
//

#import "SYGridLayout.h"

@implementation SYGridLayout

@dynamic minimumLineSpacing;
@dynamic minimumInteritemSpacing;
@dynamic itemSize;
@dynamic estimatedItemSize;

- (BOOL)shouldInvalidateLayoutForBoundsChange:(CGRect)newBounds
{
    CGRect oldBounds = self.collectionView.bounds;
    return (fabs(CGRectGetWidth(newBounds) - CGRectGetWidth(oldBounds)) > 0.5);
}

- (void)setMaxSize:(CGFloat)maxSize
{
    if (maxSize == self.maxSize)
        return;
    
    self->_maxSize = maxSize;
    [self invalidateLayout];
}

- (void)setMargin:(CGFloat)margin
{
    if (margin == self.margin)
        return;
    
    self->_margin = margin;
    [super setMinimumLineSpacing:margin];
    [super setMinimumInteritemSpacing:margin];
}

- (void)invalidateLayoutWithContext:(UICollectionViewLayoutInvalidationContext *)context
{
    [(UICollectionViewFlowLayoutInvalidationContext *)context setInvalidateFlowLayoutDelegateMetrics:YES];
    [super invalidateLayoutWithContext:context];
}

- (CGSize)itemSize
{
    CGRect bounds = UIEdgeInsetsInsetRect(self.collectionView.bounds, self.collectionView.contentInset);
    CGFloat collectionSpace = bounds.size.width;
    if (self.scrollDirection == UICollectionViewScrollDirectionHorizontal)
        collectionSpace = bounds.size.height;
    
    // using floor would mean preferring using a bigger size but bigger margins, using floor we know the best size to keep fixed margins
    NSUInteger numberOfItemsPerRow = ceil(collectionSpace / self.maxSize);
    CGFloat availableSpace = collectionSpace - (numberOfItemsPerRow - 1) * self.margin;
    
    // donnot use floor, it would mess up margins, returning float works just fine
    CGFloat cellSize = availableSpace / numberOfItemsPerRow;
    
    return CGSizeMake(cellSize, cellSize);
}

@end
