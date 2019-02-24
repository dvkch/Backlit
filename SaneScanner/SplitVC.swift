//
//  SplitVC.swift
//  SaneScanner
//
//  Created by Stanislas Chevallier on 04/02/2019.
//  Copyright © 2019 Syan. All rights reserved.
//

import UIKit

class SplitVC: UISplitViewController {

    // MARK: Init
    init() {
        super.init(nibName: nil, bundle: nil)
        delegate = self
    }
    
    required init?(coder aDecoder: NSCoder) {
        fatalError("init(coder:) has not been implemented")
    }

    // MARK: Properties
    private var scanNC: ScanNC? {
        return viewControllers.compactMap({ $0 as? ScanNC }).first
    }
    
    private var galleryNC: GalleryNC? {
        return viewControllers.compactMap({ $0 as? GalleryNC }).first
    }
    
    // MARK: Layout
    override func willTransition(to newCollection: UITraitCollection, with coordinator: UIViewControllerTransitionCoordinator) {
        super.willTransition(to: newCollection, with: coordinator)
        coordinator.animate(alongsideTransition: { (_) in
            self.adaptPresentedVCs()
        }, completion: nil)
    }

    // MARK: Adaptative changes
    private func adaptPresentedVCs() {
        viewControllers.forEach {
            ($0 as? ScanNC)?.updateToolbar(animated: false)
        }
        
        if traitCollection.horizontalSizeClass != .compact {
            if let presentedGallery = presentedViewController as? GalleryNC {
                if let currentIndex = presentedGallery.currentIndex {
                    galleryNC?.openImage(at: currentIndex, animated: false)
                }
                else if presentedGallery.isShowingGrid {
                    galleryNC?.openGallery(animated: false)
                }
            }
            dismiss(animated: false, completion: nil)
        }
    }
}

extension SplitVC : UISplitViewControllerDelegate {
    func splitViewController(_ splitViewController: UISplitViewController, collapseSecondary secondaryViewController: UIViewController, onto primaryViewController: UIViewController) -> Bool {
        return splitViewController.traitCollection.horizontalSizeClass == .compact
    }
    
    func splitViewController(_ splitViewController: UISplitViewController, separateSecondaryFrom primaryViewController: UIViewController) -> UIViewController? {
        return galleryNC
    }
}
