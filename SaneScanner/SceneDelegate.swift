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
extension SceneDelegate : UISceneDelegate {
    func scene(_ scene: UIScene, willConnectTo session: UISceneSession, options connectionOptions: UIScene.ConnectionOptions) {

        guard let _ = (scene as? UIWindowScene) else { return }
        if let windowScene = scene as? UIWindowScene {
            context = Context(windowScene: windowScene)
            window = context?.window

            // temporary fix for SVProgressHUD
            (UIApplication.shared.delegate as? AppDelegate)?.window = window

            #if targetEnvironment(macCatalyst)
            if let titlebar = windowScene.titlebar {
                titlebar.titleVisibility = .hidden
                titlebar.toolbar = nil
            }
            #endif
        }
    }
}
