//
//  UINavigationController+SY.swift
//  SaneScanner
//
//  Created by syan on 12/04/2023.
//  Copyright © 2023 Syan. All rights reserved.
//

import UIKit
import SYKit

extension UINavigationController {
    func popViewController(animated: Bool, completion: @escaping () -> ()) {
        guard animated else {
            self.popViewController(animated: false)
            return
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
    }
}
