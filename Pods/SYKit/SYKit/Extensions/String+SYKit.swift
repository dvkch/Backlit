//
//  String+SYKit.swift
//  SYKit
//
//  Created by Stanislas Chevallier on 27/06/2019.
//  Copyright © 2019 Syan. All rights reserved.
//

import Foundation

public extension StringProtocol {
    var localized: String {
        return NSLocalizedString(String(self), comment: String(self))
    }
    
    func localized(_ arguments: CVarArg...) -> String {
        let format = self.localized
        return String(format: format, arguments: arguments)
    }

    func localized(quantity: Int) -> String {
        let key: String
        switch quantity {
        case 0: key = self + ".zero"
        case 1: key = self + ".one"
        default: key = self + ".other"
        }
        
        return String(format: key.localized, quantity)
    }

    func firstLines(_ count: Int) -> String {
        var lines = components(separatedBy: .newlines)
        if lines.count > count {
            lines = Array(lines[0..<count])
        }
        return lines.joined(separator: "\n")
    }

    func stringBetween(_ startDelimiter: String, and endDelimiter: String, caseInsensitive: Bool = false, keepDelimiters: Bool = false) -> String? {
        let options: String.CompareOptions = caseInsensitive ? [.caseInsensitive] : []
        
        guard let startLocation = range(of: startDelimiter, options: options) else { return nil }
        guard let endLocation = range(of: endDelimiter, options: options, range: startLocation.upperBound..<endIndex) else { return nil}
        
        let indexStart = keepDelimiters ? startLocation.lowerBound : startLocation.upperBound
        let indexEnd   = keepDelimiters ? endLocation.upperBound   : endLocation.lowerBound
        
        return String(self[indexStart..<indexEnd])
    }
}

public extension String {
    var url: URL? {
        return URL(string: self)
    }
}
