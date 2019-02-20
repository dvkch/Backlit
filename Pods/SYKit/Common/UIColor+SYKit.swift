//
//  UIColor+SYKit.swift
//  SYKit
//
//  Created by Stanislas Chevallier on 20/02/2019.
//  Copyright © 2019 Syan. All rights reserved.
//

import UIKit

// Informations
@objc public extension UIColor {
    
    @objc(sy_alpha)
    public var alpha: CGFloat {
        // https://github.com/mbcharbonneau/UIImage-Categories/blob/master/UIImage%2BAlpha.m
        var alpha: CGFloat = 0
        if getRed(nil, green: nil, blue: nil, alpha: &alpha) {
            return alpha
        }
        if getHue(nil, saturation: nil, brightness: nil, alpha: &alpha) {
            return alpha
        }
        if getWhite(nil, alpha: &alpha) {
            return alpha
        }
        return 1
    }
}
