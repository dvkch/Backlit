//
//  UICollectionView+SYKit.swift
//  SYKit
//
//  Created by Stanislas Chevallier on 12/12/2018.
//  Copyright © 2018 Syan. All rights reserved.
//

import UIKit

public extension UICollectionView {
    func registerCell<T: UICollectionViewCell>(_ type: T.Type, xib: Bool = true) {
        let className = NSStringFromClass(type).components(separatedBy: ".").last!
        if xib {
            register(UINib(nibName: className, bundle: nil), forCellWithReuseIdentifier: className)
        }
        else {
            register(type, forCellWithReuseIdentifier: className)
        }
    }
    
    func registerSupplementaryView<T: UICollectionReusableView>(_ type: T.Type, kind: String, xib: Bool = true) {
        let className = NSStringFromClass(type).components(separatedBy: ".").last!
        if xib {
            register(UINib(nibName: className, bundle: nil), forSupplementaryViewOfKind: kind, withReuseIdentifier: className)
        }
        else {
            register(type, forSupplementaryViewOfKind: kind, withReuseIdentifier: className)
        }
    }
    
    func dequeueCell<T: UICollectionViewCell>(_ type: T.Type, for indexPath: IndexPath) -> T {
        let className = NSStringFromClass(type).components(separatedBy: ".").last!
        return dequeueReusableCell(withReuseIdentifier: className, for: indexPath) as! T
    }
    
    func dequeueSupplementaryView<T: UICollectionReusableView>(_ type: T.Type, kind: String, for indexPath: IndexPath) -> T {
        let className = NSStringFromClass(type).components(separatedBy: ".").last!
        return dequeueReusableSupplementaryView(ofKind: kind, withReuseIdentifier: className, for: indexPath) as! T
    }
    
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
