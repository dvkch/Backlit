//
//  SYWindow.swift
//  SYKit
//
//  Created by Stanislas Chevallier on 27/06/2019.
//  Copyright © 2019 Syan. All rights reserved.
//

import UIKit

@objcMembers
public class SYWindow: UIWindow {
    
    public var alternateAnimationSpeed: Float = 0.05
    public var enableSlowAnimationsOnShake: Bool = false

    public func toggleAnimations() {
        if layer.speed == 1 {
            print("SYWindow: enabling slow animations")
            layer.speed = alternateAnimationSpeed
        }
        else {
            print("SYWindow: disabling slow animations")
            layer.speed = 1
        }
    }
    
    public override func motionEnded(_ motion: UIEvent.EventSubtype, with event: UIEvent?) {
        if motion == .motionShake && enableSlowAnimationsOnShake {
            toggleAnimations()
            return
        }
        super.motionEnded(motion, with: event)
    }
}
