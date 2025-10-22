//
//  Context.swift
//  Backlit
//
//  Created by Stanislas Chevallier on 04/02/2021.
//  Copyright © 2021 Syan. All rights reserved.
//

import UIKit
import SYKit
import SaneSwift

class Context: NSObject {
    init(windowScene: UIWindowScene) {
        window = ContextWindow.mainWindow(windowScene: windowScene, rootViewController: nil)
        super.init()
        setup()
    }
    
    // MARK: Properties
    let window: ContextWindow
    private let splitViewController = SplitVC()

    // MARK: UI properties
    var currentPreviewView: PreviewView? {
        if let previewVC = splitViewController.previewNC.viewControllers.last as? DevicePreviewVC, previewVC.isVisible {
            return previewVC.previewView
        }
        if let deviceVC = splitViewController.scanNC.viewControllers.last as? DeviceVC {
            return deviceVC.previewCell?.previewView
        }
        return nil
    }

    // MARK: Methods
    private func setup() {
        window.context = self

        #if DEBUG
        window.enableSlowAnimationsOnShake = true
        #endif
        window.tintColor = .tint

        // auto manage toolbar visibility
        GalleryManager.shared.addDelegate(self)

        // update window
        window.rootViewController = splitViewController
    }
    
    func openShortcut(_ shortcut: UIApplicationShortcutItem, completion: ((Bool) -> ())?) {
        // let's find the device name we would like to open
        guard shortcut.type == "device", let deviceName = shortcut.userInfo?["device"] as? String else {
            completion?(false)
            return
        }
        
        // if we can already find the device in the already visible devices, let's open it
        if let device = Sane.shared.devices.first(where: { $0.name.rawValue == deviceName }) {
            openDevice(device: device)
            completion?(true)
            return
        }
        
        // if not, let's try to refresh and find the proper device. we abort if it took more than 5s, to prevent
        // breaking the user flow
        let refreshStart = Date()
        Sane.shared.updateDevices { result in
            guard Date().timeIntervalSince(refreshStart) < 5 else {
                completion?(false)
                return
            }
            if let device = Sane.shared.devices.first(where: { $0.name.rawValue == deviceName }) {
                self.openDevice(device: device)
                completion?(true)
            }
            else {
                completion?(false)
            }
        }
    }
    
    private func openDevice(device: Device) {
        if let deviceVC = splitViewController.scanNC.viewControllers.last as? DeviceVC {
            // device is already opened, all good
            if deviceVC.device == device {
                return
            }

            // another device is opened, if it's scanning we don't do anything
            if deviceVC.isScanning {
                deviceVC.close {
                    self.openDevice(device: device)
                }
                return
            }
        }
        splitViewController.scanNC.popToRootViewController(animated: true)

        guard let devicesVC = splitViewController.scanNC.viewControllers.last as? DevicesVC else { return }
        devicesVC.openDevice(device)
    }
    
    func openGallery(on item: GalleryItem) {
        #if targetEnvironment(macCatalyst)
        // crashes in debug in Catalyst, but all good in release
        UIApplication.shared.open(GalleryManager.shared.galleryFolder, options: [:], completionHandler: nil)
        #else
        let topVC = splitViewController.presentedViewController ?? splitViewController
        let nc = GalleryNC(openedOn: GalleryManager.shared.galleryItems.last)
        topVC.present(nc, animated: true, completion: nil)
        #endif
    }
    
    // MARK: Status
    enum Status: Int {
        case devicesList
        case deviceOpened
        case scanning
    }
    var status: Status {
        if let deviceVC = splitViewController.scanNC.viewControllers.last as? DeviceVC {
            if deviceVC.isScanning || deviceVC.isSavingScan {
                return .scanning
            }
            return .deviceOpened
        }
        return .devicesList
    }

    // MARK: Notifications
    func stopOperations() {
        splitViewController.scanNC.popToRootViewController(animated: true)
    }
}

extension Context : GalleryManagerDelegate {
    func galleryManager(_ manager: GalleryManager, didUpdate items: [GalleryItem], newItems: [GalleryItem], removedItems: [GalleryItem]) {
        let constrainedW = splitViewController.traitCollection.horizontalSizeClass == .compact
        
        splitViewController.scanNC.setToolbarHidden(!constrainedW || items.isEmpty, animated: true)
    }
}

class ContextWindow: SYWindow {
    fileprivate(set) var context: Context?
}
