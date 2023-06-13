//
//  UIGestureRecognizer+SYKit.swift
//  SYKit
//
//  Created by Stanislas Chevallier on 27/06/2019.
//  Copyright © 2019 Syan. All rights reserved.
//

import Foundation

public extension UIGestureRecognizer {
    // https://stackoverflow.com/a/4167471/1439489
    @objc func cancelPendingTouches() {
        guard isEnabled else { return }
        isEnabled = false
        isEnabled = true
    }
}
