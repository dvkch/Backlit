//
//  UIAlertController+SY.swift
//  SaneScanner
//
//  Created by Stanislas Chevallier on 09/05/2021.
//  Copyright © 2021 Syan. All rights reserved.
//

import UIKit

extension UIAlertController {
    static func show(for error: Error, title: String? = nil, in viewController: UIViewController) {
        let alert = self.init(title: title, message: error.localizedDescription, preferredStyle: .alert)
        alert.addAction(title: "ACTION CLOSE".localized, style: .cancel, handler: nil)
        viewController.present(alert, animated: true, completion: nil)
    }
    
    static func show(title: String? = nil, message: String? = nil, in viewController: UIViewController) {
        let alert = self.init(title: title, message: message, preferredStyle: .alert)
        alert.addAction(title: "ACTION CLOSE".localized, style: .cancel, handler: nil)
        viewController.present(alert, animated: true, completion: nil)
    }
}
