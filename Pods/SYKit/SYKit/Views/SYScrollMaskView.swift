//
//  SYScrollMaskView.swift
//  SYKit
//
//  Created by Stanislas Chevallier on 27/06/2019.
//  Copyright © 2019 Syan. All rights reserved.
//

import UIKit

/**
 * @class SYScrollMaskView
 *
 * Container for UIScrollView and subclasses, used to mask the top and
 * bottom (or left and right) of a scrollview. This creates an effect
 * that shows the user there is something so scroll to.
 *
 * The only way to achieve that is to add the scrollView to this view, which
 * is itself masked to achieve the desired effect, since scrollViews can't be
 * directly masked (shows weird glitches, tableView cells are dropped, gradient
 * moves when we scroll etc)
 *
 * You should position this view as you would have your scrollView, set the
 * scrollView property (this will add the scrollView to this container) and
 * set the desired mask orientation and size.
 *
 */
@objcMembers
public class SYScrollMaskView: UIView {
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
        gradientMaskView.scrollMaskView = self
        mask = gradientMaskView
        setNeedsLayout()
    }
    
    // MARK: Properties
    @objc(SYScrollMask)
    public enum MaskType: Int {
        case none, vertical, horizontal
    }
    public var scrollMask: MaskType = .vertical {
        didSet { setNeedsLayout() }
    }
    public var scrollMaskSize: CGFloat = 16 {
        didSet { setNeedsLayout() }
    }
    public var scrollView: UIScrollView? {
        didSet {
            oldValue?.removeFromSuperview()
            if let scrollView = scrollView {
                addSubview(scrollView)
            }
            setNeedsLayout()
        }
    }
    private let gradientMaskView = SYScrollMaskGradientView()
    
    // MARK: Layout
    public override func layoutSubviews() {
        super.layoutSubviews()
        gradientMaskView.frame = bounds
        scrollView?.frame = bounds
        gradientMaskView.setNeedsLayout()
    }
}

/**
 * @class SYScrollMaskGradientView
 *
 * View that actually does the gradient work using layers, used as a mask by SYScrollMaskView
 *
 */
internal class SYScrollMaskGradientView: UIView {
    
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
        layerStart.colors = [UIColor.clear.cgColor, UIColor.black.cgColor]
        layerStart.startPoint = .zero
        layerStart.locations = [0, 1]
        layerStart.type = .axial
        layer.addSublayer(layerStart)
        
        layerMiddle.backgroundColor = UIColor.black.cgColor
        layer.addSublayer(layerMiddle)
        
        layerEnd.colors = [UIColor.clear.cgColor, UIColor.black.cgColor]
        layerEnd.endPoint = .zero
        layerEnd.locations = [0, 1]
        layerEnd.type = .axial
        layer.addSublayer(layerEnd)
        
        setNeedsLayout()
    }
    
    // MARK: Private properties
    fileprivate var scrollMaskView: SYScrollMaskView!
    private let layerStart  = CAGradientLayer()
    private let layerMiddle = CALayer()
    private let layerEnd    = CAGradientLayer()
    
    // MARK: Layout
    public override func layoutSubviews() {
        super.layoutSubviews()
        
        let maskSize = scrollMaskView.scrollMaskSize
        
        switch scrollMaskView.scrollMask {
        case .none:
            backgroundColor = .black
            
        case .vertical:
            backgroundColor = .clear
            
            layerStart.frame = CGRect(x: layer.bounds.minX, y: layer.bounds.minY, width: layer.bounds.width, height: maskSize)
            layerStart.endPoint = CGPoint(x: 0, y: 1)
            
            layerMiddle.frame = CGRect(x: 0, y: maskSize, width: layer.bounds.width, height: layer.bounds.height - 2 * maskSize)
            
            layerEnd.frame = CGRect(x: layer.bounds.minX, y: layer.bounds.maxY - maskSize, width: layer.bounds.width, height: maskSize)
            layerEnd.startPoint = CGPoint(x: 0, y: 1)
            
        case .horizontal:
            backgroundColor = .clear
            
            layerStart.frame = CGRect(x: layer.bounds.minX, y: layer.bounds.minY, width: maskSize, height: layer.bounds.height)
            layerStart.endPoint = CGPoint(x: 1, y: 0)
            
            layerMiddle.frame = CGRect(x: maskSize, y: 0, width: layer.bounds.width - 2 * maskSize, height: layer.bounds.height)
            
            layerEnd.frame = CGRect(x: layer.bounds.maxX - maskSize, y: layer.bounds.minY, width: maskSize, height: layer.bounds.height)
            layerEnd.startPoint = CGPoint(x: 1, y: 0)
        }
    }
}
