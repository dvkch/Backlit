//
//  Sequence+SY.swift
//  SaneScanner
//
//  Created by syan on 24/02/2023.
//  Copyright © 2023 Syan. All rights reserved.
//

import Foundation

extension Array where Element: RandomAccessCollection, Element.Index == Int {
    subscript(indexPath: IndexPath) -> Element.Element {
        get {
            return self[indexPath.section][indexPath.item]
        }
    }
    
    var lastIndexPath: IndexPath? {
        guard let lastNonEmptyElementIndex = lastIndex(where: { $0.isNotEmpty }) else { return nil }
        return IndexPath(item: self[lastNonEmptyElementIndex].count - 1, section: lastNonEmptyElementIndex)
    }
    
    var totalCount: Int {
        return map(\.count).reduce(0, +)
    }
    
    var allItems: [(IndexPath, Element.Element)] {
        var items: [(IndexPath, Element.Element)] = []
        items.reserveCapacity(totalCount)
        for (section, group) in enumerated() {
            for (index, item) in group.enumerated() {
                items.append((IndexPath(item: index, section: section), item))
            }
        }
        return items
    }
}
