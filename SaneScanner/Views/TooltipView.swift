//
//  TooltipView.swift
//  SaneScanner
//
//  Created by Stanislas Chevallier on 27/04/2021.
//  Copyright © 2021 Syan. All rights reserved.
//

import UIKit

class TooltipView : UIView {
    
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
        backgroundColor = .background
        layer.borderColor = UIColor.backgroundAlt.cgColor
        layer.borderWidth = 1

        label.numberOfLines = 1
        label.textColor = .normalText
        label.font = .preferredFont(forTextStyle: .footnote)
        label.setContentHuggingPriority(.required, for: .horizontal)
        label.setContentHuggingPriority(.required, for: .vertical)
        label.setContentCompressionResistancePriority(.required, for: .horizontal)
        label.setContentCompressionResistancePriority(.required, for: .vertical)
        addSubview(label)

        label.snp.makeConstraints { (make) in
            make.top.equalToSuperview().offset(2)
            make.left.equalToSuperview().offset(4)
            make.right.equalToSuperview().offset(-4).priority(.high)
            make.bottom.equalToSuperview().offset(-2).priority(.high)
        }
    }

    // MARK: Views
    private let label = UILabel()
    
    // MARK: Actions
    func show(text: String, from view: UIView, location: CGPoint) {
        guard let window = view.window else { return }

        self.alpha = 0
        label.text = text
        layoutIfNeeded()
        
        // for some reason constraints to top left + contained to superview doesn't work, let's to it manually!
        let size = self.systemLayoutSizeFitting(window.bounds.size, withHorizontalFittingPriority: .fittingSizeLevel, verticalFittingPriority: .fittingSizeLevel)
        var adjustedLocation = window.convert(location, from: view)
        adjustedLocation.x -= max(0, adjustedLocation.x + size.width - window.bounds.size.width)
        adjustedLocation.y -= max(0, adjustedLocation.y + size.height - window.bounds.size.height)
        frame = CGRect(origin: adjustedLocation, size: size)
        window.addSubview(self)
        
        UIView.animate(withDuration: 0.2) {
            self.alpha = 1
        }
    }
    
    func dismiss(animated: Bool = true) {
        UIView.animate(withDuration: 0.2) {
            self.alpha = 0
        } completion: { (_) in
            self.removeFromSuperview()
        }
    }
}
