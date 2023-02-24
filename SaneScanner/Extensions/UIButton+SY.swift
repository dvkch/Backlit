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
    
    static func system(prominent: Bool) -> Self {
        let button = Self(type: .roundedRect)
        
        // TODO: test on macOS 11.0
        // TODO: test on iOS 14.0

        if #available(macCatalyst 15.0, iOS 15.0, *) {
            button.preferredBehavioralStyle = .mac
            #if !targetEnvironment(macCatalyst)
            button.configuration = prominent ? .filled() : .borderedTinted()
            #endif
        }
        
        return button
    }
}
