//
//  UIGestureRecognizer+SY.swift
//  SaneScanner
//
//  Created by syan on 18/04/2023.
//  Copyright © 2023 Syan. All rights reserved.
//

import UIKit

extension UIGestureRecognizer {
    // https://stackoverflow.com/a/4167471/1439489
    func cancelPendingTouches() {
        guard isEnabled else { return }
        isEnabled = false
        isEnabled = true
    }
}
