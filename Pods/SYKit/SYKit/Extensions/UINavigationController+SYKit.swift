//
//  UINavigationController+SYKit.swift
//  SYKit
//
//  Created by Stanislas Chevallier on 06/02/2019.
//  Copyright © 2023 Syan. All rights reserved.
//

import UIKit
import ImageIO

// Informations
@objc public extension UINavigationController {
    @discardableResult
    func popViewController(animated: Bool, completion: @escaping () -> ()) -> UIViewController? {
        guard animated else {
            let vc = self.popViewController(animated: false)
            completion()
            return vc
        }
        
        let poppedVC = popViewController(animated: true)

        var waitForDismissal: (() -> ())?
        waitForDismissal = {
            if poppedVC?.isVisible == true {
                DispatchQueue.main.asyncAfter(deadline: .now() + 0.1) {
                    waitForDismissal?()
                }
            }
            else {
                completion()
            }
        }
        waitForDismissal?()
        return poppedVC
    }
}
