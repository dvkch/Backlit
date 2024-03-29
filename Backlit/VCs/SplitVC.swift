//
//  SplitVC.swift
//  Backlit
//
//  Created by Stanislas Chevallier on 04/02/2019.
//  Copyright © 2019 Syan. All rights reserved.
//

import UIKit
import SaneSwift

class SplitVC: UISplitViewController {

    // MARK: Init
    init() {
        super.init(nibName: nil, bundle: nil)
        delegate = self
    }
    
    required init?(coder aDecoder: NSCoder) {
        fatalError("init(coder:) has not been implemented")
    }
    
    override func viewDidLoad() {
        super.viewDidLoad()
        viewControllers = [scanNC, previewNC]
        #if targetEnvironment(macCatalyst)
        maximumPrimaryColumnWidth = 500
        minimumPrimaryColumnWidth = 350
        preferredDisplayMode = .oneBesideSecondary
        #else
        preferredDisplayMode = .allVisible
        #endif
        scanNC.delegate = self
        addKeyCommand(.openGallery)
    }

    // MARK: Properties
    let scanNC: ScanNC = {
        let nc = ScanNC()
        nc.viewControllers = [DevicesVC()]
        return nc
    }()
    
    let previewNC: UINavigationController = {
        let nc = UINavigationController()
        nc.viewControllers = [DevicePreviewVC()]
        return nc
    }()
    
    // MARK: Actions
    override func canPerformAction(_ action: Selector, withSender sender: Any?) -> Bool {
        if action == #selector(openGallery) {
            return presentedViewController == nil
        }
        return super.canPerformAction(action, withSender: sender)
    }
 
    @objc func openGallery() {
        #if targetEnvironment(macCatalyst)
        // crashes in debug in Catalyst, but all good in release
        UIApplication.shared.open(GalleryManager.shared.galleryFolder, options: [:], completionHandler: nil)
        #else
        let nc = GalleryNC(openedOn: GalleryManager.shared.galleryItems.last)
        present(nc, animated: true, completion: nil)
        #endif
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
        scanNC.updateToolbar(animated: false)
        
        let useLargeLayout = traitCollection.horizontalSizeClass == .regular && traitCollection.verticalSizeClass == .regular
        scanNC.viewControllers.forEach {
            ($0 as? DeviceVC)?.useLargeLayout = useLargeLayout
            ($0 as? DeviceVC)?.delegate = self
        }
    }
    
    // MARK: Content
    private func refreshPreviewVC() {
        let previewVC = previewNC.viewControllers.first as? DevicePreviewVC
        let deviceVC = scanNC.viewControllers.compactMap { $0 as? DeviceVC }.first

        previewVC?.device = deviceVC?.device
        previewVC?.delegate = deviceVC
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
            refreshPreviewVC()
            adaptPresentedVCs()
            viewController.transitionCoordinator?.animate(alongsideTransition: nil, completion: { _ in
                self.adaptPresentedVCs()
            })
        }
    }
    
    func navigationController(_ navigationController: UINavigationController, didShow viewController: UIViewController, animated: Bool) {
        if navigationController == scanNC {
            adaptPresentedVCs()
        }
    }
}

extension SplitVC : DeviceVCDelegate {
    func deviceVC(_ deviceVC: DeviceVC, didRefreshDevice device: Device) {
        refreshPreviewVC()
    }
}
