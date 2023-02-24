//
//  Sequence+SY.swift
//  SaneScanner
//
//  Created by syan on 24/02/2023.
//  Copyright © 2023 Syan. All rights reserved.
//

import Foundation

extension Sequence {
    func sorted<V: Comparable>(by path: KeyPath<Element, V>) -> [Self.Element] {
        return self.sorted { e1, e2 in
            return e1[keyPath: path] < e2[keyPath: path]
        }
    }
}
