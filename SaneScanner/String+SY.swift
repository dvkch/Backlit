//
//  String+SY.swift
//  SaneScanner
//
//  Created by Stanislas Chevallier on 02/02/2019.
//  Copyright © 2019 Syan. All rights reserved.
//

import UIKit

extension String {
    var localized: String {
        return NSLocalizedString(self, comment: self)
    }
}
