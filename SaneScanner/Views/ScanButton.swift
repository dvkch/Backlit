//
//  ScanButton.swift
//  SaneScanner
//
//  Created by Stanislas Chevallier on 28/04/2021.
//  Copyright © 2021 Syan. All rights reserved.
//

import UIKit
import SaneSwift

class ScanButton : UIButton {
    
    // MARK: Init
    override func awakeFromNib() {
        super.awakeFromNib()
        updateStyle()
        updateContent()
    }

    // MARK: Properties
    var kind: ScanOperation = .scan {
        didSet {
            updateContent()
        }
    }
    
    enum Style { case cell, rounded }
    var style: Style = .cell {
        didSet {
            updateStyle()
        }
    }
    
    var progress: ScanProgress? {
        didSet {
            guard progress != oldValue else { return }
            updateContent()
        }
    }
    
    private var heightConstraint: NSLayoutConstraint?
    
    // MARK: Content
    private func updateStyle() {
        layer.masksToBounds = true
        layer.cornerRadius = style == .rounded ? 10 : 0
        setTitleColor(kind == .preview ? .normalText : .normalTextOnTint, for: .normal)
        setTitleColor(.altText, for: .disabled)
        titleLabel?.font = .preferredFont(forTextStyle: .body)
        titleLabel?.autoAdjustsFontSize = true
        titleLabel?.numberOfLines = 2
        setContentHuggingPriority(.required, for: .vertical)
        setContentCompressionResistancePriority(.required, for: .vertical)
        contentEdgeInsets = .init(top: 10, left: 10, bottom: 10, right: 10)

        var background = kind == .preview ? UIColor.cellBackground : .tint
        if #available(iOS 13.0, *) {
            background = background.resolvedColor(with: traitCollection)
        }
        setBackgrounColor(background, for: .normal)
        setBackgrounColor(background.withAlphaComponent(0.7), for: .disabled)

        if heightConstraint == nil {
            heightConstraint = heightAnchor.constraint(equalToConstant: 0)
            heightConstraint?.isActive = true
        }
        heightConstraint?.constant = ScanButton.preferredHeight
    }

    private func updateContent() {
        setAttributedTitle(nil, for: .normal)
        
        let title: String
        let subtitle: String?

        switch progress {
        case .none:
            title = kind == .preview ? "DEVICE BUTTON UPDATE PREVIEW".localized : "ACTION SCAN".localized
            subtitle = nil

        case .warmingUp:
            title = kind == .preview ? "PREVIEWING".localized : "SCANNING".localized
            subtitle = "WARMING UP".localized

        case .scanning(let progress, _, _):
            let titleFormat = kind == .preview ? "PREVIEWING %f" : "SCANNING %f"
            title = String(format: titleFormat.localized, progress * 100)
            subtitle = "ACTION HINT TAP TO CANCEL".localized

        case .cancelling:
            title = "CANCELLING".localized
            subtitle = nil
        }

        let fullTitle = [
            NSAttributedString(string: title, font: .preferredFont(forTextStyle: .body), color: .normalText),
            NSAttributedString(string: subtitle, font: .preferredFont(forTextStyle: .callout), color: .altText)
        ].concat(separator: "\n").setParagraphStyle(alignment: .center, lineSpacing: 0, paragraphSpacing: 0)
        setAttributedTitle(fullTitle, for: .normal)
    }

    // MARK: Layout
    override func traitCollectionDidChange(_ previousTraitCollection: UITraitCollection?) {
        super.traitCollectionDidChange(previousTraitCollection)
        updateStyle()
    }
    
    static var preferredHeight: CGFloat {
        return NSAttributedString(string: "X", font: .preferredFont(forTextStyle: .body)).size().height * 2.5
    }
}
