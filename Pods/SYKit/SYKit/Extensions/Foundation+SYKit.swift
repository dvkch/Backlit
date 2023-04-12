//
//  Foundation+SYKit.swift
//  SYKit
//
//  Created by Stanislas Chevallier on 12/04/2023.
//  Copyright © 2023 Syan. All rights reserved.
//

import Foundation

public extension Double {
    static func parse(from string: String) -> Double? {
        let formatter = NumberFormatter()
        formatter.locale = .current
        return formatter.number(from: string)?.doubleValue ?? Double(string)
    }
}

public extension Comparable {
    func clamped(min: Self, max: Self) -> Self {
        return Swift.min(max, Swift.max(min, self))
    }
}

public extension OperatingSystemVersion {
    var stringVersion: String {
        return "\(majorVersion).\(minorVersion).\(patchVersion)"
    }
}
