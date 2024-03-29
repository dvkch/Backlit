//
//  NSObject+SYKit.swift
//  SYKit
//
//  Created by Stanislas Chevallier on 27/06/2019.
//  Copyright © 2019 Syan. All rights reserved.
//

import Foundation

public extension NSObject {
    @objc(sy_className)
    static var className: String {
        return String(describing: self)
    }
    
    @objc(sy_className)
    var className: String {
        return String(describing: type(of: self))
    }
    
    @objc(sy_pointerAddressString)
    var pointerAddressString: String {
        return Unmanaged.passUnretained(self).toOpaque().debugDescription
    }
}

