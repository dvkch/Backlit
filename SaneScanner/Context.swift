//
//  Context.swift
//  SaneScanner
//
//  Created by Stanislas Chevallier on 04/02/2021.
//  Copyright © 2021 Syan. All rights reserved.
//

import UIKit
import SYKit

class Context: NSObject {
    override init() {
        window = SYWindow.mainWindow(rootViewController: nil)
        super.init()
        setup()
    }

    @available(iOS 13.0, *)
    init(windowScene: UIWindowScene) {
        window = SYWindow.mainWindow(windowScene: windowScene, rootViewController: nil)
        super.init()
        setup()
    }
    
    // MARK: Properties
    let window: SYWindow
    private var splitViewController: SplitVC!
    
    // MARK: Methods
    private func setup() {
        guard splitViewController == nil else { return }
        
        NotificationCenter.default.addObserver(self, selector: #selector(didEnterBackgroundNotification), name: UIApplication.didEnterBackgroundNotification, object: nil)
        
        #if DEBUG
        window.enableSlowAnimationsOnShake = true
        #endif
        window.tintColor = .tint

        // split controller
        splitViewController = SplitVC()

        // auto manage toolbar visibility
        GalleryManager.shared.addDelegate(self)

        // update window
        window.rootViewController = splitViewController
    }
    
    // MARK: Notifications
    @objc private func didEnterBackgroundNotification() {
        splitViewController.scanNC.popToRootViewController(animated: true)
    }
}

extension Context : GalleryManagerDelegate {
    func galleryManager(_ manager: GalleryManager, didCreate thumbnail: UIImage, for item: GalleryItem) { }
    func galleryManager(_ manager: GalleryManager, didUpdate items: [GalleryItem], newItems: [GalleryItem], removedItems: [GalleryItem]) {
        let constrainedW = splitViewController.traitCollection.horizontalSizeClass == .compact
        
        splitViewController.scanNC.setToolbarHidden(!constrainedW || items.isEmpty, animated: true)
    }
}

