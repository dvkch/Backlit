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
        let image: UIImage = .icon(.right, variant: index)!
        // compute scale because adjustsImageSizeForAccessibilityContentSizeCategory = true doesn't work here ¯\_(ツ)_/¯
        // but multiply by 0.6 because the original assets are too big
        let scale: CGFloat = UIFontMetrics.default.scaledValue(for: image.size.width) / image.size.width * 0.6

        let disclosure = UIImageView(image: image)
        disclosure.frame.size = .init(width: image.size.width * scale, height: image.size.height * scale)
        disclosure.tintColor = .altText
        disclosure.contentMode = .scaleAspectFit
        accessoryView = disclosure
        accessoryType = .none
    }
}
