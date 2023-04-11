//
//  SceneDelegate.swift
//  SaneScanner
//
//  Created by Stanislas Chevallier on 04/02/2021.
//  Copyright © 2021 Syan. All rights reserved.
//

import UIKit
import SYKit

class SceneDelegate: UIResponder {
    var window: UIWindow?
    private var context: Context?
}

@available(iOS 13.0, *)
extension SceneDelegate : UISceneDelegate, UIWindowSceneDelegate {
    func scene(_ scene: UIScene, willConnectTo session: UISceneSession, options connectionOptions: UIScene.ConnectionOptions) {
        guard let windowScene = (scene as? UIWindowScene) else { return }
        context = Context(windowScene: windowScene)
        window = context?.window
        windowScene.sizeRestrictions?.minimumSize = .init(width: 800, height: 550)

        #if targetEnvironment(macCatalyst)
        if let titlebar = windowScene.titlebar {
            titlebar.titleVisibility = .hidden
            titlebar.toolbar = nil
        }
        #endif
        
        if let shortcut = connectionOptions.shortcutItem {
            context?.openShortcut(shortcut, completion: nil)
        }
    }
    
    func windowScene(_ windowScene: UIWindowScene, performActionFor shortcutItem: UIApplicationShortcutItem, completionHandler: @escaping (Bool) -> Void) {
        guard let context = (windowScene.windows.first as? ContextWindow)?.context else {
            completionHandler(false)
            return
        }
        
        context.openShortcut(shortcutItem, completion: completionHandler)
    }
    
    func sceneDidEnterBackground(_ scene: UIScene) {
        if UIApplication.shared.applicationState == .background {
            AppDelegate.obtain.applicationDidEnterBackground(UIApplication.shared)
        }
    }
}
