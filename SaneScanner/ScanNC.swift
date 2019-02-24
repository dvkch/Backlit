//
//  ScanNC.swift
//  SaneScanner
//
//  Created by Stanislas Chevallier on 04/02/2019.
//  Copyright © 2019 Syan. All rights reserved.
//

import UIKit

class ScanNC: UINavigationController {
    
    // MARK: Init
    convenience init() {
        self.init(navigationBarClass: nil, toolbarClass: Toolbar.self)
    }
    
    override init(navigationBarClass: AnyClass?, toolbarClass: AnyClass?) {
        super.init(navigationBarClass: navigationBarClass, toolbarClass: Toolbar.self)
        updateToolbar(animated: false)
    }
    
    override init(nibName nibNameOrNil: String?, bundle nibBundleOrNil: Bundle?) {
        super.init(nibName: nibNameOrNil, bundle: nibBundleOrNil)
    }
    
    required init?(coder aDecoder: NSCoder) {
        fatalError("init(coder:) has not been implemented")
    }
    
    // MARK: Properties
    private var customToolbar: Toolbar? {
        return self.toolbar as? Toolbar
    }
    
    // MARK: Content
    func updateToolbar(animated: Bool) {
        customToolbar?.isTranslucent = false
        customToolbar?.height = traitCollection.verticalSizeClass == .compact ? 34 : 64
        customToolbar?.padding = 0
        
        let toolbarHidden = splitViewController?.traitCollection.horizontalSizeClass != .compact || GalleryManager.shared.items.isEmpty
        setToolbarHidden(toolbarHidden, animated: animated)
    }
    
    // MARK: Layout
    override func traitCollectionDidChange(_ previousTraitCollection: UITraitCollection?) {
        super.traitCollectionDidChange(previousTraitCollection)
        updateToolbar(animated: false)
    }
}

extension ScanNC: GalleryManagerDelegate {
    func galleryManager(_ manager: GalleryManager, didCreate thumbnail: UIImage, for item: GalleryItem) { }
    func galleryManager(_ manager: GalleryManager, didUpdate items: [GalleryItem], newItems: [GalleryItem], removedItems: [GalleryItem]) {
        updateToolbar(animated: true)
    }
}
