//
//  NKDiffable+SY.swift
//  Backlit
//
//  Created by syan on 16/04/2023.
//  Copyright © 2023 Syan. All rights reserved.
//

import UIKit

protocol CollectionViewDiffableDataSourceViewsProvider<SectionIdentifier, ItemIdentifier>: NSObjectProtocol {
    associatedtype SectionIdentifier
    associatedtype ItemIdentifier
    
    func collectionView(_ collectionView: UICollectionView, cellForItem item: ItemIdentifier, at indexPath: IndexPath) -> UICollectionViewCell
    
    func collectionView(_ collectionView: UICollectionView, viewForSupplementaryElementOfKind kind: String, at indexPath: IndexPath) -> UICollectionReusableView
}

extension UICollectionViewDiffableDataSource {
    
    // MARK: Init
    convenience init(
        collectionView: UICollectionView,
        viewsProvider: any CollectionViewDiffableDataSourceViewsProvider<SectionIdentifierType, ItemIdentifierType>
    ) {
        weak var provider = viewsProvider
        self.init(collectionView: collectionView) { collectionView, indexPath, item in
            provider?.collectionView(collectionView, cellForItem: item, at: indexPath)
        }
        self.supplementaryViewProvider = { collectionView, kind, indexPath in
            provider?.collectionView(collectionView, viewForSupplementaryElementOfKind: kind, at: indexPath)
        }
    }

    // MARK: Computed properties
    var totalCount: Int {
        return snapshot().numberOfItems
    }
    
    var lastIndexPath: IndexPath? {
        let snapshot = self.snapshot()
        let sections = Array(snapshot.sectionIdentifiers.enumerated())
        guard let section = sections.last(where: { snapshot.numberOfItems(inSection: $0.element) > 0 }) else { return nil }
        return IndexPath(item: snapshot.numberOfItems(inSection: section.element) - 1, section: section.offset)
    }
}
