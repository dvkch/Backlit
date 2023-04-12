//
//  UIColor+SY.swift
//  SaneScanner
//
//  Created by Stanislas Chevallier on 07/02/2019.
//  Copyright © 2019 Syan. All rights reserved.
//

import UIKit

extension UIColor {
    
    static var accentColor: UIColor? {
        #if targetEnvironment(macCatalyst)
        return UIColor.value(forKey: "controlAccentColor") as? UIColor
        #else
        return nil
        #endif
    }

    static var tint: UIColor {
        if let accentColor {
            return accentColor
        }

        let lighterTint = UIColor(red: 140, green: 103, blue: 204)
        let darkerTint = UIColor(red: 112, green: 65, blue: 192)
        if #available(iOS 13.0, *) {
            return UIColor { (traits) -> UIColor in
                return traits.userInterfaceStyle == .light ? darkerTint : lighterTint
            }
        } else {
            return darkerTint
        }
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
        if UIDevice.isCatalyst {
            return .background
        }
        if #available(iOS 13.0, *) {
            return .secondarySystemGroupedBackground
        } else {
            return .white
        }
    }

    static var cellBackgroundAlt: UIColor {
        if UIDevice.isCatalyst {
            return .background
        }
        return cellBackground.withAlphaComponent(0.90)
    }

    static var splitSeparator: UIColor {
        if UIDevice.isCatalyst {
            return UIColor.black
        } else if #available(iOS 13.0, *) {
            return UIColor.opaqueSeparator
        } else {
            return UIColor.darkGray
        }
    }

    static var normalText: UIColor {
        if #available(iOS 13.0, *) {
            return UIColor.label
        } else {
            return UIColor.darkText
        }
    }
    
    static var normalTextOnTint: UIColor {
        return .white
    }

    static var altText: UIColor {
        if #available(iOS 13.0, *) {
            return UIColor.secondaryLabel
        } else {
            return UIColor.darkGray
        }
    }

    static var altTextOnTint: UIColor {
        return UIColor.lightGray
    }

    static var disabledText: UIColor {
        if #available(iOS 13.0, *) {
            return UIColor.placeholderText
        } else {
            return UIColor.lightGray
        }
    }

    static var pullToRefresh: UIColor {
        let lightColor = UIColor.gray
        if #available(iOS 13.0, *) {
            return UIColor { (traits) -> UIColor in
                return traits.userInterfaceStyle == .light ? lightColor : .tertiarySystemGroupedBackground
            }
        } else {
            return lightColor
        }
    }
}
