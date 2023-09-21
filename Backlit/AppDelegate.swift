//
//  AppDelegate.swift
//  Backlit
//
//  Created by Stanislas Chevallier on 04/02/2019.
//  Copyright © 2019 Syan. All rights reserved.
//

import UIKit
import SYKit
import SaneSwift
import SYPictureMetadata

// TODO: scanning in the background doesn't seem to properly save images, or at least doesn't refresh then propagate the list of images ? but also opening them is black ? idk. weird. => peut etre parce que l'animation foire ?

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
        // Analytics
        Analytics.shared.send(event: .appLaunch)
        
        // Snapshots
        Snapshot.setup { config in
            Preferences.shared.previewWithAutoColorMode = false
            Preferences.shared.imageFormat = .heic
            Preferences.shared.pdfSize = .imageSize
            Preferences.shared.askedAnalytics = true
            Preferences.shared.enableAnalytics = false

            SaneMockable.shared.isMockingEnabled = config.mockScan
            SaneMockable.shared.mockedScanImage = config.mockScanImage

            config.setupHost()
        }
        
        // Logging
        Logger.level = .info
        SaneLogger.externalLoggingMethod = { level, message in
            Logger.logSane(level: level, message: message)
        }
        SaneSetLogLevel(0)

        // Local notifications
        UNUserNotificationCenter.current().delegate = self
        
        // create UI on iOS < 13
        if #available(iOS 13.0, *) {} else {
            context = Context()
            window = context?.window
        }

        return true
    }
    
    func applicationDidBecomeActive(_ application: UIApplication) {
        Logger.i(.background, "App became active again, stopping keep alive")
        BackgroundKeepAlive.shared.keepAlive = false
    }
    
    func applicationDidEnterBackground(_ application: UIApplication) {
        startBackgroundTask(startTime: Date())
        updateShortcuts()
    }
    
    private func startBackgroundTask(startTime: Date) {
        guard UIApplication.shared.applicationState == .background else {
            Logger.i(.background, "Not in background, no need to keep alive in background")
            BackgroundKeepAlive.shared.keepAlive = false
            return
        }

        let highestLevelTask = allContexts.map(\.status).sorted(by: \.rawValue).last ?? .devicesList
        if highestLevelTask == .devicesList {
            Logger.i(.background, "No device opened, no need to keep alive in background")
            BackgroundKeepAlive.shared.keepAlive = false
            return
        }
        
        if highestLevelTask == .deviceOpened && Date().timeIntervalSince(startTime) > 5 * 60 {
            Logger.i(.background, "Device opened for more than 5min, aborting background keep alive")
            BackgroundKeepAlive.shared.keepAlive = false
            stopAllOperations()
            return
        }
        
        Logger.i(.background, "Keeping app alive for a bit longer")
        BackgroundKeepAlive.shared.keepAlive = true

        DispatchQueue.main.asyncAfter(deadline: .now() + 15) {
            // try to restart a background task, only if need be
            self.startBackgroundTask(startTime: startTime)
        }
    }
    
    private func stopAllOperations() {
        Logger.i(.background, "Stopping all operations")

        // Let's make Sane end gracefully to prevent using a dangling SANE_Handle
        SaneMockable.shared.cancelCurrentScan()
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

extension AppDelegate: UNUserNotificationCenterDelegate {
    func userNotificationCenter(_ center: UNUserNotificationCenter, didReceive response: UNNotificationResponse, withCompletionHandler completionHandler: @escaping () -> Void) {
        if response.actionIdentifier == UNNotificationDefaultActionIdentifier,
           let path = response.notification.request.content.userInfo["galleryItemPath"] as? String,
           let item = GalleryManager.shared.galleryItems.reversed().first(where: { $0.url.path == path })
        {
            allContexts.first?.openGallery(on: item)
        }
        completionHandler()
    }
}
