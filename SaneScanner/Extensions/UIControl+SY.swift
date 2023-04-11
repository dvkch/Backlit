//
//  UIControl+SY.swift
//  SaneScanner
//
//  Created by syan on 11/04/2023.
//  Copyright © 2023 Syan. All rights reserved.
//

import UIKit

private var UIControlPrimaryActionClosure: UInt8?
extension UIControl {
    // MARK: Primary action block
    private var primaryActionClosure: (() -> ())? {
        get {
            return objc_getAssociatedObject(self, &UIControlPrimaryActionClosure) as? () -> ()
        }
        set {
            objc_setAssociatedObject(self, &UIControlPrimaryActionClosure, newValue, .OBJC_ASSOCIATION_COPY)
        }
    }
    
    func addPrimaryAction(using closure: @escaping () -> ()) {
        self.primaryActionClosure = closure
        self.addTarget(self, action: #selector(blockActionTrigger), for: .primaryActionTriggered)
    }
    
    @objc private func blockActionTrigger() {
        self.primaryActionClosure?()
    }
}
