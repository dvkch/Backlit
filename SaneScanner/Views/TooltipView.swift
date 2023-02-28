//
//  TooltipView.swift
//  SaneScanner
//
//  Created by Stanislas Chevallier on 27/04/2021.
//  Copyright © 2021 Syan. All rights reserved.
//

import UIKit

// LATER: replace with UIToolTipInteraction when dropping macOS 11.0
class TooltipView : UIView {
    
    // MARK: Init
    required init(for view: UIView, title: @escaping () -> String?) {
        self.containingView = view
        self.titleBlock = title
        super.init(frame: .zero)
        setup()
    }
    
    required init?(coder aDecoder: NSCoder) {
        fatalError("init(coder:) has not been implemented")
    }
    
    private func setup() {
        backgroundColor = .background
        layer.borderColor = UIColor.separator.cgColor
        layer.borderWidth = 1
        layer.cornerRadius = 2
        layer.shadowColor = UIColor.black.cgColor
        layer.shadowOffset = .zero
        layer.shadowRadius = 10
        layer.shadowOpacity = 0.6

        label.numberOfLines = 0
        label.textColor = .normalText
        label.font = .preferredFont(forTextStyle: .callout)
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
            make.width.lessThanOrEqualTo(400)
        }
        
        if #available(iOS 13.0, *) {
            tooltipGesture = UIHoverGestureRecognizer(target: self, action: #selector(hoverGestureRecognized(_:)))
            containingView.addGestureRecognizer(tooltipGesture)
        }
    }

    // MARK: Views
    private let containingView: UIView
    private let label = UILabel()
    private var hoverTimer: Timer? {
        didSet {
            oldValue?.invalidate()
        }
    }
    private var tooltipGesture: UIGestureRecognizer!
    private var titleBlock: (() -> String?)?

    // MARK: Actions
    @objc private func hoverGestureRecognized(_ gesture: UIGestureRecognizer) {
        if gesture.state == .began {
            hoverTimer = Timer(timeInterval: 0.7, target: self, selector: #selector(showTooltip), userInfo: nil, repeats: false)
            RunLoop.main.add(hoverTimer!, forMode: .common)
        }
        if gesture.state == .ended || gesture.state == .cancelled {
            hoverTimer?.invalidate()
            dismiss()
        }
    }
    
    @objc private func showTooltip() {
        guard let title = titleBlock?() else { return }

        var location = tooltipGesture.location(in: containingView)
        location.y += 20 // approximate cursor height
        show(text: title, from: containingView, location: location)
    }

    private func observePositionRelativeToWindow(_ previousOrigin: CGPoint?) {
        guard let window = window else { return }
        
        let origin = containingView.convert(CGPoint.zero, to: window)
        if previousOrigin != nil && previousOrigin != origin {
            dismiss()
            return
        }
        
        DispatchQueue.main.asyncAfter(deadline: .now() + 0.2) { [weak self] in
            self?.observePositionRelativeToWindow(origin)
        }
    }

    // MARK: Content
    private func show(text: String, from view: UIView, location: CGPoint) {
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

        observePositionRelativeToWindow(nil)

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
