//
//  UIControl+SYKit.swift
//  SYKit
//
//  Created by Stanislas Chevallier on 27/06/2019.
//  Copyright © 2019 Syan. All rights reserved.
//

import UIKit

// MARK: Primary action block
private var UIControlPrimaryActionClosure: UInt8?
public extension UIControl {
    private var primaryActionClosure: (() -> ())? {
        get {
            return objc_getAssociatedObject(self, &UIControlPrimaryActionClosure) as? () -> ()
        }
        set {
            objc_setAssociatedObject(self, &UIControlPrimaryActionClosure, newValue, .OBJC_ASSOCIATION_COPY)
        }
    }
    
    @objc(sy_addPrimaryActionUsingBlock:)
    func addPrimaryAction(using closure: @escaping () -> ()) {
        self.primaryActionClosure = closure
        self.addTarget(self, action: #selector(blockActionTrigger), for: .primaryActionTriggered)
    }
    
    @objc private func blockActionTrigger() {
        self.primaryActionClosure?()
    }
}
