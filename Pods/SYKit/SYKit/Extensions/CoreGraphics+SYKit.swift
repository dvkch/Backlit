//
//  CoreGraphics+SYKit.swift
//  SYKit
//
//  Created by Stanislas Chevallier on 27/06/2019.
//  Copyright © 2019 Syan. All rights reserved.
//

import CoreGraphics
import UIKit

public extension UIEdgeInsets {
    init(value: CGFloat) {
        self.init(top: value, left: value, bottom: value, right: value)
    }
    
    init(leftAndRight: CGFloat) {
        self.init(top: 0, left: leftAndRight, bottom: 0, right: leftAndRight)
    }
    
    init(topAndBottom: CGFloat) {
        self.init(top: topAndBottom, left: 0, bottom: topAndBottom, right: 0)
    }
}

public extension CGSize {
    static func +(lhs: CGSize, rhs: CGFloat) -> CGSize {
        return .init(width: lhs.width + rhs, height: lhs.height + rhs)
    }
    
    var area: CGFloat {
        return width * height
    }
    
    init(area: CGFloat, aspectRatio: CGFloat) {
        // area  = w * h
        // ratio = w / h
        self.init(width: sqrt(area * aspectRatio), height: sqrt(area / aspectRatio))
    }
}

public extension CGRect {
    func asPercents(of containingRect: CGRect) -> CGRect {
        let spanX = containingRect.maxX
        let spanY = containingRect.maxY
        
        guard spanX != 0, spanY != 0 else { return CGRect(x: 0, y: 0, width: 1, height: 1) }
        
        return CGRect(x: origin.x / spanX,
                      y: origin.y / spanY,
                      width:  size.width  / spanX,
                      height: size.height / spanY)
    }
    
    func fromPercents(of containingRect: CGRect) -> CGRect {
        let spanX = containingRect.maxX
        let spanY = containingRect.maxY
        
        guard spanX != 0, spanY != 0 else { return containingRect }
        
        return CGRect(x: origin.x * spanX,
                      y: origin.y * spanY,
                      width:  size.width  * spanX,
                      height: size.height * spanY)
    }
}

