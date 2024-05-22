//
//  ChasingDotsView.swift
//  Backlit
//
//  Created by syan on 22/05/2024.
//  Copyright © 2024 Syan. All rights reserved.
//

import UIKit

// https://github.com/raymondjavaxx/SpinKit-ObjC/blob/master/SpinKit/Animations/RTSpinKitChasingDotsAnimation.m
// TODO: it doesn't work ¯\_(ツ)_/¯ 
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
        setupChasingDotsAnimation()
    }
    
    
    // MARK: Properties
    var color: UIColor = .tint {
        didSet {
            updateSublayersColor(layer)
        }
    }
    var hidesWhenStopped: Bool = true
    var spinnerSize: CGFloat = 37 {
        didSet {
            setupChasingDotsAnimation()
            invalidateIntrinsicContentSize()
        }
    }
    var isAnimating: Bool = false
    
    // MARK: Style
    private func updateSublayersColor(_ layer: CALayer) {
        for sublayer in (layer.sublayers ?? []) {
            sublayer.backgroundColor = color.cgColor
            updateSublayersColor(sublayer)
        }
    }
    
    // MARK: Actions
    func startAnimating() {
        guard !isAnimating else { return }
        isAnimating = false
        isHidden = false
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
    
    // MARK: Animation
    private func pauseLayers() {
        let pausedTime = layer.convertTime(CACurrentMediaTime(), from: nil)
        layer.speed = 0
        layer.timeOffset = pausedTime
    }

    private func resumeLayers() {
        let pausedTime = layer.timeOffset
        layer.speed = 1.0
        layer.timeOffset = 0.0
        layer.beginTime = 0.0

        let timeSincePause = layer.convertTime(CACurrentMediaTime(), from: nil) - pausedTime
        layer.beginTime = timeSincePause
    }

    private func setupChasingDotsAnimation() {
        layer.sublayers?.forEach { $0.removeFromSuperlayer() }
        let size = CGSize(width: spinnerSize, height: spinnerSize)
        
        let beginTime = CACurrentMediaTime()
        
        let spinner = CALayer()
        spinner.frame = .init(x: 0.0, y: 0.0, width: size.width, height: size.height)
        spinner.anchorPoint = .init(x: 0.5, y: 0.5)
        spinner.transform = CATransform3DIdentity
        spinner.shouldRasterize = true
        spinner.rasterizationScale = UIScreen.main.scale
        layer.addSublayer(spinner)
        
        let spinnerAnim = CAKeyframeAnimation(keyPath: "transform")
        spinnerAnim.isRemovedOnCompletion = false
        spinnerAnim.repeatCount = .greatestFiniteMagnitude
        spinnerAnim.duration = 2.0
        spinnerAnim.beginTime = beginTime
        spinnerAnim.keyTimes = [0.0, 0.25, 0.5, 0.75, 1.0]
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
        spinner.add(spinnerAnim, forKey: "spinner-anim")
        
        for i in 0..<2 {
            let circle = CALayer()
            let offset = size.width * 0.3 * Double(i)
            circle.frame = CGRectOffset(CGRectApplyAffineTransform(CGRectMake(0.0, 0.0, size.width, size.height), CGAffineTransformMakeScale(0.6, 0.6)), offset, offset)
            circle.backgroundColor = color.cgColor
            circle.anchorPoint = .init(x: 0.5, y: 0.5)
            circle.cornerRadius = circle.bounds.height * 0.5
            circle.transform = CATransform3DMakeScale(0.0, 0.0, 0.0)
            
            let anim = CAKeyframeAnimation(keyPath: "transform")
            anim.isRemovedOnCompletion = false
            anim.repeatCount = .greatestFiniteMagnitude
            anim.duration = 2.0
            anim.beginTime = beginTime - (1.0 * Double(i))
            anim.keyTimes = [0.0, 0.5, 1.0]
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
            spinner.addSublayer(circle)
            circle.add(anim, forKey: "spinkit-anim")
        }
    }
}
