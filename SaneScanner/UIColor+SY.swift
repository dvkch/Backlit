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
        return .systemBlue
    }
    
    static var background: UIColor {
        if #available(iOS 13.0, *) {
            return .systemGroupedBackground
        } else {
            return .groupTableViewBackground
        }
    }

    static var backgroundAlt: UIColor {
        if #available(iOS 13.0, *) {
            return .secondarySystemGroupedBackground
        } else {
            return .lightGray
        }
    }
    
    static var cellBackground: UIColor {
        if #available(iOS 13.0, *) {
            return .secondarySystemGroupedBackground
        } else {
            return .white
        }
    }

    static var cellBackgroundAlt: UIColor {
        return cellBackground.withAlphaComponent(0.90)
    }

    static var contrastedBackground: UIColor {
        if #available(iOS 13.0, *) {
            return UIColor.init { (traits) -> UIColor in
                return traits.userInterfaceStyle == .dark ? .black : .white
            }
        } else {
            return .white
        }
    }

    static var normalText: UIColor {
        if #available(iOS 13.0, *) {
            return UIColor.label
        } else {
            return UIColor.darkText
        }
    }

    static var altText: UIColor {
        if #available(iOS 13.0, *) {
            return UIColor.secondaryLabel
        } else {
            return UIColor.lightText
        }
    }

    static var disabledText: UIColor {
        if #available(iOS 13.0, *) {
            return UIColor.placeholderText
        } else {
            return UIColor.lightGray
        }
    }

    static var pullToRefresh: UIColor {
        if #available(iOS 13.0, *) {
            return .tertiarySystemGroupedBackground
        } else {
            return UIColor(red: 0.43, green: 0.43, blue: 0.45, alpha: 1)
        }
    }
}
