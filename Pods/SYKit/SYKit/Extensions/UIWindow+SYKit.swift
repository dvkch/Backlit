//
//  UIWindow+SYKit.swift
//  SYKit
//
//  Created by Stanislas Chevallier on 27/06/2019.
//  Copyright © 2019 Syan. All rights reserved.
//

import UIKit

public extension UIWindow {
    
    static func mainWindow(rootViewController: UIViewController?) -> Self {
        let window = self.init()
        window.makeKeyAndVisible()
        
        // http://stackoverflow.com/questions/25963101/unexpected-nil-window-in-uiapplicationhandleeventfromqueueevent
        // The issue and solution described in the link above don't apply
        // for iOS 9+ screen spliting, an app started with a fraction of
        // the window would have the wrong frame. iOS uses the right frame
        // automatically
        if #available(iOS 9.0, *) { } else {
            window.frame = UIScreen.main.bounds
        }

        window.rootViewController = rootViewController
        window.backgroundColor = .black
        window.layer.masksToBounds = true
        
        return window
    }
}
