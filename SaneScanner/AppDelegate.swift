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

@UIApplicationMain
class AppDelegate: UIResponder {

    static var obtain: AppDelegate {
        return UIApplication.shared.delegate as! AppDelegate
    }
    
    // MARK: Properties
    @objc var window: UIWindow?
    private var context: Context?
    
    private var allContexts: [Context] {
        return UIApplication.shared.windows.compactMap({ $0 as? ContextWindow }).compactMap(\.context)
    }
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
            Sane.shared.configuration.hosts = [.init(hostname: snapshotHost, displayName: snapshotHost)]
        }
        
        Logger.level = .info
        SaneLogger.externalLoggingMethod = { level, message in
            Logger.logSane(level: level, message: message)
        }
        SaneSetLogLevel(0)

        return true
    }
    
    func applicationDidEnterBackground(_ application: UIApplication) {
        startBackgroundTask()
        updateShortcuts()
    }
    
    private func startBackgroundTask(retry: Int = 0) {
        guard UIApplication.shared.applicationState == .background else { return }

        let requiresBackgroundMode = allContexts.contains(where: { $0.status != .devicesList })
        guard requiresBackgroundMode else {
            Logger.i(.app, "No device opened, no need to keep alive in background")
            return
        }
        
        var backgroundTaskID: UIBackgroundTaskIdentifier? = nil
        backgroundTaskID = UIApplication.shared.beginBackgroundTask(withName: "saneKeepAlive-\(retry + 1)", expirationHandler: {
            // try to restart a background task, only if need be
            self.startBackgroundTask(retry: retry + 1)

            // give time to the system to really close the deviceVC if
            // it's opened, close eventual scan alertView, and dealloc
            // the VC, which will in turn closing the device and make
            // sane exit gracefully
            let gracePeriod = UIApplication.shared.backgroundTimeRemaining.clamped(min: 1, max: 5)
            DispatchQueue.main.asyncAfter(deadline: .now() + gracePeriod) {
                if let backgroundTaskID {
                    UIApplication.shared.endBackgroundTask(backgroundTaskID)
                }
            }
        })

        if backgroundTaskID == .invalid {
            Logger.w(.app, "Couldn't start a background task, let's cancel scans and close devices")
            stopAllOperations()
        }
        else {
            Logger.i(.app, "Keeping app alive for a bit longer")
        }
    }
    
    private func stopAllOperations() {
        Logger.i(.app, "Stopping all operations")

        // Let's make Sane end gracefully to prevent using a dangling SANE_Handle
        Sane.shared.cancelCurrentScan()
        allContexts.forEach { $0.stopOperations() }

        // will be restarted by DevicesVC
        SaneBonjour.shared.stop()
    }
    
    private func updateShortcuts() {
        UIApplication.shared.shortcutItems = Sane.shared.devices.map { device in
            return UIApplicationShortcutItem(
                type: "device",
                localizedTitle: device.model,
                localizedSubtitle: device.host.displayName,
                icon: UIApplicationShortcutIcon(templateImageName: UIImage.Icon.scannerSmall.rawValue),
                userInfo: ["device": device.name.rawValue as NSString]
            )
        }
    }
    
    func application(_ application: UIApplication, performActionFor shortcutItem: UIApplicationShortcutItem, completionHandler: @escaping (Bool) -> Void) {
        context?.openShortcut(shortcutItem, completion: completionHandler)
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
