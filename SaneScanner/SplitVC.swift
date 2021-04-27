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
    
    private var previewNC: PreviewNC? {
        return viewControllers.compactMap({ $0 as? PreviewNC }).first
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
    }
}

extension SplitVC : UISplitViewControllerDelegate {
    func splitViewController(_ splitViewController: UISplitViewController, collapseSecondary secondaryViewController: UIViewController, onto primaryViewController: UIViewController) -> Bool {
        return splitViewController.traitCollection.horizontalSizeClass == .compact
    }
    
    func splitViewController(_ splitViewController: UISplitViewController, separateSecondaryFrom primaryViewController: UIViewController) -> UIViewController? {
        return previewNC
    }
}

extension SplitVC : UINavigationControllerDelegate {
    func navigationController(_ navigationController: UINavigationController, willShow viewController: UIViewController, animated: Bool) {
        if navigationController == scanNC {
            let previewVC = previewNC?.viewControllers.first as? DevicePreviewVC

            if let deviceVC = viewController as? DeviceVC {
                previewVC?.device = deviceVC.device
                previewVC?.delegate = deviceVC
            }
            else {
                previewVC?.device = nil
                previewVC?.delegate = nil
            }
        }
    }
}
