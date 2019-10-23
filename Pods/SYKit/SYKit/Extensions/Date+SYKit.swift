//
//  Date+SYKit.swift
//  SYKit
//
//  Created by Stanislas Chevallier on 27/06/2019.
//  Copyright © 2019 Syan. All rights reserved.
//

import Foundation

public extension Date {
    static func -(lhs: Date, rhs: Date) -> TimeInterval {
        return lhs.timeIntervalSince(rhs)
    }
    
    static func +(lhs: Date, rhs: TimeInterval) -> Date {
        return Date(timeInterval: rhs, since: lhs)
    }
    
    static func -(lhs: Date, rhs: TimeInterval) -> Date {
        return Date(timeInterval: -rhs, since: lhs)
    }
}
