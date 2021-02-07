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
import SYPictureMetadata

#if !targetEnvironment(macCatalyst)
import SVProgressHUD
#endif

enum SnapshotType {
    case none, devicePreview, deviceOptions, deviceOptionPopup, other
}

// TODO: change layout

// TODO: fix Sane not stopped (in catalyst?) when leaving the ScanVC

// TODO: show button in catalyst to open gallery folder, maybe even save pictures in Images ?

// LATER: add avahi support ?
// LATER: usb support for Catalyst ?

@UIApplicationMain
class AppDelegate: UIResponder {

    static var obtain: AppDelegate {
        return UIApplication.shared.delegate as! AppDelegate
    }
    
    // MARK: Properties
    @objc var window: UIWindow?
    private var context: Context?
    
    // MARK: Snapshot properties
    private(set) var snapshotType = SnapshotType.none
    private(set) var snapshotTestScanImagePath: String? = nil
}

extension AppDelegate : UIApplicationDelegate {
    func application(_ application: UIApplication, didFinishLaunchingWithOptions launchOptions: [UIApplication.LaunchOptionsKey : Any]? = nil) -> Bool {
        // log
        print("Document path:", FileManager.documentsDirectoryURL)
        
        // catalyst
        NSObject.fixCatalystScaling()

        // create UI on iOS < 13
        if #available(iOS 13.0, *) {} else {
            context = Context()
            window = context?.window
        }

        // customize HUD
        SVProgressHUD.applyStyle()
        
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
        
        // give time to the system to really close the deviceVC if
        // it's opened, close eventual scan alertView, and dealloc
        // the VC, which will in turn closing the device and make
        // sane exit gracefully
        DispatchQueue.main.asyncAfter(deadline: .now() + 5) {
            UIApplication.shared.endBackgroundTask(taskID)
        }
    }
}
