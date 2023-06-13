//
//  AnimatedCirclesView.swift
//  SYKit
//
//  Created by syan on 18/05/2023.
//  Copyright © 2023 Syan. All rights reserved.
//

import UIKit

@objcMembers
public class AnimatedCirclesView: UIView {
    
    // MARK: Init
    public override init(frame: CGRect) {
        super.init(frame: frame)
        setup()
    }
    
    public required init?(coder aDecoder: NSCoder) {
        super.init(coder: aDecoder)
        setup()
    }
    
    private func setup() {
        clipsToBounds = true
        
        blurView.translatesAutoresizingMaskIntoConstraints = false
        addSubview(blurView)
        NSLayoutConstraint.activate([
            blurView.topAnchor.constraint(equalTo: topAnchor),
            blurView.leftAnchor.constraint(equalTo: leftAnchor),
            blurView.rightAnchor.constraint(equalTo: rightAnchor),
            blurView.bottomAnchor.constraint(equalTo: bottomAnchor),
        ])
        
        createCircles()
    }
    
    // MARK: Properties
    public var numberOfCircles: Int = 3 {
        didSet {
            guard numberOfCircles != oldValue else { return }
            createCircles()
        }
    }
    
    public var color: UIColor = .white {
        didSet {
            updateCirclesColor()
        }
    }
    
    public var blurEffect: UIVisualEffect? = UIBlurEffect(style: .light) {
        didSet {
            blurView.effect = blurEffect
        }
    }
    
    public var animationDuration: TimeInterval = 120 {
        didSet {
            updateAnimations()
        }
    }
    
    public var animationCalculationMode: CAAnimationCalculationMode = .cubic {
        didSet {
            updateAnimations()
        }
    }
    
    // MARK: Views
    private lazy var blurView: UIVisualEffectView = .init(effect: blurEffect)
    private var circleLayers = [CircleLayer]()
    
    // MARK: Content
    private func createCircles() {
        circleLayers.forEach { $0.removeFromSuperlayer() }

        circleLayers = (0..<numberOfCircles).map { _ in
            let circleLayer = CircleLayer()
            circleLayer.drawsAsynchronously = true
            circleLayer.circleSize = Double.random(in: 0.5..<1)
            layer.addSublayer(circleLayer)
            return circleLayer
        }
        bringSubviewToFront(blurView)

        updateCirclesColor()
        DispatchQueue.main.async {
            self.updateAnimations()
        }
    }
    
    private func updateCirclesColor() {
        circleLayers.forEach { circleLayer in
            circleLayer.circleColor = color.withAlphaComponent(color.alpha * Double.random(in: 0.3...1)).cgColor
        }
    }
    
    // MARK: Animations
    private func updateAnimations() {
        CATransaction.flush()

        let framesCount = 30

        let positions: [CGPoint] = (0..<framesCount).map { _ in CGPoint.random(between: .zero, and: .one) }

        let timeSpacing: [Double] = (0..<framesCount).map { _ in Double.random(in: 1..<3) }
        let timings: [Double] = timeSpacing.additive.normalized

        CATransaction.begin()
        circleLayers.forEach { circleLayer in
            let animation = CAKeyframeAnimation(keyPath: #keyPath(CircleLayer.circleCenter))

            let circlePositions = positions.shuffled()
            animation.values = circlePositions + [circlePositions.first!] // add back the first one to properly loop
            animation.keyTimes = timings.map(NSNumber.init)               // from 0 to 1
            assert(animation.values?.count == animation.keyTimes?.count)

            animation.duration = animationDuration
            animation.repeatCount = .greatestFiniteMagnitude
            animation.calculationMode = animationCalculationMode
            animation.rotationMode = .none
            animation.isRemovedOnCompletion = true

            circleLayer.add(animation, forKey: "movement")
        }
        CATransaction.commit()
    }
    
    // MARK: Layout
    public override func layoutSubviews() {
        super.layoutSubviews()
        circleLayers.forEach {
            $0.frame = layer.bounds
            $0.contentsScale = window?.screen.scale ?? 1
        }
    }
}

private class CircleLayer: CALayer {

    // MARK: Init
    override init() {
        super.init()
    }

    override init(layer: Any) {
        super.init(layer: layer)
        if let layer = layer as? Self {
            circleSize = layer.circleSize
            circleColor = layer.circleColor
            circleCenter = layer.circleCenter
        }
    }
    
    required init?(coder: NSCoder) {
        fatalError("init(coder:) has not been implemented")
    }
    
    // MARK: Properties
    @objc dynamic var circleColor: CGColor = UIColor.white.cgColor
    @objc dynamic var circleSize: CGFloat = .zero // in % of min(bounds.size)
    @objc dynamic var circleCenter: CGPoint = .zero // in % of bounds.size

    // MARK: Drawing
    override class func needsDisplay(forKey key: String) -> Bool {
        if key == #keyPath(circleColor) {
            return true
        }
        if key == #keyPath(circleSize) {
            return true
        }
        if key == #keyPath(circleCenter) {
            return true
        }
        return super.needsDisplay(forKey: key)
    }
    
    override func draw(in ctx: CGContext) {
        super.draw(in: ctx)
        
        let size = circleSize * min(bounds.width, bounds.height)

        let circleRect = CGRect(
            x: circleCenter.x * bounds.width - size / 2,
            y: circleCenter.y * bounds.height - size / 2,
            width: size,
            height: size
        )

        ctx.setShouldAntialias(true)
        ctx.addEllipse(in: circleRect)
        ctx.setFillColor(circleColor)
        ctx.drawPath(using: .fill)
    }
    
    // MARK: Implicit animations
    override class func defaultAction(forKey event: String) -> CAAction? {
        if event == #keyPath(CALayer.bounds) {
            return NSNull()
        }
        if event == #keyPath(CALayer.position) {
            return NSNull()
        }
        return super.defaultAction(forKey: event)
    }
}

private extension CGPoint {
    static var one: CGPoint {
        return CGPoint(x: 1, y: 1)
    }

    static func random(between min: CGPoint, and max: CGPoint) -> CGPoint {
        return CGPoint(
            x: Double.random(in: min.x..<max.x),
            y: Double.random(in: min.y..<max.y)
        )
    }
    
    static func *(lhs: CGPoint, rhs: CGSize) -> CGPoint {
        return .init(x: lhs.x * rhs.width, y: lhs.y * rhs.height)
    }
}

private extension Collection where Element == Double {
    var normalized: [Element] {
        let maximum = self.max() ?? 1
        return self.map { $0 / maximum }
    }
    
    var additive: [Element] {
        return reduce([0]) { subarray, item in subarray + [item + subarray.last!] }
    }
}
