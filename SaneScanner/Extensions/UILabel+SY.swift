//
//  UILabel+SY.swift
//  SaneScanner
//
//  Created by Stanislas Chevallier on 06/03/2019.
//  Copyright © 2019 Syan. All rights reserved.
//

import UIKit

extension UILabel {

    var autoAdjustsFontSize: Bool {
        get {
            if #available(iOS 10.0, *) {
                return self.adjustsFontForContentSizeCategory
            } else {
                return false
            }
        }
        set {
            if #available(iOS 10.0, *) {
                self.adjustsFontForContentSizeCategory = newValue
            }
        }
    }
}
