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
        window = ContextWindow.mainWindow(rootViewController: nil)
        super.init()
        setup()
    }

    @available(iOS 13.0, *)
    init(windowScene: UIWindowScene) {
        window = ContextWindow.mainWindow(windowScene: windowScene, rootViewController: nil)
        super.init()
        setup()
    }
    
    // MARK: Properties
    let window: ContextWindow
    private let splitViewController = SplitVC()
    
    // MARK: Methods
    private func setup() {
        window.context = self

        #if DEBUG
        window.enableSlowAnimationsOnShake = true
        #endif
        window.tintColor = .tint

        // auto manage toolbar visibility
        GalleryManager.shared.addDelegate(self)

        // update window
        window.rootViewController = splitViewController
    }
    
    // MARK: Status
    enum Status {
        case devicesList
        case deviceOpened
        case scanning
    }
    var status: Status {
        if let deviceVC = splitViewController.scanNC.viewControllers.last as? DeviceVC {
            if deviceVC.device.isScanning {
                return .scanning
            }
            return .deviceOpened
        }
        return .devicesList
    }

    // MARK: Notifications
    func stopOperations() {
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

class ContextWindow: SYWindow {
    fileprivate(set) var context: Context?
}
