//
//  SplitVC.swift
//  SaneScanner
//
//  Created by Stanislas Chevallier on 04/02/2019.
//  Copyright © 2019 Syan. All rights reserved.
//

import UIKit

class SplitVC: UISplitViewController {

    init() {
        super.init(nibName: nil, bundle: nil)
        delegate = self
    }
    
    required init?(coder aDecoder: NSCoder) {
        fatalError("init(coder:) has not been implemented")
    }
    
    override func willTransition(to newCollection: UITraitCollection, with coordinator: UIViewControllerTransitionCoordinator) {
        super.willTransition(to: newCollection, with: coordinator)
        coordinator.animate(alongsideTransition: { (_) in
            self.viewControllers.forEach {
                ($0 as? ScanNC)?.updateToolbar(animated: false)
            }
            
            if self.traitCollection.horizontalSizeClass != .compact {
                self.dismiss(animated: false, completion: nil)
            }
        }, completion: nil)
    }

    // TODO: cleanup
    func oldCode(to traitCollection: UITraitCollection) {
        let constrainedW = traitCollection.horizontalSizeClass == .compact
        let constrainedH = traitCollection.verticalSizeClass   == .compact
        
        if !constrainedW {
            /*
             if let currentGallery = scanNavigationController.presentedViewController as? GalleryViewController {
             if currentGallery.isShowingOverview() {
             currentGallery.openOverview()
             }
             else {
             galleryViewController.openImageView(forPage: currentGallery.imageViewerViewController.pageIndex)
             }
             }
             */
        }
    }
}

extension SplitVC : UISplitViewControllerDelegate {
    func splitViewController(_ splitViewController: UISplitViewController, collapseSecondary secondaryViewController: UIViewController, onto primaryViewController: UIViewController) -> Bool {
        return splitViewController.traitCollection.horizontalSizeClass == .compact
    }
    
    func splitViewController(_ splitViewController: UISplitViewController, separateSecondaryFrom primaryViewController: UIViewController) -> UIViewController? {
        return viewControllers.first(where: { $0 is GalleryNC })
    }
    
    func splitViewController(_ svc: UISplitViewController, willChangeTo displayMode: UISplitViewController.DisplayMode) {
        // TODO: cleanup
        // galleryViewController.overViewViewController.viewWillAppear(false)
    }
}
