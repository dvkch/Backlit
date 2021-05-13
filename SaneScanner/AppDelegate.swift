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
import TelemetryClient

// TODO: add review prompt every N scans
// TODO: UX: verrouillage des options pendant un scan/preview/update d'options. utilisation d'une propriété Device.lock observable? est-ce necesaire puisque les calls sont tous sur une queue synchrone (are they tho?)

// TODO: Catalyst: replace pull to refresh with navBar item ?
// TODO: Catalyst: keyboard shortcuts, menu, etc (+ test on iPad)

// LATER: add auto search on local network
// LATER: usb support for Catalyst ? (excluding those that don't include the Sane condition licence)
// LATER: see if blocking IO is necessary (seems to be an issue after a scan, where it might prevent updating an option... and can even ask for auth even if none is set)
// LATER: test text field input for String & Int and handle Auto

@UIApplicationMain
class AppDelegate: UIResponder {

    static var obtain: AppDelegate {
        return UIApplication.shared.delegate as! AppDelegate
    }
    
    // MARK: Properties
    @objc var window: UIWindow?
    private var context: Context?
}

extension AppDelegate : UIApplicationDelegate {
    func application(_ application: UIApplication, didFinishLaunchingWithOptions launchOptions: [UIApplication.LaunchOptionsKey : Any]? = nil) -> Bool {
        // log
        print("Gallery path:", GalleryManager.shared.galleryFolder)
        
        // catalyst
        NSObject.fixCatalystScaling()

        // create UI on iOS < 13
        if #available(iOS 13.0, *) {} else {
            context = Context()
            window = context?.window
        }
        
        // Analytics
        TelemetryManager.initialize(with: .init(appID: "9CF71A71-190A-4B84-AB6B-2E0DE0A44F12s"))
        TelemetryManager.send("start", for: Preferences.shared.telemetryUserID, with: [:])

        // Snapshots
        if SnapshotKind.fromLaunchOptions == .other {
            Sane.shared.configuration.clearHosts()
            Sane.shared.configuration.addHost("192.168.69.42")
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
