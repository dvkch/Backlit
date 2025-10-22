//
//  ScanNC.swift
//  Backlit
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
    }
    
    override init(nibName nibNameOrNil: String?, bundle nibBundleOrNil: Bundle?) {
        super.init(nibName: nibNameOrNil, bundle: nibBundleOrNil)
    }
    
    required init?(coder aDecoder: NSCoder) {
        fatalError("init(coder:) has not been implemented")
    }
    
    override func viewDidLoad() {
        super.viewDidLoad()
        navigationBar.prefersLargeTitles = true
        navigationBar.isTranslucent = true
        navigationBar.scrollEdgeAppearance = UINavigationBarAppearance()
        navigationBar.scrollEdgeAppearance?.configureWithDefaultBackground()
        updateToolbar(animated: false)
   }
    
    // MARK: Properties
    private var customToolbar: Toolbar? {
        return self.toolbar as? Toolbar
    }
    
    // MARK: Content
    func updateToolbar(animated: Bool) {
        customToolbar?.isTranslucent = false
        customToolbar?.height = traitCollection.verticalSizeClass == .compact ? 50 : 75
        customToolbar?.padding = 0
        
        // fix for iOS 14+ (maybe 13 too?) not showing the toolbar. for realsies, the simple fact of accessing
        // the `toolbar` property makes it disappear completely from the view stack. tested on iOS 14.4
        toolbar.isHidden = true
        toolbar.isHidden = false

        let largeLayout = splitViewController?.traitCollection.horizontalSizeClass != .compact
        let showingScanVC = viewControllers.contains { $0 is DeviceVC }
        let noImages = GalleryManager.shared.galleryItems.isEmpty
        let toolbarHidden = largeLayout || noImages
        setToolbarHidden(toolbarHidden, animated: animated)
        
        let appearance = UIToolbarAppearance()
        appearance.backgroundColor = showingScanVC ? .tint : .cellBackground
        toolbar.standardAppearance = appearance
        toolbar.compactAppearance = appearance
        toolbar.scrollEdgeAppearance = appearance
        toolbar.compactScrollEdgeAppearance = appearance
    }
    
    // MARK: Layout
    override func traitCollectionDidChange(_ previousTraitCollection: UITraitCollection?) {
        super.traitCollectionDidChange(previousTraitCollection)
        updateToolbar(animated: false)
    }
}

extension ScanNC: GalleryManagerDelegate {
    func galleryManager(_ manager: GalleryManager, didUpdate items: [GalleryItem], newItems: [GalleryItem], removedItems: [GalleryItem]) {
        updateToolbar(animated: true)
    }
}

extension ScanNC: UINavigationBarDelegate {
    func navigationBar(_ navigationBar: UINavigationBar, shouldPop item: UINavigationItem) -> Bool {
        guard let dismissibleVC = topViewController as? ConditionallyDismissible, !dismissibleVC.isDismissible else {
            return true
        }
        dismissibleVC.showDismissalConfirmation {
            self.popViewController(animated: true)
        }
        return false
    }
}
