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

#if !MARZIPAN
import SVProgressHUD
#endif

enum SnapshotType {
    case none, devicePreview, deviceOptions, deviceOptionPopup, other
}

// TODO: add auth to Keychain?
// LATER: send in chronological order, older to newer

@UIApplicationMain
class AppDelegate: UIResponder {

    static var obtain: AppDelegate {
        return UIApplication.shared.delegate as! AppDelegate
    }
    
    // MARK: Properties
    @objc var window: UIWindow?
    private var splitViewController: SplitVC!
    private var scanNC: ScanNC!
    private var galleryNC: GalleryNC!
    
    // MARK: Snapshot properties
    private(set) var snapshotType = SnapshotType.none
    private(set) var snapshotTestScanImagePath: String? = nil
}

extension AppDelegate : UIApplicationDelegate {
    func application(_ application: UIApplication, didFinishLaunchingWithOptions launchOptions: [UIApplication.LaunchOptionsKey : Any]? = nil) -> Bool {
        // log
        print("Document path:", FileManager.documentsDirectoryURL)
        
        // navigation controller
        scanNC = ScanNC()
        scanNC.viewControllers = [DevicesVC()]
        
        // gallery view controller
        galleryNC = GalleryNC(openedAt: nil)
        /*
        GalleryViewController.gallery(withPresentationStyle: .overView, uiCustomization: .sy_defaultTheme)
        galleryViewController.uiCustomization.hideDoneButton = true
        galleryViewController.uiCustomization.setMHGalleryBackgroundColor(.groupTableViewBackground, for: .imageViewerNavigationBarHidden)
 */
        
        // split controller
        splitViewController = SplitVC()
        splitViewController.viewControllers = [scanNC, galleryNC]
        splitViewController.preferredDisplayMode = .allVisible
        
        // creating window
        window = SYWindow.mainWindow(rootViewController: splitViewController)
        
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
        
        scanNC.popToRootViewController(animated: true)
        
        // give time to the system to really close the deviceVC if
        // it's opened, close eventual scan alertView, and dealloc
        // the VC, which will in turn closing the device and make
        // sane exit gracefully
        DispatchQueue.main.asyncAfter(deadline: .now() + 5) {
            UIApplication.shared.endBackgroundTask(taskID)
        }
    }
}

extension AppDelegate : GalleryManagerDelegate {
    func galleryManager(_ manager: GalleryManager, didCreate thumbnail: UIImage, for item: GalleryItem) { }
    func galleryManager(_ manager: GalleryManager, didUpdate items: [GalleryItem], newItems: [GalleryItem], removedItems: [GalleryItem]) {
        let constrainedW = splitViewController.traitCollection.horizontalSizeClass == .compact
        
        scanNC.setToolbarHidden(!constrainedW || items.isEmpty, animated: true)
    }
}

