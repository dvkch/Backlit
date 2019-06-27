//
//  UIColor+SYKit.swift
//  SYKit
//
//  Created by Stanislas Chevallier on 20/02/2019.
//  Copyright © 2019 Syan. All rights reserved.
//

import UIKit

// Informations
public extension UIColor {
    
    @objc(sy_alpha)
    var alpha: CGFloat {
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

    @objc(sy_initWithHex:)
    convenience init(hex: UInt) {
        self.init(
            red:    (hex >> 16) & 0xFF,
            green:  (hex >>  8) & 0xFF,
            blue:   hex         & 0xFF
        )
    }
    
    @objc(sy_initWithIntegerRed:green:blue:)
    convenience init(red: UInt, green: UInt, blue: UInt) {
        self.init(
            red:    CGFloat(red)   / 255,
            green:  CGFloat(green) / 255,
            blue:   CGFloat(blue)  / 255,
            alpha:  1
        )
    }
    
    @objc(sy_random)
    static var random: UIColor {
        // https://stackoverflow.com/a/43365841/1439489
        return UIColor(red:   .random(in: 0...1),
                       green: .random(in: 0...1),
                       blue:  .random(in: 0...1),
                       alpha: 1.0)
    }
    
    @objc(sy_lighterByPercentage:)
    func lighter(by percentage: CGFloat = 0.3) -> UIColor? {
        return self.adjustBrightness(by: abs(percentage) )
    }
    
    @objc(sy_darkerByPercentage:)
    func darker(by percentage: CGFloat = 0.3) -> UIColor? {
        return self.adjustBrightness(by: -1 * abs(percentage) )
    }
    
    @objc(sy_adjustingBrightnessByPercentage:)
    func adjustBrightness(by percentage: CGFloat = 30.0) -> UIColor? {
        var red: CGFloat = 0, green: CGFloat = 0, blue: CGFloat = 0, alpha: CGFloat = 0
        guard getRed(&red, green: &green, blue: &blue, alpha: &alpha) else {
            return nil
        }
        
        return UIColor(red:   min(red   + percentage, 1.0),
                       green: min(green + percentage, 1.0),
                       blue:  min(blue  + percentage, 1.0),
                       alpha: alpha)
    }
}
