//
//  UIScreen+SYKit.swift
//  SYKit
//
//  Created by Stanislas Chevallier on 27/06/2019.
//  Copyright © 2019 Syan. All rights reserved.
//

import UIKit

public extension UIScreen {
    
    @objc(sy_boundsFixedToPortraitOrientation)
    var boundsFixedToPortraitOrientation: CGRect {
        return coordinateSpace.convert(bounds, to: fixedCoordinateSpace)
    }
}
