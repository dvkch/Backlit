//
//  ChasingDotsView.swift
//  Backlit
//
//  Created by syan on 22/05/2024.
//  Copyright © 2024 Syan. All rights reserved.
//

import UIKit

// https://github.com/raymondjavaxx/SpinKit-ObjC/blob/master/SpinKit/Animations/RTSpinKitChasingDotsAnimation.m
class ChasingDotsView: UIView {
    
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
        setContentHuggingPriority(.required, for: .horizontal)
        setContentHuggingPriority(.required, for: .vertical)
        setContentCompressionResistancePriority(.required, for: .horizontal)
        setContentCompressionResistancePriority(.required, for: .vertical)
        setupChasingDotsAnimation()
    }
    
    // MARK: Properties
    var color: UIColor = .tint {
        didSet {
            updateCirclesColor()
        }
    }
    var hidesWhenStopped: Bool = true
    var spinnerSize: CGFloat = 37 {
        didSet {
            invalidateIntrinsicContentSize()
            sizeToFit()
            setupChasingDotsAnimation()
        }
    }
    private(set) var isAnimating: Bool = false
    
    // MARK: Layers
    private let spinnerLayer = CALayer()

    // MARK: Style
    private func updateCirclesColor() {
        for circleLayer in (spinnerLayer.sublayers ?? []) {
            circleLayer.backgroundColor = color.cgColor
        }
    }
    
    // MARK: Actions
    func startAnimating(force: Bool = false) {
        guard !isAnimating || force else { return }
        isAnimating = true
        isHidden = false
        setupChasingDotsAnimation()
        resumeLayers()
    }

    func stopAnimating() {
        guard isAnimating else { return }
        isAnimating = false
        if hidesWhenStopped {
            isHidden = true
        }
        pauseLayers()
    }
    
    // MARK: Layout
    override func layoutSubviews() {
        super.layoutSubviews()
        setupChasingDotsAnimation()
    }
    
    override func sizeThatFits(_ size: CGSize) -> CGSize {
        return intrinsicContentSize
    }
    
    override var intrinsicContentSize: CGSize {
        return .init(width: spinnerSize, height: spinnerSize)
    }
    
    override func didMoveToSuperview() {
        super.didMoveToSuperview()
        if superview != nil && isAnimating {
            startAnimating(force: true)
        }
    }

    // MARK: Animation
    private func pauseLayers() {
        let pausedTime = layer.convertTime(CACurrentMediaTime(), from: nil)
        spinnerLayer.speed = 0
        spinnerLayer.timeOffset = pausedTime
    }

    private func resumeLayers() {
        let pausedTime = spinnerLayer.timeOffset
        spinnerLayer.speed = 1.0
        spinnerLayer.timeOffset = 0.0
        spinnerLayer.beginTime = 0.0

        let timeSincePause = layer.convertTime(CACurrentMediaTime(), from: nil) - pausedTime
        spinnerLayer.beginTime = timeSincePause
    }

    private var prevSize: CGSize = .zero
    private func setupChasingDotsAnimation() {
        guard prevSize != bounds.size else { return }
        prevSize = bounds.size

        let size = CGSize(width: spinnerSize, height: spinnerSize)
        let beginTime = CACurrentMediaTime()
        
        spinnerLayer.frame = .init(x: 0.0, y: 0.0, width: size.width, height: size.height)
        spinnerLayer.anchorPoint = .init(x: 0.5, y: 0.5)
        spinnerLayer.transform = CATransform3DIdentity
        spinnerLayer.shouldRasterize = true
        spinnerLayer.rasterizationScale = UIScreen.main.scale
        spinnerLayer.timeOffset = CACurrentMediaTime()
        layer.addSublayer(spinnerLayer)
        
        let spinnerAnim = CAKeyframeAnimation(keyPath: "transform")
        spinnerAnim.isRemovedOnCompletion = false
        spinnerAnim.repeatCount = .greatestFiniteMagnitude
        spinnerAnim.duration = 2.0
        spinnerAnim.beginTime = beginTime
        spinnerAnim.keyTimes = [0.0 as NSNumber, 0.25, 0.5, 0.75, 1.0]
        spinnerAnim.timingFunctions = [
            CAMediaTimingFunction(name: .linear),
            CAMediaTimingFunction(name: .linear),
            CAMediaTimingFunction(name: .linear),
            CAMediaTimingFunction(name: .linear),
            CAMediaTimingFunction(name: .linear)
        ]
        spinnerAnim.values = [
            CATransform3DMakeRotation(0, 0, 0, 1.0),
            CATransform3DMakeRotation(.pi / 2, 0, 0, 1.0),
            CATransform3DMakeRotation(.pi, 0, 0, 1.0),
            CATransform3DMakeRotation(3 * .pi / 2, 0, 0, 1.0),
            CATransform3DMakeRotation(2 * .pi, 0, 0, 1.0)
        ]
        spinnerLayer.removeAnimation(forKey: "spinner-anim")
        spinnerLayer.add(spinnerAnim, forKey: "spinner-anim")
        
        spinnerLayer.sublayers?.forEach { $0.removeFromSuperlayer() }
        for i in 0..<2 {
            let circle = CALayer()
            let offset = size.width * 0.3 * Double(i)
            circle.frame = spinnerLayer.bounds.applying(.init(scaleX: 0.6, y: 0.6)).offsetBy(dx: offset, dy: offset)
            circle.backgroundColor = color.cgColor
            circle.anchorPoint = .init(x: 0.5, y: 0.5)
            circle.cornerRadius = circle.bounds.height * 0.5
            circle.transform = CATransform3DMakeScale(0.0, 0.0, 0.0)
            spinnerLayer.addSublayer(circle)

            let anim = CAKeyframeAnimation(keyPath: "transform")
            anim.isRemovedOnCompletion = false
            anim.repeatCount = .greatestFiniteMagnitude
            anim.duration = 2.0
            anim.beginTime = beginTime - (1.0 * Double(i))
            anim.keyTimes = [0.0 as NSNumber, 0.5, 1.0]
            anim.timingFunctions = [
                CAMediaTimingFunction(name: .easeInEaseOut),
                CAMediaTimingFunction(name: .easeInEaseOut),
                CAMediaTimingFunction(name: .easeInEaseOut)
            ]
            anim.values = [
                CATransform3DMakeScale(0.0, 0.0, 0.0),
                CATransform3DMakeScale(1.0, 1.0, 0.0),
                CATransform3DMakeScale(0.0, 0.0, 0.0)
            ]
            circle.add(anim, forKey: "spinkit-anim")
        }
        
        if isAnimating {
            resumeLayers()
        }
        else {
            pauseLayers()
        }
    }
}
