//
//  Foundation+SY.swift
//  SaneScanner
//
//  Created by syan on 24/02/2023.
//  Copyright © 2023 Syan. All rights reserved.
//

import Foundation

extension Double {
    static func parse(from string: String) -> Double? {
        let formatter = NumberFormatter()
        formatter.locale = .current
        return formatter.number(from: string)?.doubleValue ?? Double(string)
    }
}
