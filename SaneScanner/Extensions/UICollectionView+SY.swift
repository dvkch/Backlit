//
//  UICollectionView+SY.swift
//  SaneScanner
//
//  Created by syan on 12/04/2023.
//  Copyright © 2023 Syan. All rights reserved.
//

import UIKit

extension UICollectionView {
    func reloadSectionHeaders<T>(for cellsIndexPaths: [IndexPath], block: (IndexPath, T) -> ()) {
        let sections = Set(cellsIndexPaths.map(\.section)).map { IndexPath(item: 0, section: $0) }
        reloadSupplementaryView(ofKind: UICollectionView.elementKindSectionHeader, at: sections, block: block)
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
