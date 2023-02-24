//
//  UIButton+SY.swift
//  SaneScanner
//
//  Created by syan on 23/02/2023.
//  Copyright © 2023 Syan. All rights reserved.
//

import UIKit

import ObjectiveC

private var UIButtonPrimaryActionClosure: UInt8?

extension UIButton {
    
    private var primaryActionClosure: (() -> ())? {
        get {
            return objc_getAssociatedObject(self, &UIButtonPrimaryActionClosure) as? () -> ()
        }
        set {
            objc_setAssociatedObject(self, &UIButtonPrimaryActionClosure, newValue, .OBJC_ASSOCIATION_COPY)
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
