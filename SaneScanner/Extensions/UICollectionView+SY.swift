//
//  UICollectionView+SY.swift
//  SaneScanner
//
//  Created by syan on 12/04/2023.
//  Copyright © 2023 Syan. All rights reserved.
//

import UIKit

extension UICollectionView {
    func reloadVisibleSectionHeaders<T>(block: (IndexPath, T) -> ()) {
        let indexPaths = self.indexPathsForVisibleSupplementaryElements(ofKind: UICollectionView.elementKindSectionHeader)
        reloadSupplementaryView(ofKind: UICollectionView.elementKindSectionHeader, at: indexPaths, block: block)
    }
    
    func reloadSupplementaryView<T>(ofKind kind: String, at indexPaths: [IndexPath], block: (IndexPath, T) -> ()) {
        let context = UICollectionViewFlowLayoutInvalidationContext()
        context.invalidateSupplementaryElements(ofKind: kind, at: indexPaths)
        collectionViewLayout.invalidateLayout(with: context)
        
        indexPathsForVisibleSupplementaryElements(ofKind: kind).forEach { indexPath in
            if let view = supplementaryView(forElementKind: kind, at: indexPath) as? T {
                block(indexPath, view)
            }
        }
    }
}
