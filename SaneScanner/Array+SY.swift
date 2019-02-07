//
//  Array+SY.swift
//  SaneScanner
//
//  Created by Stanislas Chevallier on 07/02/2019.
//  Copyright © 2019 Syan. All rights reserved.
//

import UIKit

extension RandomAccessCollection where Index == Int {
    func object(at index: Index, or value: Element) -> Element {
        return index < count ? self[index] : value
    }
    
    func object(at index: Index, or value: Element?) -> Element? {
        return index < count ? self[index] : value
    }
}
