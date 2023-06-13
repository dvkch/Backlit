//
//  UINavigationBar+SYKit.swift
//  SYKit
//
//  Created by Stanislas Chevallier on 06/02/2019.
//  Copyright © 2023 Syan. All rights reserved.
//

import UIKit
import ImageIO

// Informations
@objc public extension UINavigationBar {
    func setBackButtonImage(_ image: UIImage?) {
        #if !os(tvOS)
        self.backIndicatorImage = image
        self.backIndicatorTransitionMaskImage = image
        #endif

        if #available(iOS 13.0, tvOS 13.0, *) {
            self.standardAppearance.setBackIndicatorImage(image, transitionMaskImage: image)
            self.scrollEdgeAppearance?.setBackIndicatorImage(image, transitionMaskImage: image)
            self.compactAppearance?.setBackIndicatorImage(image, transitionMaskImage: image)
        }
        if #available(iOS 15.0, tvOS 15.0, *) {
            self.compactScrollEdgeAppearance?.setBackIndicatorImage(image, transitionMaskImage: image)
        }
    }
}
