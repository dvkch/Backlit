//
//  UIColor+SY.swift
//  SaneScanner
//
//  Created by Stanislas Chevallier on 07/02/2019.
//  Copyright © 2019 Syan. All rights reserved.
//

import UIKit

extension UIColor {
    static var vividBlue: UIColor {
        return UIColor.init(red: 0, green: 0.48, blue: 1, alpha: 1)
    }
    
    static var groupTableViewHeaderTitle: UIColor {
        return UIColor.init(red: 0.43, green: 0.43, blue: 0.45, alpha: 1)
    }
    
    static var random: UIColor {
        // https://gist.github.com/kylefox/1689973
        let hue = CGFloat(arc4random() % 256 / 256)  //  0.0 to 1.0
        let saturation = CGFloat((Double(arc4random() % 128) / 256.0) + 0.5)  //  0.5 to 1.0, away from white
        let brightness = CGFloat((Double(arc4random() % 128) / 256.0) + 0.5)  //  0.5 to 1.0, away from black
        
        return UIColor(hue: hue, saturation: saturation, brightness: brightness, alpha: 1)
    }
}
