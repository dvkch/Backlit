//
//  UIColor+SY.swift
//  Backlit
//
//  Created by syan on 07/02/2019.
//  Copyright © 2019 Syan. All rights reserved.
//

import UIKit

extension UIColor {
    
    private static var accentColor: UIColor? {
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

        return UIColor(red: 112, green: 65, blue: 192)
    }
    
    static var background: UIColor {
        return .systemGroupedBackground
    }

    static var backgroundAlt: UIColor {
        return .secondarySystemGroupedBackground
    }
    
    static var cellBackground: UIColor {
        if UIDevice.isCatalyst {
            return .background
        }
        return .secondarySystemGroupedBackground
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
        } else {
            return UIColor.opaqueSeparator
        }
    }

    static var normalText: UIColor {
        return UIColor.label
    }
    
    static var normalTextOnTint: UIColor {
        return .white
    }

    static var altText: UIColor {
        return UIColor.secondaryLabel
    }

    static var altTextOnTint: UIColor {
        return UIColor.lightGray
    }

    static var disabledText: UIColor {
        return UIColor.placeholderText
    }

    static var pullToRefresh: UIColor {
        let lightColor = UIColor.gray
        return UIColor { (traits) -> UIColor in
            return traits.userInterfaceStyle == .light ? lightColor : .tertiarySystemGroupedBackground
        }
    }
}
