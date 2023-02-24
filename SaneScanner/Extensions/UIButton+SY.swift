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
        // TODO: test on macOS 11.0

        let button = Self(type: .roundedRect)
        
        if #available(macCatalyst 15.0, iOS 15.0, *) {
            button.preferredBehavioralStyle = .mac
            #if !targetEnvironment(macCatalyst)
            if prominent {
                button.configuration = .filled()
            }
            else {
                button.configuration = .borderedTinted()
                button.setTitleColor(.tint.adjustBrightness(by: 0.2), for: .normal)
            }
            button.configuration?.contentInsets = .init(top: 5, leading: 10, bottom: 5, trailing: 10)
            #endif
        }
        else {
            button.clipsToBounds = true
            button.layer.cornerRadius = 5
            button.contentEdgeInsets = .init(top: 5, left: 10, bottom: 5, right: 10)
            if prominent {
                button.setBackgrounColor(.tint, for: .normal)
                button.setBackgrounColor(.altText, for: .disabled)
                button.setTitleColor(.normalText, for: .normal)
            }
            else {
                button.setBackgrounColor(.tint.withAlphaComponent(0.2), for: .normal)
                button.setBackgrounColor(.altText.withAlphaComponent(0.1), for: .disabled)
                button.setTitleColor(.tint.adjustBrightness(by: 0.2), for: .normal)
                button.setTitleColor(.altText.withAlphaComponent(0.5), for: .disabled)
            }
        }
        
        return button
    }
}
