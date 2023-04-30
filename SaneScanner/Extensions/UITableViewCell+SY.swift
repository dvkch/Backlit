//
//  UITableViewCell+SY.swift
//  SaneScanner
//
//  Created by syan on 30/04/2023.
//  Copyright © 2023 Syan. All rights reserved.
//

import UIKit

extension UITableViewCell {
    func showDisclosureIndicator(index: Int) {
        let disclosure = UIImageView(image: .icon(.right, variant: index))
        disclosure.adjustsImageSizeForAccessibilityContentSizeCategory = true
        disclosure.tintColor = .altText
        disclosure.transform = .init(scaleX: 0.6, y: 0.6)
        disclosure.contentMode = .center
        accessoryView = disclosure
        accessoryType = .none
    }
}
