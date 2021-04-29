//
//  TapInsetsView.swift
//  SaneScanner
//
//  Created by Stanislas Chevallier on 27/06/2019.
//  Copyright © 2019 Syan. All rights reserved.
//

import UIKit

class TapInsetsView: UIView {
    
    var tapInsets: UIEdgeInsets = .zero
    
    override func point(inside point: CGPoint, with event: UIEvent?) -> Bool {
        if tapInsets != .zero {
            if isHidden || alpha == 0 {
                return false
            }
            return bounds.inset(by: tapInsets).contains(point)
        }
        return super.point(inside: point, with: event)
    }
}
