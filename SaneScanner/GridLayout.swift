//
//  GridLayout.swift
//  SaneScanner
//
//  Created by Stanislas Chevallier on 07/02/2019.
//  Copyright © 2019 Syan. All rights reserved.
//

import UIKit

class GridLayout: UICollectionViewFlowLayout {

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
    
    override var itemSize: CGSize {
        get { return computedItemSize }
        set { fatalError("This method should not be called") }
    }
    
    var computedItemSize: CGSize {
        guard let collectionView = self.collectionView else { return .zero }
        
        // determining available size
        var bounds = collectionView.bounds.inset(by: collectionView.contentInset)
        
        if #available(iOS 11.0, *) {
            bounds = bounds.inset(by: collectionView.safeAreaInsets)
        }
        
        let length = scrollDirection == .horizontal ? bounds.height : bounds.width
        
        // using floor would mean preferring using a bigger size but bigger margins, using floor we know the best size to keep fixed margins
        let numberOfItemsPerRow = Int(ceil(length / maxSize))
        let availableSpace = length - CGFloat(numberOfItemsPerRow - 1) * margin
        
        // donnot use floor, it would mess up margins, returning float works just fine
        let cellSize = availableSpace / CGFloat(numberOfItemsPerRow)
        
        return CGSize(width: cellSize, height: cellSize)
    }
}

extension GridLayout {
    override func shouldInvalidateLayout(forBoundsChange newBounds: CGRect) -> Bool {
        let prevWidth = collectionView?.bounds.width ?? 0
        return abs(prevWidth - newBounds.width) > 0.5
    }
    
    override func invalidateLayout(with context: UICollectionViewLayoutInvalidationContext) {
        (context as? UICollectionViewFlowLayoutInvalidationContext)?.invalidateFlowLayoutDelegateMetrics = true
        super.invalidateLayout(with: context)
    }
}
