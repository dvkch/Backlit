//
//  UIWindow+SYKit.swift
//  SYKit
//
//  Created by Stanislas Chevallier on 27/06/2019.
//  Copyright © 2019 Syan. All rights reserved.
//

import UIKit

public extension UIWindow {
    
    @available(iOS 13.0, tvOS 13.0, *)
    static func mainWindow(windowScene: UIWindowScene?, rootViewController: UIViewController?) -> Self {
        let window: Self
        if let windowScene = windowScene {
            window = self.init(windowScene: windowScene)
        } else {
            window = self.init()
        }
        configureWindow(window, rootViewController: rootViewController)
        return window
    }
    
    static func mainWindow(rootViewController: UIViewController?) -> Self {
        let window = self.init()
        configureWindow(window, rootViewController: rootViewController)
        return window
    }
    
    private static func configureWindow(_ window: UIWindow, rootViewController: UIViewController?) {
        window.makeKeyAndVisible()
        
        window.rootViewController = rootViewController
        window.backgroundColor = .black
        window.layer.masksToBounds = true
    }
}
