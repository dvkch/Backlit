//
//  GalleryGridLayout.swift
//  SaneScanner
//
//  Created by Stanislas Chevallier on 07/02/2019.
//  Copyright © 2019 Syan. All rights reserved.
//

import UIKit

class GalleryGridLayout: BouncyLayout {
    
    var cellZIndex: Int = 0

    var maxSize: CGFloat = 50 {
        didSet {
            invalidateLayout()
        }
    }
    
    var margin: CGFloat = 0 {
        didSet {
            minimumInteritemSpacing = margin
            minimumLineSpacing = margin
            invalidateLayout()
        }
    }
    
    var linesHorizontalInset: CGFloat = 0 {
        didSet {
            invalidateLayout()
        }
    }
    
    override var itemSize: CGSize {
        get { return computedItemSize }
        set { fatalError("This method should not be called") }
    }
    
    var computedItemSize: CGSize {
        guard let collectionView = self.collectionView else { return .zero }
        
        // determining available size
        let bounds = collectionView.bounds
            .inset(by: collectionView.adjustedContentInset)
            .inset(by: UIEdgeInsets(leftAndRight: linesHorizontalInset))
        
        let length = scrollDirection == .horizontal ? bounds.height : bounds.width
        
        // using floor would mean preferring using a bigger size but bigger margins, using floor we know the best size to keep fixed margins
        let numberOfItemsPerRow = Int(ceil(length / maxSize))
        let availableSpace = length - CGFloat(numberOfItemsPerRow - 1) * margin
        
        // donnot use floor, it would mess up margins, returning float works just fine
        let cellSize = availableSpace / CGFloat(numberOfItemsPerRow)
        
        return CGSize(width: cellSize, height: cellSize)
    }

    override func layoutAttributesForElements(in rect: CGRect) -> [UICollectionViewLayoutAttributes]? {
        let attrs = super.layoutAttributesForElements(in: rect)
        attrs?.forEach { attr in
            if attr.representedElementCategory == .cell {
                attr.zIndex = cellZIndex
            }
        }
        return attrs
    }
}

extension GalleryGridLayout {
    override func shouldInvalidateLayout(forBoundsChange newBounds: CGRect) -> Bool {
        let prevWidth = collectionView?.bounds.width ?? 0
        let sizeChanged = abs(prevWidth - newBounds.width) > 0.5
        return super.shouldInvalidateLayout(forBoundsChange: newBounds) || sizeChanged
    }
    
    override func invalidateLayout(with context: UICollectionViewLayoutInvalidationContext) {
        (context as? UICollectionViewFlowLayoutInvalidationContext)?.invalidateFlowLayoutDelegateMetrics = true
        super.invalidateLayout(with: context)
    }
}
