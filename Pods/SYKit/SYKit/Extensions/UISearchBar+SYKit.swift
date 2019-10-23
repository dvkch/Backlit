//
//  UISearchBar+SYKit.swift
//  SYKit
//
//  Created by Stanislas Chevallier on 27/06/2019.
//  Copyright © 2019 Syan. All rights reserved.
//

import UIKit

public extension UISearchBar {
    
    @objc(sy_textField)
    var textField: UITextField? {
        if #available(iOS 13, *) {
            return findTextView(in: self, depth: 3)
        }
        return findTextView(in: self, depth: 2)
    }
    
    private func findTextView(in view: UIView, depth: Int) -> UITextField? {
        if let field = view as? UITextField { return field }
        guard depth > 0 else { return nil }
        return view.subviews.compactMap { findTextView(in: $0, depth: depth - 1) }.first
    }
}
