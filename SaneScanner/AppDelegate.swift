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
import SVProgressHUD
import MHVideoPhotoGallery

enum SnapshotType {
    case none, devicePreview, deviceOptions, deviceOptionPopup, other
}


// TODO: cleanup all "as NSString" / "as NSData" references
// TODO: cleanup @objc

// LATER: send in chronological order, older to newer


@UIApplicationMain
class AppDelegate: UIResponder {

    static var obtain: AppDelegate {
        return UIApplication.shared.delegate as! AppDelegate
    }
    
    // MARK: Properties
    @objc var window: UIWindow?
    private var splitViewController: SplitVC!
    private var scanNavigationController: ScanNC!
    private var galleryViewController: GalleryViewController!
    private let emptyVC = EmptyGalleryVC()
    
    // MARK: Snapshot properties
    private(set) var snapshotType = SnapshotType.none
    private(set) var snapshotTestScanImagePath: String? = nil
}

extension AppDelegate : UIApplicationDelegate {
    func application(_ application: UIApplication, didFinishLaunchingWithOptions launchOptions: [UIApplication.LaunchOptionsKey : Any]? = nil) -> Bool {
        // log
        print("Document path:", FileManager.documentsDirectoryURL)
        UIDevice.current.logMemoryUsage()
        
        // TODO: add button in DEBUG in EmptyVC?
        // create test images if needed
        // GalleryManager.shared.createRandomTestImages(count: 200)
        
        // navigation controller
        scanNavigationController = ScanNC()
        scanNavigationController.isToolbarHidden = true
        scanNavigationController.customToolbar?.height = 64
        scanNavigationController.customToolbar?.padding = 0
        scanNavigationController.customToolbar?.isTranslucent = false
        scanNavigationController.viewControllers = [DevicesVC()]
        
        // gallery view controller
        galleryViewController = GalleryViewController.gallery(withPresentationStyle: .overView, uiCustomization: .sy_defaultTheme)
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
        GalleryManager.shared.addDelegate(self)
        
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
            if let currentGallery = scanNavigationController.presentedViewController as? GalleryViewController {
                if currentGallery.isShowingOverview() {
                    currentGallery.openOverview()
                }
                else {
                    galleryViewController.openImageView(forPage: currentGallery.imageViewerViewController.pageIndex)
                }
            }
            
            scanNavigationController.dismiss(animated: false, completion: nil)
        }
        
        let toolbarHidden = !constrainedW || GalleryManager.shared.items.isEmpty
        scanNavigationController.setToolbarHidden(toolbarHidden, animated: true)
    }
    
    func splitViewController(_ splitViewController: UISplitViewController, collapseSecondary secondaryViewController: UIViewController, onto primaryViewController: UIViewController) -> Bool {
        return splitViewController.traitCollection.horizontalSizeClass == .compact
    }
    
    func splitViewController(_ splitViewController: UISplitViewController, separateSecondaryFrom primaryViewController: UIViewController) -> UIViewController? {
        if GalleryManager.shared.items.isEmpty {
            return emptyVC
        }
        return galleryViewController
    }
    
    func splitViewController(_ svc: UISplitViewController, willChangeTo displayMode: UISplitViewController.DisplayMode) {
        galleryViewController.overViewViewController.viewWillAppear(false)
    }
}

extension AppDelegate : GalleryManagerDelegate {
    func galleryManager(_ manager: GalleryManager, didUpdate items: [MHGalleryItem], newItems: [MHGalleryItem], removedItems: [MHGalleryItem]) {
        let constrainedW = splitViewController.traitCollection.horizontalSizeClass == .compact
        
        scanNavigationController.setToolbarHidden(!constrainedW || items.isEmpty, animated: true)
        
        let detailsVC = splitViewController.viewControllers.object(at: 1, or: nil)
        
        if items.isEmpty, detailsVC != emptyVC {
            splitViewController.viewControllers = [scanNavigationController, emptyVC]
        }
        
        if !items.isEmpty, detailsVC != galleryViewController {
            splitViewController.viewControllers = [scanNavigationController, galleryViewController]
        }
    }
}

