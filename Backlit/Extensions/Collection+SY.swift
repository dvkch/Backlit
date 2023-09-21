//
//  Collection+SY.swift
//  Backlit
//
//  Created by syan on 21/09/2023.
//  Copyright © 2023 Syan. All rights reserved.
//

import Foundation

extension Array {
    var interleaved: [Element] {
        var elements = [Element]()
        elements.reserveCapacity(count)

        let middleIndex = (count + 1) / 2
        for i in 0..<middleIndex {
            elements.append(self[i])
            if middleIndex + i < count {
                elements.append(self[middleIndex + i])
            }
        }
        
        return elements
    }
}
