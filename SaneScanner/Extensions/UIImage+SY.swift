//
//  UIImage+SY.swift
//  SaneScanner
//
//  Created by Stanislas Chevallier on 06/03/2019.
//  Copyright © 2019 Syan. All rights reserved.
//

import UIKit

extension UIImage {
    static func testImage(size: CGFloat) -> UIImage? {
        let letter = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789".randomElement() ?? "A"
        let label = UILabel()
        label.font = UIFont.boldSystemFont(ofSize: size / 2)
        label.textAlignment = .center
        label.text = String(letter)
        label.textColor = .white
        label.backgroundColor = .clear
        label.frame = CGRect(x: 0, y: 0, width: size, height: size)
        
        return .screenshottingContext(size: CGSize(width: size, height: size), scale: 1) {
            UIColor.random.setFill()
            UIBezierPath(rect: label.bounds).fill()

            label.draw(label.bounds)
        }
    }
    
    static func screenshottingContext(size: CGSize, scale: CGFloat = 1, _ block: () -> ()) -> UIImage? {
        UIGraphicsBeginImageContextWithOptions(size, true, scale)
        defer { UIGraphicsEndImageContext() }
        block()
        return UIGraphicsGetImageFromCurrentImageContext()
    }
}
