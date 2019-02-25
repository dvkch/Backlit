//
//  Toolbar.swift
//  SaneScanner
//
//  Created by Stanislas Chevallier on 04/02/2019.
//  Copyright © 2019 Syan. All rights reserved.
//

import UIKit

class Toolbar: UIToolbar {
    
    // MARK: Properties
    var padding: CGFloat = 0 {
        didSet {
            superview?.setNeedsLayout()
            setNeedsLayout()
        }
    }
    
    var height: CGFloat? {
        didSet {
            superview?.setNeedsLayout()
            setNeedsLayout()
        }
    }
    
    @objc var _textButtonMargin: CGFloat {
        return padding
    }
    
    @objc var _imageButtonMargin: CGFloat {
        return padding
    }
    
    // MARK: Layout
    override func sizeThatFits(_ size: CGSize) -> CGSize {
        var s = super.sizeThatFits(size)
        if let height = self.height {
            s.height = height
        }
        return s
    }
    
    // MARK: Items
    override func setItems(_ items: [UIBarButtonItem]?, animated: Bool) {
        super.setItems(items, animated: animated)
        barTintColor = items?.first?.customView?.backgroundColor ?? .clear
    }
}
