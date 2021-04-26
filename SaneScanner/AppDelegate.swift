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

// TODO: change layout for iPad and catalyst
// TODO: adapt SYEmailHelper for catalyst?

// TODO: fix scan showing black image after cancelled preview
// TODO: fix accessibility font size in option cell, put option under the title instead of on the right
// TODO: show button in catalyst to open gallery folder, maybe even save pictures in Images ?
// TODO: test keyboard shortcuts on catalyst (& ipad ?)
// TODO: test perfs with instruments when scanning and reading options

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
        if SnapshotKind.fromLaunchOptions == .other {
            Sane.shared.configuration.clearHosts()
            Sane.shared.configuration.addHost("192.168.69.42")
            SVProgressHUD.show()
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
