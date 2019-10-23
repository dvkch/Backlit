//
//  Array+SYKit.swift
//  SYKit
//
//  Created by Stanislas Chevallier on 26/06/2019.
//  Copyright © 2019 Syan. All rights reserved.
//

import Foundation

public extension Array where Element : Equatable {
    mutating func remove(_ element: Element) {
        while let index = self.firstIndex(of: element) {
            self.remove(at: index)
        }
    }

    func before(_ element: Element) -> Element? {
        guard let index = self.firstIndex(of: element) else { return nil }
        let newIndex = (index - 1 + count) % count
        return self[newIndex]
    }
    
    func after(_ element: Element) -> Element? {
        guard let index = self.firstIndex(of: element) else { return nil }
        let newIndex = (index + 1) % count
        return self[newIndex]
    }
}

public extension Collection {
    var isNotEmpty: Bool {
        return !isEmpty
    }
    
    var nilIfEmpty: Self? {
        return isEmpty ? nil : self
    }
}

public extension Sequence where Element : OptionalType {
    func removingNils() -> [Element.Wrapped] {
        return compactMap { $0.value }
    }
}

public extension Array {
    func element(at index: Index, `default`: Element? = nil) -> Element? {
        return index < count ? self[index] : `default`
    }
}

public extension Collection {
    func subarray(maxCount: Int) -> Self.SubSequence {
        let max = Swift.min(maxCount, count)
        let maxIndex = index(startIndex, offsetBy: max)
        return self[startIndex..<maxIndex]
    }
}
