//
//  AppDelegate.swift
//  SaneScanner
//
//  Created by Stanislas Chevallier on 04/02/2019.
//  Copyright © 2019 Syan. All rights reserved.
//

import UIKit
import SYKit
import SaneSwift

enum SnapshotType {
    case none, devicePreview, deviceOptions, deviceOptionPopup, other
}

// TODO: send in chronological order, older to newer


@UIApplicationMain
class AppDelegate: UIResponder {

    static var obtain: AppDelegate {
        return UIApplication.shared.delegate as! AppDelegate
    }
    
    // MARK: Properties
    @objc var window: UIWindow?
    private var splitViewController: SplitVC!
    private var scanNavigationController: ScanNC!
    private var galleryViewController: SYGalleryController!
    private let emptyVC = EmptyGalleryVC()
    
    // MARK: Snapshot properties
    private(set) var snapshotType = SnapshotType.none
    private(set) var snapshotTestScanImagePath: String? = nil
}

extension AppDelegate : UIApplicationDelegate {
    func application(_ application: UIApplication, didFinishLaunchingWithOptions launchOptions: [UIApplication.LaunchOptionsKey : Any]? = nil) -> Bool {
        // log
        print("Document path:", SYTools.documentsPath)
        
        // create test images if needed
        // SYTools.createTestImages(200)
        
        // navigation controller
        scanNavigationController = ScanNC()
        scanNavigationController.isToolbarHidden = true
        scanNavigationController.customToolbar?.height = 64
        scanNavigationController.customToolbar?.padding = 0
        scanNavigationController.customToolbar?.isTranslucent = false
        scanNavigationController.viewControllers = [DevicesVC()]
        
        // gallery view controller
        galleryViewController = SYGalleryController.gallery(withPresentationStyle: .overView, uiCustomization: .sy_defaultTheme())
        galleryViewController.uiCustomization.hideDoneButton = true
        galleryViewController.uiCustomization.setMHGalleryBackgroundColor(.groupTableViewBackground, for: .imageViewerNavigationBarHidden)
        
        // split controller
        splitViewController = SplitVC()
        splitViewController.viewControllers = [scanNavigationController, emptyVC]
        splitViewController.delegate = self
        splitViewController.preferredDisplayMode = .allVisible
        
        // creating window
        window = SYWindow.mainWindow(withRootViewController: splitViewController)
        
        // customize HUD
        SVProgressHUD.setDefaultMaskType(.black)
        
        // auto manage toolbar visibility
        SYGalleryManager.shared.add(self)
        
        // Snapshots
        if ProcessInfo.processInfo.arguments.contains("DOING_SNAPSHOT") {
            snapshotType = .other
            Sane.shared.configuration.clearHosts()
            Sane.shared.configuration.addHost("192.168.69.42")
            SVProgressHUD.show()
        }

        if ProcessInfo.processInfo.arguments.contains("SNAPSHOT_PREVIEW") {
            snapshotType = .devicePreview
        }
        
        if ProcessInfo.processInfo.arguments.contains("SNAPSHOT_OPTIONS") {
            snapshotType = .deviceOptions
        }
        
        if ProcessInfo.processInfo.arguments.contains("SNAPSHOT_OPTION_POPUP") {
            snapshotType = .deviceOptionPopup
        }
        
        let testPathPrefix = "SNAPSHOT_TEST_IMAGE_PATH="
        if let testPathArgument = ProcessInfo.processInfo.arguments.first(where: { $0.hasPrefix(testPathPrefix) }) {
            snapshotTestScanImagePath = testPathArgument.replacingOccurrences(of: testPathPrefix, with: "")
        }

        return true
    }
    
    func applicationDidEnterBackground(_ application: UIApplication) {
        let taskID = UIApplication.shared.beginBackgroundTask(expirationHandler: {})
        
        Sane.shared.cancelCurrentScan()
        
        scanNavigationController.popToRootViewController(animated: true)
        
        // give time to the system to really close the deviceVC if
        // it's opened, close eventual scan alertView, and dealloc
        // the VC, which will in turn closing the device and make
        // sane exit gracefully
        DispatchQueue.main.asyncAfter(deadline: .now() + 5) {
            UIApplication.shared.endBackgroundTask(taskID)
        }
    }
}

extension AppDelegate : UISplitViewControllerDelegate {
    func splitVCtraitCollectionWillChange(to traitCollection: UITraitCollection) {
        let constrainedW = traitCollection.horizontalSizeClass == .compact
        let constrainedH = traitCollection.verticalSizeClass   == .compact
        
        scanNavigationController.customToolbar?.height = constrainedH ? 34 : 64
        
        if !constrainedW {
            if let currentGallery = scanNavigationController.presentedViewController as? SYGalleryController {
                if currentGallery.isShowingOverview() {
                    currentGallery.openOverview()
                }
                else {
                    galleryViewController.openImageView(forPage: currentGallery.imageViewerViewController.pageIndex)
                }
            }
            
            scanNavigationController.dismiss(animated: false, completion: nil)
        }
        
        let toolbarHidden = !constrainedW || SYGalleryManager.shared.galleryItems().isEmpty
        scanNavigationController.setToolbarHidden(toolbarHidden, animated: true)
    }
    
    func splitViewController(_ splitViewController: UISplitViewController, collapseSecondary secondaryViewController: UIViewController, onto primaryViewController: UIViewController) -> Bool {
        return splitViewController.traitCollection.horizontalSizeClass == .compact
    }
    
    func splitViewController(_ splitViewController: UISplitViewController, separateSecondaryFrom primaryViewController: UIViewController) -> UIViewController? {
        if SYGalleryManager.shared.galleryItems().isEmpty {
            return emptyVC
        }
        return galleryViewController
    }
    
    func splitViewController(_ svc: UISplitViewController, willChangeTo displayMode: UISplitViewController.DisplayMode) {
        galleryViewController.overViewViewController.viewWillAppear(false)
    }
}

extension AppDelegate : SYGalleryManagerDelegate {
    func gallerymanager(_ gallerymanager: SYGalleryManager!, didUpdate items: [MHGalleryItem]!, newItem: MHGalleryItem!, removedItem: MHGalleryItem!) {
        let constrainedW = splitViewController.traitCollection.horizontalSizeClass == .compact
        
        scanNavigationController.setToolbarHidden(!constrainedW || items.isEmpty, animated: true)
        
        let detailsVC = (splitViewController.viewControllers as NSArray).nullableObject(at: 1) as? UIViewController
        
        if items.isEmpty, detailsVC != emptyVC {
            splitViewController.viewControllers = [scanNavigationController, emptyVC]
        }
        
        if !items.isEmpty, detailsVC != galleryViewController {
            splitViewController.viewControllers = [scanNavigationController, galleryViewController]
        }
    }
}

