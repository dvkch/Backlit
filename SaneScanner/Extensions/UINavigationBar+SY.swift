//
//  UINavigationBar+SY.swift
//  SaneScanner
//
//  Created by syan on 30/04/2023.
//  Copyright © 2023 Syan. All rights reserved.
//

import UIKit

extension UINavigationBar {
    func setBackButtonImage(_ image: UIImage?) {
        self.backIndicatorImage = image
        self.backIndicatorTransitionMaskImage = image

        if #available(iOS 13.0, *) {
            self.standardAppearance.setBackIndicatorImage(image, transitionMaskImage: image)
            self.scrollEdgeAppearance?.setBackIndicatorImage(image, transitionMaskImage: image)
            self.compactAppearance?.setBackIndicatorImage(image, transitionMaskImage: image)
        }
        if #available(iOS 15.0, *) {
            self.compactScrollEdgeAppearance?.setBackIndicatorImage(image, transitionMaskImage: image)
        }
    }
}
