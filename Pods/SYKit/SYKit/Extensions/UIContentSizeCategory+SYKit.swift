//
//  UIContentSizeCategory+SY.swift
//  SYKit
//
//  Created by Stanislas Chevallier on 13/02/2021.
//  Copyright © 2021 Syan. All rights reserved.
//

import UIKit

public extension UIContentSizeCategory {
    var isAccessibilitySize: Bool {
        return rawValue.contains("Accessibility")
    }
}
