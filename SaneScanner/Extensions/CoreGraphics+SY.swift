//
//  CoreGraphics+SY.swift
//  SaneScanner
//
//  Created by Stanislas Chevallier on 07/02/2019.
//  Copyright © 2019 Syan. All rights reserved.
//

import CoreGraphics
import UIKit

enum CGRectSide {
    case top, left, right, bottom
    
    var isVertical: Bool {
        return self == .left || self == .right
    }
}

enum CGRectCorner {
    case topLeft, topRight, bottomLeft, bottomRight
    
    var horizontalSide: CGRectSide {
        return (self == .topLeft || self == .topRight) ? .top : .bottom
    }
    
    var verticalSide: CGRectSide {
        return (self == .topLeft || self == .bottomLeft) ? .left : .right
    }
}

extension CGRect {
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
