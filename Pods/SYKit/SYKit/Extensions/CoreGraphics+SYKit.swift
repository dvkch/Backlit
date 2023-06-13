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

public enum CGRectSide {
    case top, left, right, bottom
    
    public var isVertical: Bool {
        return self == .left || self == .right
    }
}

extension CGRectSide: CustomDebugStringConvertible {
    public var debugDescription: String {
        switch self {
        case .top:      return "top"
        case .left:     return "left"
        case .right:    return "right"
        case .bottom:   return "bottom"
        }
    }
}

public enum CGRectCorner {
    case topLeft, topRight, bottomLeft, bottomRight
    
    public var horizontalSide: CGRectSide {
        return (self == .topLeft || self == .topRight) ? .top : .bottom
    }
    
    public var verticalSide: CGRectSide {
        return (self == .topLeft || self == .bottomLeft) ? .left : .right
    }
}

extension CGRectCorner: CustomDebugStringConvertible {
    public var debugDescription: String {
        switch self {
        case .topLeft:      return "topLeft"
        case .topRight:     return "topRight"
        case .bottomLeft:   return "bottomLeft"
        case .bottomRight:  return "bottomRight"
        }
    }
}

public extension CGRect {
    func point(for corner: CGRectCorner) -> CGPoint {
        switch corner {
        case .topLeft:      return CGPoint(x: minX, y: minY)
        case .topRight:     return CGPoint(x: maxX, y: minY)
        case .bottomLeft:   return CGPoint(x: minX, y: maxY)
        case .bottomRight:  return CGPoint(x: maxX, y: maxY)
        }
    }
    
    func moving(corner: CGRectCorner, delta: CGSize, maxRect: CGRect? = nil) -> CGRect {
        var newRect = self
        
        newRect = newRect.moving(side: corner.verticalSide, delta: delta.width)
        newRect = newRect.moving(side: corner.horizontalSide, delta: delta.height)
        
        if let maxRect = maxRect {
            newRect = newRect.intersection(maxRect)
        }

        return newRect
    }
    
    func moving(side: CGRectSide, delta: CGFloat, maxRect: CGRect? = nil) -> CGRect {
        var newRect = self
        
        switch side {
        case .top:
            newRect.origin.y    += delta
            newRect.size.height -= delta
        case .left:
            newRect.origin.x    += delta
            newRect.size.width  -= delta
        case .right:
            newRect.size.width  += delta
        case .bottom:
            newRect.size.height += delta
        }
        
        if let maxRect = maxRect {
            newRect = newRect.intersection(maxRect)
        }
        
        return newRect
    }
}
