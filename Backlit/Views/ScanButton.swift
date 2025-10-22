//
//  ScanButton.swift
//  Backlit
//
//  Created by syan on 28/04/2021.
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
    
    private static let maximumSize: UIContentSizeCategory = .accessibilityMedium
    private var heightConstraint: NSLayoutConstraint?
    
    private var nonDeprecatedContentEdgeInsets: UIEdgeInsets {
        get { (value(forKey: "contentEdgeInsets") as! NSValue).uiEdgeInsetsValue }
        set { setValue(newValue, forKey: "contentEdgeInsets") }
    }
    
    // MARK: Content
    private func updateStyle() {
        layer.masksToBounds = true
        layer.cornerRadius = style == .rounded ? 10 : 0
        setTitleColor(kind == .preview ? .normalText : .normalTextOnTint, for: .normal)
        setTitleColor(kind == .preview ? .altText : .altTextOnTint, for: .disabled)
        var traits = traitCollection
        if traits.preferredContentSizeCategory > type(of: self).maximumSize {
            // need to manually constraint the initial size to the max size, or it will appear to big... but when
            // changing the font size under the max, then scaling up, it will be ok. tested on iOS 16.4
            traits = UITraitCollection(traitsFrom: [traits, .init(preferredContentSizeCategory: type(of: self).maximumSize)])
        }
        titleLabel?.font = .preferredFont(forTextStyle: .body, compatibleWith: traits)
        titleLabel?.adjustsFontForContentSizeCategory = true
        maximumContentSizeCategory = type(of: self).maximumSize
        titleLabel?.numberOfLines = 2
        setContentHuggingPriority(.required, for: .vertical)
        setContentCompressionResistancePriority(.required, for: .vertical)
        nonDeprecatedContentEdgeInsets = .init(top: 10, left: 10, bottom: 10, right: 10)

        var background = kind == .preview ? UIColor.backgroundAlt : UIColor.tint
        background = background.resolvedColor(with: traitCollection)
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
            title = kind == .preview ? L10n.deviceButtonUpdatePreview : L10n.actionScan
            subtitle = nil

        case .warmingUp:
            title = kind == .preview ? L10n.previewing : L10n.scanning
            subtitle = L10n.warmingUp

        case .scanning(let progress, let finishedDocs, _, _):
            let p = Int(progress * 100)
            switch kind {
            case .scan:
                title = finishedDocs > 0 ? L10n.scanningProgressCount(p, finishedDocs) : L10n.scanningProgress(p)
            case .preview:
                title = L10n.previewingProgress(p)
            }
            subtitle =  L10n.actionHintTapToCancel

        case .cancelling:
            title = L10n.cancelling
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
        let prefFont = UIFont.preferredFont(forTextStyle: .body)
        let prefHeight = NSAttributedString(string: "X", font: prefFont).size().height

        let maxTrait = UITraitCollection(preferredContentSizeCategory: maximumSize)
        let maxFont = UIFont.preferredFont(forTextStyle: .body, compatibleWith: maxTrait)
        let maxHeight = NSAttributedString(string: "X", font: maxFont).size().height

        return prefHeight.clamped(min: 0, max: maxHeight) * 2.5
    }
}
