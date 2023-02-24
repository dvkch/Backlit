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

// LATER: add names to hosts
// LATER: add support for https://github.com/alexpevzner/sane-airscan
// LATER: usb support for Catalyst ? (excluding those that don't include the Sane condition license)
// LATER: test text field input for String & Int and handle Auto
// LATER: add auto update on macOS using https://sparkle-project.org/

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
        // create UI on iOS < 13
        if #available(iOS 13.0, *) {} else {
            context = Context()
            window = context?.window
        }
        
        // Analytics
        Analytics.shared.send(event: .appLaunch)
        
        // Snapshots
        if Snapshot.isSnapshotting, let snapshotHost = Snapshot.snapshotHost {
            Sane.shared.configuration.clearHosts()
            Sane.shared.configuration.addHost(snapshotHost)
        }
        
        SaneSetLogLevel(0)
        SaneLogger.level = .info

        return true
    }
    
    func applicationDidEnterBackground(_ application: UIApplication) {
        let taskID = UIApplication.shared.beginBackgroundTask(expirationHandler: {})
        
        // will be restarted by DevicesVC
        SaneBonjour.shared.stop()

        // Let's make Sane end gracefully to prevent using a dangling SANE_Handle
        Sane.shared.cancelCurrentScan()
        
        // give time to the system to really close the deviceVC if
        // it's opened, close eventual scan alertView, and dealloc
        // the VC, which will in turn closing the device and make
        // sane exit gracefully
        DispatchQueue.main.asyncAfter(deadline: .now() + 5) {
            UIApplication.shared.endBackgroundTask(taskID)
        }
    }
    
    @available(iOS 13.0, *)
    override func buildMenu(with builder: UIMenuBuilder) {
        super.buildMenu(with: builder)

        guard builder.system == UIMenuSystem.main else { return }
        builder.insertChild([UIKeyCommand.preview, .scan, .abort].asMenu(identifier: UIMenu.Identifier("me.syan.SaneScanner.scan-actions")), atStartOfMenu: .file)
        builder.insertChild([UIKeyCommand.openGallery].asMenu(identifier: UIMenu.Identifier("me.syan.SaneScanner.gallery")), atStartOfMenu: .file)
        builder.insertChild([UIKeyCommand.addHost].asMenu(identifier: UIMenu.Identifier("me.syan.SaneScanner.hosts")), atStartOfMenu: .file)
        builder.insertChild([UIKeyCommand.refresh].asMenu(identifier: UIMenu.Identifier("me.syan.SaneScanner.refresh")), atStartOfMenu: .view)
        builder.insertChild([UIKeyCommand.settings].asMenu(identifier: UIMenu.Identifier("me.syan.SaneScanner.preferences")), atStartOfMenu: .help)
    }
}
