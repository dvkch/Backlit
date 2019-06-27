//
//  SYShapeView.swift
//  SYKit
//
//  Created by Stanislas Chevallier on 27/06/2019.
//  Copyright © 2019 Syan. All rights reserved.
//

import UIKit

@objcMembers
public class SYShapeView: UIView {
    
    public override static var layerClass: AnyClass {
        return CAShapeLayer.self
    }
    
    public var shapeLayer: CAShapeLayer {
        return layer as! CAShapeLayer
    }
    
    public var useBackgroundColorAsFillColor: Bool = false
    public var layoutSubviewsBlock: ((SYShapeView) -> ())?
    
    public override func layoutSubviews() {
        super.layoutSubviews()
        layoutSubviewsBlock?(self)
    }

    public override var backgroundColor: UIColor? {
        get {
            if useBackgroundColorAsFillColor {
                if let cgColor = layer.backgroundColor {
                    return UIColor(cgColor: cgColor)
                }
                return nil
            }
            return super.backgroundColor
        }
        set {
            if useBackgroundColorAsFillColor {
                super.backgroundColor = .clear
                shapeLayer.fillColor = newValue?.cgColor
            }
            else {
                super.backgroundColor = newValue
            }
        }
    }
}
