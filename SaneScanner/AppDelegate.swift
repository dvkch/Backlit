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

// TODO: add more telemetry
// TODO: check if we can play with BT trackpad and keyboard on iPad
// TODO: add tabs/arrows/space handling on macos?

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
    
    @available(iOS 13.0, *)
    override func buildMenu(with builder: UIMenuBuilder) {
        super.buildMenu(with: builder)
        
        guard builder.system == UIMenuSystem.main else { return }
        
        let previewCommand = UIKeyCommand(title: "MENU PREVIEW".localized, action: #selector(DeviceVC.preview), input: "P", modifierFlags: .command)
        let scanCommand = UIKeyCommand(title: "MENU SCAN".localized, action: #selector(DeviceVC.scan), input: "S", modifierFlags: .command)
        let abortCommand = UIKeyCommand(title: "MENU ABORT".localized, action: #selector(DeviceVC.cancelOperation), input: UIKeyCommand.inputEscape, modifierFlags: .command)
        builder.insertChild([previewCommand, scanCommand, abortCommand].asMenu(identifier: UIMenu.Identifier("me.syan.SaneScanner.scan-actions")), atStartOfMenu: .file)

        let openGalleryCommand = UIKeyCommand(title: "MENU OPEN GALLERY".localized, action: #selector(SplitVC.openGallery), input: "O", modifierFlags: .command)
        builder.insertChild([openGalleryCommand].asMenu(identifier: UIMenu.Identifier("me.syan.SaneScanner.gallery")), atStartOfMenu: .file)

        let addHostCommand = UIKeyCommand(title: "MENU ADD HOST".localized, action: #selector(DevicesVC.addHostButtonTap), input: "N", modifierFlags: .command)
        builder.insertChild([addHostCommand].asMenu(identifier: UIMenu.Identifier("me.syan.SaneScanner.hosts")), atStartOfMenu: .file)

        let refreshCommand = UIKeyCommand(title: "MENU REFRESH".localized, action: #selector(DevicesVC.refresh), input: "R", modifierFlags: .command)
        builder.insertChild([refreshCommand].asMenu(identifier: UIMenu.Identifier("me.syan.SaneScanner.refresh")), atStartOfMenu: .view)

        let settingsCommand = UIKeyCommand(title: "MENU PREFERENCES".localized, action: #selector(DevicesVC.settingsButtonTap), input: ",", modifierFlags: .command)
        builder.insertChild([settingsCommand].asMenu(identifier: UIMenu.Identifier("me.syan.SaneScanner.preferences")), atStartOfMenu: .help)
    }
}
