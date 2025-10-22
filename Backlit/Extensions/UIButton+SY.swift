//
//  UIButton+SY.swift
//  Backlit
//
//  Created by syan on 23/02/2023.
//  Copyright © 2023 Syan. All rights reserved.
//

import UIKit

import ObjectiveC

extension UIButton {
    // MARK: Style
    static func system(prominent: Bool) -> Self {
        let button = Self(type: .roundedRect)
        button.isPointerInteractionEnabled = true

        #if targetEnvironment(macCatalyst)
        // by default macOS counts 20px for the intrinsicContentSize.height of the button, but
        // it is actually 21px. to prevent the button from being cut, let's try to improve this
        button.translatesAutoresizingMaskIntoConstraints = false
        button.heightAnchor.constraint(greaterThanOrEqualToConstant: 22).isActive = true
        #else
        
        if prominent {
            button.configuration = .filled()
        }
        else {
            button.configuration = .borderedTinted()
            button.setTitleColor(.tint.adjustBrightness(by: 0.2), for: .normal)
        }
        button.configuration?.contentInsets = .init(top: 5, leading: 10, bottom: 5, trailing: 10)
        #endif

        return button
    }
}
