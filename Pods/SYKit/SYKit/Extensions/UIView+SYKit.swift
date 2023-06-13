//
//  UIView+SYKit.swift
//  SYKit
//
//  Created by Stanislas Chevallier on 27/06/2019.
//  Copyright © 2019 Syan. All rights reserved.
//

import UIKit

public extension UIView {

    @objc(sy_findSubviewsOfClass:recursive:)
    @available(swift, obsoleted: 1.0)
    func objc_subviews(ofKind clazz: AnyClass, recursive: Bool) -> [UIView] {
        guard let t = clazz as? UIView.Type else { return [] }
        return subviews(ofKind: t, recursive: recursive)
    }

    func subviews<T: UIView>(ofKind clazz: T.Type, recursive: Bool) -> [T] {
        var result = [T]()
        
        for subview in subviews {
            // need to do both tests, because when calling from objc_subviews(ofKing:recursive:) we will have clazz of the requested type, but T will UIView
            if subview.isKind(of: clazz), let subview = subview as? T {
                result.append(subview)
            }
            
            if recursive {
                result.append(contentsOf: subview.subviews(ofKind: clazz, recursive: recursive))
            }
        }
        return result
    }

    @objc(sy_safeAreaBounds)
    var safeAreaBounds: CGRect {
        return bounds.inset(by: safeAreaInsets)
    }

    // hiding an item in a UIStackView that is already hidden while animating has a bug in UIKit and prevents us to ever make this item visible again
    @objc(sy_isHidden)
    var sy_isHidden: Bool {
        get { return isHidden }
        set {
            if newValue == isHidden { return }
            isHidden = newValue
        }
    }

    @objc(sy_screenshot)
    var screenshot: UIImage? {
        UIGraphicsBeginImageContextWithOptions(self.bounds.size, false, UIScreen.main.scale)
        defer { UIGraphicsEndImageContext() }
        drawHierarchy(in: bounds, afterScreenUpdates: true)
        return UIGraphicsGetImageFromCurrentImageContext()
    }
    
    @objc(sy_minFrameForSubviews)
    var minFrameForSubviews: CGRect {
        return self.subviews
            .map { $0.frame }
            .reduce(.zero, { $0.union($1) })
    }
}
