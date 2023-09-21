//
//  ProgressMaskView.swift
//  Backlit
//
//  Created by Stanislas Chevallier on 13/05/2021.
//  Copyright © 2021 Syan. All rights reserved.
//

import UIKit

class ProgressMaskView: UIView {
    // MARK: Init
    override init(frame: CGRect) {
        super.init(frame: frame)
        setup()
    }
    
    required init?(coder aDecoder: NSCoder) {
        super.init(coder: aDecoder)
        setup()
    }
    
    private func setup() {
        gradientLayer.type = .axial
        gradientLayer.drawsAsynchronously = true
        layer.addSublayer(gradientLayer)
        
        isUserInteractionEnabled = false
        updateColors()
    }

    // MARK: Properties
    var cropAreaPercent: CGRect? {
        didSet {
            isHidden = cropAreaPercent == nil
            setNeedsLayout()
        }
    }
    
    var progress: Float? {
        didSet {
            setNeedsLayout()
        }
    }
    
    private var previousProgressDate: Date?
    
    // MARK: Views
    private let gradientLayer = CAGradientLayer()
    
    // MARK: Layout
    override func layoutSubviews() {
        CATransaction.begin()
        CATransaction.setDisableActions(true)
        if let cropAreaPercent = cropAreaPercent {
            // Just a quick comment for future me: when writing this line you were very happy. As you should. It
            // is clean, easy to understand, uses a method on CGRect you had already made and worked beautifully.
            // We had tried to use layout constraints and it was a mess. Now it's one line in PreviewView and
            // one line here, way better. Be nice to yourself, that's good code
            gradientLayer.frame = cropAreaPercent.fromPercents(of: bounds)
        }

        let progress = CGFloat(self.progress ?? 0)
        let progressHeight = 10 / max(1, gradientLayer.bounds.height)

        gradientLayer.startPoint = .init(x: 0, y: progress)
        gradientLayer.endPoint = .init(x: 0, y: progress + progressHeight)
        CATransaction.commit()

        super.layoutSubviews()
    }
    
    // MARK: Style
    private func updateColors() {
        gradientLayer.colors = [
            UIColor.tint.withAlphaComponent(0.7).cgColor,
            UIColor.tint.withAlphaComponent(0).cgColor
        ]
    }
    
    override func traitCollectionDidChange(_ previousTraitCollection: UITraitCollection?) {
        super.traitCollectionDidChange(previousTraitCollection)
        updateColors()
    }
}
