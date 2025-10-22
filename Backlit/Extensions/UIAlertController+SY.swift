//
//  UIAlertController+SY.swift
//  Backlit
//
//  Created by syan on 09/05/2021.
//  Copyright © 2021 Syan. All rights reserved.
//

import UIKit
import SaneSwift

extension UIAlertController {
    static func show(for error: Error, title: String? = nil, in viewController: UIViewController) {
        if case .cancelled = error as? SaneError {
            return
        }
        self.show(for: error, title: title, close: L10n.actionClose, in: viewController)
    }
    
    static func show(title: String? = nil, message: String? = nil, in viewController: UIViewController) {
        self.show(title: title, message: message, close: L10n.actionClose, in: viewController)
    }
}
