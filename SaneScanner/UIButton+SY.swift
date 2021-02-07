//
//  UIButton+SY.swift
//  SaneScanner
//
//  Created by Stanislas Chevallier on 07/02/2021.
//  Copyright © 2021 Syan. All rights reserved.
//

import UIKit
import SaneSwift

extension UIButton {
    func updateTitle(progress: ScanProgress?, isPreview: Bool) {
        setAttributedTitle(nil, for: .normal)

        switch progress {
        case .none:
            let string = isPreview ? "DEVICE BUTTON UPDATE PREVIEW" : "ACTION SCAN"
            setTitle(string.localized, for: .normal)

        case .warmingUp:
            let string = isPreview ? "PREVIEWING" : "SCANNING"
            setTitle(string.localized, for: .normal)

        case .scanning(let progress, _):
            let string = isPreview ? "PREVIEWING %f" : "SCANNING %f"
            let title = NSAttributedString(string: String(format: string.localized, progress * 100), font: titleLabel?.font, color: .normalText)
            let subtitle = NSAttributedString(string: "ACTION HINT TAP TO CANCEL".localized, font: titleLabel?.font, color: .altText)
            let fullTitle: NSAttributedString = [title, subtitle].concat(separator: "\n").setParagraphStyle(alignment: .center, lineSpacing: 0, paragraphSpacing: 0)
            setAttributedTitle(fullTitle, for: .normal)

        case .cancelling:
            setTitle("CANCELLING".localized, for: .normal)
        }
    }
}
