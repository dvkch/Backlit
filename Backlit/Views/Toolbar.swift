//
//  Toolbar.swift
//  Backlit
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
    
    override func layoutSubviews() {
        if let thumbsView = items?.first?.customView as? GalleryThumbsView {
            thumbsView.preferredSize = bounds.size
        }
        super.layoutSubviews()
    }
    
    override func setItems(_ items: [UIBarButtonItem]?, animated: Bool) {
        super.setItems(items, animated: animated)
        UIView.transition(with: self, duration: animated ? 0.3 : 0, options: .transitionCrossDissolve, animations: {
            self.barTintColor = items?.first?.customView?.backgroundColor
        }, completion: nil)
    }
}
