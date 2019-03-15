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
        // https://stackoverflow.com/a/43365841/1439489
        return UIColor(red: .random(in: 0...1),
                       green: .random(in: 0...1),
                       blue: .random(in: 0...1),
                       alpha: 1.0)
    }
}
