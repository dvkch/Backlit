//
//  UIBarButtonItem+SY.swift
//  SaneScanner
//
//  Created by Stanislas Chevallier on 13/05/2021.
//  Copyright © 2021 Syan. All rights reserved.
//

import UIKit
import ObjectiveC

private var UIBarButtonItemTapBlock: UInt8?

extension UIBarButtonItem {
    convenience init(systemItem: UIBarButtonItem.SystemItem, block: @escaping (() -> ())) {
        self.init(barButtonSystemItem: systemItem, target: nil, action: nil)
        self.tapBlock = block
    }
    
    var tapBlock: (() -> ())? {
        get { objc_getAssociatedObject(self, &UIBarButtonItemTapBlock) as? (() -> ()) }
        set {
            objc_setAssociatedObject(self, &UIBarButtonItemTapBlock, newValue, .OBJC_ASSOCIATION_COPY)
            self.target = self
            self.action = #selector(tapBlockCallback)
        }
    }
    
    @objc private func tapBlockCallback() {
        tapBlock?()
    }
}
