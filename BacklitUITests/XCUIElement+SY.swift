//
//  XCUIElement+SY.swift
//  BacklitUITests
//
//  Created by syan on 06/05/2023.
//  Copyright © 2023 Syan. All rights reserved.
//

import XCTest

extension XCUIElement {
    func go(duration: TimeInterval = 0.2) {
        press(forDuration: duration)
    }
}
