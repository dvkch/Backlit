//
//  CropMaskView.swift
//  SaneScanner
//
//  Created by Stanislas Chevallier on 07/02/2019.
//  Copyright © 2019 Syan. All rights reserved.
//

import UIKit
import SYKit
import SnapKit

class CropMaskView: UIControl {

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
        backgroundColor = .clear
        accessibilityIdentifier = "crop-view"
        isAccessibilityElement = true

        maskingView.shapeLayer.fillRule = .evenOdd
        addSubview(maskingView)
        
        maskingView.snp.makeConstraints { (make) in
            make.edges.equalToSuperview()
        }
        
        for side in [CGRectSide.top, .left, .right, .bottom] {
            let view = TapInsetsView()
            view.backgroundColor = .tint
            view.accessibilityIdentifier = "crop-side-\(side.debugDescription)"
            view.accessibilityTraits = .adjustable
            view.isAccessibilityElement = true
            view.layer.isOpaque = true
            addSubview(view)
            
            if side.isVertical {
                view.tapInsets = UIEdgeInsets(leftAndRight: -15)
            } else {
                view.tapInsets = UIEdgeInsets(topAndBottom: -15)
            }
            
            let panGesture = UIPanGestureRecognizer()
            panGesture.isEnabled = true
            panGesture.addTarget(self, action: #selector(self.borderPanGestureTouch(gesture:)))
            panGesture.delegate = self
            view.addGestureRecognizer(panGesture)
            
            borderPanGestures[side] = panGesture
            borderViews[side] = view
        }
        
        for corner in [CGRectCorner.topLeft, .topRight, .bottomLeft, .bottomRight] {
            let view = TapInsetsView()
            view.tapInsets = .init(value: -5)
            view.backgroundColor = .tint
            view.accessibilityIdentifier = "crop-corner-\(corner.debugDescription)"
            view.accessibilityTraits = .adjustable
            view.isAccessibilityElement = true
            view.layer.borderColor = UIColor.white.cgColor
            view.layer.borderWidth = 2
            view.layer.cornerRadius = cornerViewSize / 2
            view.layer.shadowColor = UIColor.black.cgColor
            view.layer.shadowOffset = .zero
            view.layer.shadowOpacity = 0.3
            view.layer.shadowRadius = 3
            view.layer.rasterizationScale = UIScreen.main.scale
            view.layer.shouldRasterize = true
            addSubview(view)
            
            let panGesture = UIPanGestureRecognizer()
            panGesture.addTarget(self, action: #selector(self.cornerPanGestureTouch(gesture:)))
            panGesture.delegate = self
            borderPanGestures.values.forEach { borderGesture in
                panGesture.shouldBeRequiredToFail(by: borderGesture)
            }
            view.addGestureRecognizer(panGesture)
            
            cornerPanGestures[corner] = panGesture
            cornerViews[corner] = view
        }
        
        setNeedsLayout()
        updateColors()
    }
    
    // MARK: Views
    private let maskingView = SYShapeView()
    private var cornerViews = [CGRectCorner: UIView]()
    private var borderViews = [CGRectSide:   UIView]()
    private var cornerPanGestures = [CGRectCorner: UIPanGestureRecognizer]()
    private var borderPanGestures = [CGRectSide:   UIPanGestureRecognizer]()
    
    // MARK: Gestures
    private func prioritizeGesturesOver(_ gesture: UIGestureRecognizer) {
        cornerPanGestures.values.forEach { g in
            gesture.require(toFail: g)
        }
        borderPanGestures.values.forEach { g in
            gesture.require(toFail: g)
        }
    }

    // MARK: Properties
    private(set) var maxCropArea: CGRect = .zero
    private(set) var cropArea: CGRect = .zero
    override var isEnabled: Bool {
        didSet {
            updateColors()
        }
    }
    var cropAreaDidChangeBlock: ((CGRect) -> ())?
    
    // MARK: Data
    private var maxCropAreaInViewBounds: CGRect {
        return maxCropArea.asPercents(of: maxCropArea).fromPercents(of: bounds)
    }
    
    var cropAreaInViewBounds: CGRect {
        return cropArea.asPercents(of: maxCropArea).fromPercents(of: bounds)
    }
    
    private func setCropAreaInViewBounds(_ rect: CGRect, notify: Bool) {
        let newValue = rect.asPercents(of: bounds).fromPercents(of: maxCropArea)
        setCropArea(newValue, maxCropArea: maxCropArea)
        
        if notify {
            cropAreaDidChangeBlock?(cropArea)
        }
    }
    
    func setCropArea(_ cropArea: CGRect, maxCropArea: CGRect) {
        self.maxCropArea = maxCropArea
        self.cropArea = cropArea.intersection(maxCropArea)
        setNeedsLayout()
    }
    
    // MARK: Gestures
    private var panFirstPoint: CGPoint?
    private var panCropArea: CGRect? {
        didSet {
            setNeedsLayout()
        }
    }
    
    @objc private func borderPanGestureTouch(gesture: UIPanGestureRecognizer) {
        switch gesture.state {
        case .began:
            panFirstPoint = gesture.location(in: self)
            
        case .changed:
            guard let panFirstPoint = self.panFirstPoint else { return }
            let point = gesture.location(in: self)
            let side = borderPanGestures.first(where: { $0.value == gesture })!.key
            let delta = side.isVertical ? point.x - panFirstPoint.x : point.y - panFirstPoint.y
            
            panCropArea = cropAreaInViewBounds.moving(side: side, delta: delta, maxRect: maxCropAreaInViewBounds)
            
        case .ended:
            if let panCropArea = self.panCropArea {
                setCropAreaInViewBounds(panCropArea, notify: true)
                self.panCropArea = nil
            }
            
        default: break
        }
    }
    
    @objc private func cornerPanGestureTouch(gesture: UIPanGestureRecognizer) {
        switch gesture.state {
        case .began:
            panFirstPoint = gesture.location(in: self)
            
        case .changed:
            guard let panFirstPoint = self.panFirstPoint else { return }
            let point = gesture.location(in: self)
            let delta = CGSize(width: point.x - panFirstPoint.x, height: point.y - panFirstPoint.y)
            
            let corner = cornerPanGestures.first(where: { $0.value == gesture })!.key
            panCropArea = cropAreaInViewBounds.moving(corner: corner, delta: delta, maxRect: maxCropAreaInViewBounds)
            
        case .ended:
        if let panCropArea = self.panCropArea {
            setCropAreaInViewBounds(panCropArea, notify: true)
            self.panCropArea = nil
        }
            
        default: break
        }
    }
    
    override func point(inside point: CGPoint, with event: UIEvent?) -> Bool {
        let insets = UIEdgeInsets(top: -cornerViewSize, left: -cornerViewSize, bottom: -cornerViewSize, right: -cornerViewSize)
        if bounds.inset(by: insets).contains(point) {
            return true
        }
        return super.point(inside: point, with: event)
    }
    
    // MARK: Keyboard
    override var keyCommands: [UIKeyCommand]? {
        let keys = [UIKeyCommand.inputUpArrow, UIKeyCommand.inputLeftArrow, UIKeyCommand.inputRightArrow, UIKeyCommand.inputDownArrow]
        let modifiers = [
            UIKeyModifierFlags(),
            .alternate,
            .shift,
            [.shift, .alternate],
        ]
        
        let commands = keys.map { key in
            modifiers.map { modifier -> UIKeyCommand in
                let command = UIKeyCommand(input: key, modifierFlags: modifier, action: #selector(pressedArrow(_:)))
                if #available(macCatalyst 15.0, iOS 15.0, *) {
                    command.wantsPriorityOverSystemBehavior = true
                }
                return command
            }
        }.reduce([], +)

        return commands
    }
    
    @objc private func pressedArrow(_ command: UIKeyCommand) {
        guard isEnabled else { return }
        let shift = command.modifierFlags.contains(.shift)
        let option = command.modifierFlags.contains(.alternate)

        var cropArea = self.cropArea
        let dx = option ? maxCropArea.width / 300 : maxCropArea.width / 50
        let dy = option ? maxCropArea.height / 300 : maxCropArea.height / 50

        switch command.input {
        case UIKeyCommand.inputUpArrow:
            if !shift {
                cropArea.origin.y -= dy
                cropArea.size.height += dy
            }
            else {
                cropArea.size.height -= dy
            }

        case UIKeyCommand.inputLeftArrow:
            if !shift {
                cropArea.origin.x -= dx
                cropArea.size.width += dx
            }
            else {
                cropArea.size.width -= dx
            }

        case UIKeyCommand.inputRightArrow:
            if !shift {
                cropArea.origin.x += dx
                cropArea.size.width -= dx
            }
            else {
                cropArea.size.width += dx
            }

        case UIKeyCommand.inputDownArrow:
            if !shift {
                cropArea.origin.y += dy
                cropArea.size.height -= dy
            }
            else {
                cropArea.size.height += dy
            }

        default: break
        }
        
        setCropArea(cropArea, maxCropArea: maxCropArea)
        cropAreaDidChangeBlock?(cropArea)
    }

    // MARK: Style
    private func updateColors() {
        maskingView.shapeLayer.fillColor = UIColor(white: 0, alpha: 0.4).cgColor
        cornerViews.values.forEach { $0.backgroundColor = isEnabled ? .tint : .disabledText }
        borderViews.values.forEach { $0.backgroundColor = isEnabled ? .tint : .disabledText }
    }
    
    override func traitCollectionDidChange(_ previousTraitCollection: UITraitCollection?) {
        super.traitCollectionDidChange(previousTraitCollection)
        updateColors()
    }
    
    // MARK: Layout
    private let cornerViewSize = CGFloat(20)
    private let kBorderWidth   = CGFloat(2)
    
    override func layoutSubviews() {
        defer {
            super.layoutSubviews()
        }
        
        guard !cropArea.isNull, !maxCropArea.isNull else {
            cornerViews.values.forEach { $0.isHidden = true }
            borderViews.values.forEach { $0.isHidden = true }
            maskingView.isHidden = true
            return
        }
        
        cornerViews.values.forEach { $0.isHidden = false }
        borderViews.values.forEach { $0.isHidden = false }
        maskingView.isHidden = false
        
        let cropRect = panCropArea ?? cropAreaInViewBounds
        
        cornerViews.forEach { (corner, view) in
            view.frame.size = .init(width: cornerViewSize, height: cornerViewSize)
            view.center = cropRect.point(for: corner)
        }
        
        borderViews.forEach { (border, view) in
            let originCentered = CGPoint(x: cropRect.minX - kBorderWidth / 2, y: cropRect.minY - kBorderWidth / 2)
            
            switch border {
            case .top:
                view.frame = CGRect(x: originCentered.x, y: originCentered.y, width: cropRect.width, height: kBorderWidth)
            case .bottom:
                view.frame = CGRect(x: originCentered.x, y: originCentered.y + cropRect.height, width: cropRect.width, height: kBorderWidth)
            case .left:
                view.frame = CGRect(x: originCentered.x, y: originCentered.y, width: kBorderWidth, height: cropRect.height)
            case .right:
                view.frame = CGRect(x: originCentered.x + cropRect.width, y: originCentered.y, width: kBorderWidth, height: cropRect.height)
            }
        }
        
        let insetsHalfBorderWidth = UIEdgeInsets(top: -kBorderWidth / 2, left: -kBorderWidth / 2, bottom: -kBorderWidth / 2, right: -kBorderWidth / 2)
        let path = UIBezierPath(rect: bounds)
        path.append(UIBezierPath(rect: cropRect.inset(by: insetsHalfBorderWidth)))
        maskingView.shapeLayer.path = path.cgPath
    }
}

extension CropMaskView : UIGestureRecognizerDelegate {
    func gestureRecognizer(_ gestureRecognizer: UIGestureRecognizer, shouldRecognizeSimultaneouslyWith otherGestureRecognizer: UIGestureRecognizer) -> Bool {
        // make sure the navigationController.interactivePopGestureRecognizer doesn't take over our cropping gestures
        // and closes the deviceVC while trying to crop an image. surprisingly implementing
        // gestureRecognizer(_:, shouldBeRequiredToFailBy:) doesn't always gets called with the popGesture, but this
        // method does. So we make sure to catch the gesture here, cancel it, then prioritize our own
        if let interactivePopGestureRecognizer = otherGestureRecognizer as? UIScreenEdgePanGestureRecognizer {
            otherGestureRecognizer.cancelPendingTouches()
            prioritizeGesturesOver(interactivePopGestureRecognizer)
            // needed to immediately allow our pan gestures to continue
            return true
        }
        // prevent UIScrollView scrolling while changing the crop
        return false
    }
}

