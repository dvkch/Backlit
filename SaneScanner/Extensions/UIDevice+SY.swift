//
//  UIDevice+SY.swift
//  SaneScanner
//
//  Created by Stanislas Chevallier on 15/05/2021.
//  Copyright © 2021 Syan. All rights reserved.
//

import UIKit

extension UIDevice {
    static var isCatalyst: Bool {
        #if targetEnvironment(macCatalyst)
        return true
        #else
        return false
        #endif
    }
}
