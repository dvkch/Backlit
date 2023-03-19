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
    override init(frame: CGRect) {
        super.init(frame: frame)
        setup()
    }
    
    required init?(coder aDecoder: NSCoder) {
        super.init(coder: aDecoder)
        setup()
    }
    
    override func awakeFromNib() {
        super.awakeFromNib()
        setup()
    }
    
    private func setup() {
        updateStyle()
        updateContent()
    }

    // MARK: Properties
    var kind: ScanOperation = .scan {
        didSet {
            updateStyle()
            updateContent()
        }
    }
    
    enum Style { case cell, rounded }
    var style: Style = .cell {
        didSet {
            // super important to ignore identical style, or we'll multiply scanning time ~4x because of the progress updates
            guard style != oldValue else { return }
            updateStyle()
        }
    }
    
    var progress: ScanProgress? {
        didSet {
            switch progress {
            case .none, .cancelling, .warmingUp:
                roundedProgress = progress
            case .scanning(let p, let c, let img, let params):
                roundedProgress = .scanning(
                    progress: Float(Int(p * 100)) / 100, finishedDocs: c,
                    incompletePreview: img, parameters: params
                )
            }
        }
    }
    
    private var roundedProgress: ScanProgress? {
        didSet {
            guard progress != oldValue else { return }
            updateContent()
        }
    }
    
    override var isEnabled: Bool {
        didSet {
            updateContent()
        }
    }
    
    private var heightConstraint: NSLayoutConstraint?
    
    // MARK: Content
    private func updateStyle() {
        layer.masksToBounds = true
        layer.cornerRadius = style == .rounded ? 10 : 0
        setTitleColor(kind == .preview ? .normalText : .normalTextOnTint, for: .normal)
        setTitleColor(kind == .preview ? .altText : .altTextOnTint, for: .disabled)
        titleLabel?.font = .preferredFont(forTextStyle: .body)
        titleLabel?.adjustsFontForContentSizeCategory = true
        titleLabel?.numberOfLines = 2
        setContentHuggingPriority(.required, for: .vertical)
        setContentCompressionResistancePriority(.required, for: .vertical)
        contentEdgeInsets = .init(top: 10, left: 10, bottom: 10, right: 10)

        var background = kind == .preview ? UIColor.backgroundAlt : UIColor.tint
        if #available(iOS 13.0, *) {
            background = background.resolvedColor(with: traitCollection)
        }
        setBackgroundColor(background, for: .normal)
        setBackgroundColor(background.withAlphaComponent(style == .rounded ? 0.7 : 1), for: .disabled)

        if heightConstraint == nil {
            heightConstraint = heightAnchor.constraint(equalToConstant: 0)
            heightConstraint?.isActive = true
        }
        heightConstraint?.constant = ScanButton.preferredHeight
        
        // styles are copied in the attributed string, let's rebuild it
        updateContent()
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

        case .scanning(let progress, let finishedDocs, _, _):
            let titleFormat: String
            switch kind {
            case .scan:
                titleFormat = finishedDocs > 0 ? "SCANNING %f %d" : "SCANNING %f"
            case .preview:
                titleFormat = "PREVIEWING %f"
            }
            title = String(format: titleFormat.localized, progress * 100, finishedDocs)
            subtitle = "ACTION HINT TAP TO CANCEL".localized

        case .cancelling:
            title = "CANCELLING".localized
            subtitle = nil
        }

        let fullTitle = [
            NSAttributedString(string: title, font: .preferredFont(forTextStyle: .body), color: titleColor(for: isEnabled ? .normal : .disabled)),
            NSAttributedString(string: subtitle, font: .preferredFont(forTextStyle: .callout), color: titleColor(for: .disabled))
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
