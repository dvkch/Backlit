//
//  UIContextMenuConfiguration+SY.swift
//  Backlit
//
//  Created by syan on 07/04/2023.
//  Copyright © 2023 Syan. All rights reserved.
//

import UIKit

private var UIContextMenuConfigurationIndexPath: UInt8?

extension UIContextMenuConfiguration {
    var indexPath: IndexPath? {
        get { objc_getAssociatedObject(self, &UIContextMenuConfigurationIndexPath) as? IndexPath }
        set { objc_setAssociatedObject(self, &UIContextMenuConfigurationIndexPath, newValue, .OBJC_ASSOCIATION_COPY) }
    }
}
