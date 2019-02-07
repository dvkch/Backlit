//
//  WeakValue.swift
//  SaneScanner
//
//  Created by Stanislas Chevallier on 06/02/2019.
//  Copyright © 2019 Syan. All rights reserved.
//

import UIKit

final class WeakValue<T: AnyObject> {
    weak var value: T?
    init(_ value: T) {
        self.value = value
    }
}
