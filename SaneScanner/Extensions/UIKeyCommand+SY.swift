//
//  UIKeyCommand+SY.swift
//  SaneScanner
//
//  Created by Stanislas Chevallier on 14/05/2021.
//  Copyright © 2021 Syan. All rights reserved.
//

import UIKit

@available(iOS 13.0, *)
extension Array where Element: UIMenuElement {
    func asMenu(identifier: UIMenu.Identifier? = nil) -> UIMenu {
        return UIMenu(title: "", image: nil, identifier: identifier, options: .displayInline, children: self)
    }
}
