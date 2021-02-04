//
//  UISceneView+SY.swift
//  SaneScanner
//
//  Created by Stanislas Chevallier on 04/02/2021.
//  Copyright © 2021 Syan. All rights reserved.
//

import UIKit

// https://gist.github.com/stefanceriu/3c21452b7d481ceae41c0ca5f0f5c15d
extension NSObject {

    static func fixCatalystScaling() {
        #if targetEnvironment(macCatalyst)
        let clazz = objc_getClass("UINSSceneView") as! NSObject.Type
        clazz.sy_swizzleSelector(NSSelectorFromString("scaleFactor"), with: #selector(sy_catalystScaleFactor))
        #endif
    }
    
    @objc private func sy_catalystScaleFactor() -> CGFloat {
        return 1.0
    }
}
